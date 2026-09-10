#include "branchswitcher.h"
#include "fuzzy.h"
#include <algorithm>
#include <QEvent>
#include <QKeyEvent>
#include <QColor>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {

/// Row glyphs, matching the project switcher's vocabulary: a filled marker for
/// the branch that is checked out, an outline for the other local ones, a dot
/// for remote-only branches (whose "<remote>/" prefix already names them).
QChar branchGlyph(const GitBranch &branch)
{
    if (branch.current)
        return QChar(u'◆');
    return branch.remote ? QChar(u'·') : QChar(u'▸');
}

constexpr int kMaxVisibleRows = 12;
constexpr int kMinWidth = 320;
constexpr int kMaxWidth = 560;
/// Gap between the popup and the segment it hangs under.
constexpr int kAnchorGap = 4;
/// A click on the git segment closes an open popup by taking focus away from
/// its input, which happens before the click reaches toggle(). Within this
/// window a close counts as the whole toggle, so the click does not reopen
/// what it just closed.
constexpr int kReopenGuardMs = 250;
/// Longest git message shown verbatim; a refusal listing every conflicting
/// file would otherwise grow the popup past the window.
constexpr int kMaxStatusChars = 400;

/// Opacity of the popup's border and of the selected row's background, both
/// derived from the highlight colour: enough to read as tinted, not enough to
/// compete with the row text.
constexpr int kBorderAlpha = 110;
constexpr int kSelectionAlpha = 56;

/// `color` as a Qt style sheet rgba() literal at the given alpha. Named
/// colours and #rrggbb both work, and an unparseable one falls through as
/// written rather than turning the rule into a syntax error.
QString rgbaOf(const QString &color, int alpha)
{
    const QColor parsed(color);
    if (!parsed.isValid())
        return color;
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(parsed.red()).arg(parsed.green()).arg(parsed.blue()).arg(alpha);
}

} // namespace

BranchSwitcherWidget::BranchSwitcherWidget(const QString &highlightColor, QWidget *parent)
    : QFrame(parent)
{
    setObjectName("nbBranchSwitcher");
    // Same shape as the project switcher, tinted with the highlight colour
    // instead of the green accent so the two are not mistaken for each other.
    // The border and the selected row derive from that one colour, at the
    // alphas below, so configuring it recolours the whole popup coherently.
    const QString border = rgbaOf(highlightColor, kBorderAlpha);
    const QString selection = rgbaOf(highlightColor, kSelectionAlpha);
    setStyleSheet(QStringLiteral(
        "QFrame#nbBranchSwitcher { background: #1a1b26; border: 1px solid %2; border-radius: 6px; }"
        "QLineEdit { background: #24283b; color: #cdd6f4; border: 1px solid #333; "
        "border-radius: 4px; padding: 5px 8px; font-size: 11pt; }"
        "QLineEdit:focus { border-color: %1; }"
        "QListWidget { background: transparent; color: #a9b1d6; border: none; font-size: 10pt; }"
        "QListWidget::item { padding: 4px 8px; border-radius: 4px; }"
        "QListWidget::item:selected { background: %3; color: %1; }")
            .arg(highlightColor, border, selection));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QStringLiteral("Switch branch or type a new name"));
    m_input->installEventFilter(this);
    layout->addWidget(m_input);

    m_list = new QListWidget(this);
    // Focus stays in the line edit; clicks on rows still activate them, and
    // they must not steal focus (focus loss dismisses the popup).
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setUniformItemSizes(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *) {
        activateCurrent();
    });
    layout->addWidget(m_list);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setContentsMargins(2, 0, 2, 0);
    m_status->hide();
    layout->addWidget(m_status);

    connect(m_input, &QLineEdit::textChanged, this, &BranchSwitcherWidget::refilter);

    parent->installEventFilter(this); // reposition on parent resizes
    hide();
}

void BranchSwitcherWidget::open(QWidget *anchor)
{
    m_anchor = anchor;
    m_branches.clear();
    m_busy = false;
    m_input->setReadOnly(false);
    {
        // Blocked: refilter() runs once below through showRows(), and on an
        // already-empty field textChanged would not fire at all.
        QSignalBlocker blocker(m_input);
        m_input->clear();
    }
    showRows({});
    setStatus(QStringLiteral("Loading branches…"));
    show();
    raise();
    m_input->setFocus();
    // Asked for on every open rather than cached: branches come and go through
    // the embedded terminal, and a fetch changes the remote ones.
    emit branchesRequested();
}

void BranchSwitcherWidget::toggle(QWidget *anchor)
{
    if (isVisible()) {
        dismiss();
        return;
    }
    if (m_dismissed.isValid() && m_dismissed.elapsed() < kReopenGuardMs)
        return;   // this call is the tail of the click that just closed it
    open(anchor);
}

void BranchSwitcherWidget::dismiss()
{
    if (!isVisible())
        return;
    hide();
    m_dismissed.start();
    emit dismissed();
}

void BranchSwitcherWidget::setBranches(const QList<GitBranch> &branches, const QString &error)
{
    if (!isVisible())
        return;
    m_branches = branches;
    refilter(m_input->text());
    if (!error.isEmpty())
        setStatus(error, true);
    else if (branches.isEmpty())
        // Unborn HEAD (`git init` with nothing committed): no ref exists to
        // list yet. Creating one still works - `git checkout -b` just moves
        // the unborn HEAD - so the create row stays useful here.
        setStatus(QStringLiteral("No branches yet - type a name to create one"));
    else
        setStatus(QString());
}

void BranchSwitcherWidget::checkoutFinished(bool ok, const QString &branch, const QString &error)
{
    if (!isVisible())
        return;
    m_busy = false;
    m_input->setReadOnly(false);
    if (ok) {
        dismiss();
        return;
    }
    setStatus(error.isEmpty()
                  ? QStringLiteral("Could not switch to %1").arg(branch)
                  : error.left(kMaxStatusChars),
              true);
    reposition();
}

bool BranchSwitcherWidget::canCreate(const QString &name) const
{
    if (name.isEmpty() || name.contains(QLatin1Char(' ')) || name.contains(QLatin1Char('\t')))
        return false;
    // Branch names are case-sensitive, so the comparison is too. Both fields
    // are checked: typing "feature-x" must not offer to create a branch that
    // already exists as the remote-only "origin/feature-x".
    for (const GitBranch &branch : m_branches) {
        if (branch.name == name || branch.target == name)
            return false;
    }
    return true;
}

void BranchSwitcherWidget::refilter(const QString &text)
{
    const QString query = text.trimmed();

    struct Scored {
        int index;
        int score;
    };
    QList<Scored> scored;
    scored.reserve(m_branches.size());
    for (int i = 0; i < m_branches.size(); ++i) {
        const int score = fuzzyScore(query, m_branches.at(i).name);
        if (score >= 0)
            scored.append({i, score});
    }
    // Stable, so an empty query keeps git's own order: locals by commit date,
    // then the remote-only branches.
    if (!query.isEmpty()) {
        std::stable_sort(scored.begin(), scored.end(),
                         [](const Scored &a, const Scored &b) { return a.score > b.score; });
    }

    QList<int> indices;
    indices.reserve(scored.size() + 1);
    for (const Scored &s : scored)
        indices.append(s.index);
    // The create row goes last: an existing branch matching what was typed is
    // the far more common intent, so Enter must not create a branch by
    // surprise. It is selected by default only when nothing else matched.
    if (canCreate(query))
        indices.append(kCreateRow);
    showRows(indices);
}

void BranchSwitcherWidget::showRows(const QList<int> &indices)
{
    m_visible = indices;
    m_list->clear();
    for (int index : indices) {
        if (index == kCreateRow) {
            m_list->addItem(QStringLiteral("+ Create branch \"%1\"").arg(m_input->text().trimmed()));
            continue;
        }
        const GitBranch &branch = m_branches.at(index);
        auto *row = new QListWidgetItem(QStringLiteral("%1 %2   %3")
                                            .arg(branchGlyph(branch))
                                            .arg(branch.name, branch.detail));
        row->setToolTip(branch.remote
            ? QStringLiteral("%1 - checks out %2, tracking the remote branch")
                  .arg(branch.name, branch.target)
            : branch.name);
        m_list->addItem(row);
    }

    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
        const int rowHeight = m_list->sizeHintForRow(0);
        const int rows = qMin(m_list->count(), kMaxVisibleRows);
        m_list->setFixedHeight(rows * rowHeight + 2 * m_list->frameWidth());
        m_list->show();
    } else {
        m_list->hide();
    }
    // The popup height follows the list; keep it against its anchor as it
    // changes.
    adjustSize();
    reposition();
}

void BranchSwitcherWidget::setStatus(const QString &text, bool error)
{
    m_status->setText(text);
    m_status->setStyleSheet(QStringLiteral("color: %1; font-size: 9pt;")
                                .arg(error ? QStringLiteral("#f7768e") : QStringLiteral("#565f89")));
    m_status->setVisible(!text.isEmpty());
    adjustSize();
    reposition();
}

void BranchSwitcherWidget::moveSelection(int delta)
{
    if (m_list->count() == 0)
        return;
    m_list->setCurrentRow(qBound(0, m_list->currentRow() + delta, m_list->count() - 1));
}

void BranchSwitcherWidget::activateCurrent()
{
    const int row = m_list->currentRow();
    if (m_busy || row < 0 || row >= m_visible.size())
        return;
    const int index = m_visible.at(row);
    const bool create = index == kCreateRow;
    const QString target = create ? m_input->text().trimmed() : m_branches.at(index).target;
    if (target.isEmpty())
        return;
    // Already on it: nothing to run, just close.
    if (!create && m_branches.at(index).current) {
        dismiss();
        return;
    }
    // Frozen rather than disabled: disabling the line edit would move focus
    // out of it, and focus loss dismisses the popup before the answer lands.
    m_busy = true;
    m_input->setReadOnly(true);
    setStatus(create ? QStringLiteral("Creating %1…").arg(target)
                     : QStringLiteral("Switching to %1…").arg(target));
    emit checkoutRequested(target, create);
}

void BranchSwitcherWidget::reposition()
{
    QWidget *p = parentWidget();
    if (!p)
        return;
    setFixedWidth(qBound(kMinWidth, p->width() * 40 / 100, kMaxWidth));
    adjustSize();

    QPoint topLeft;
    if (m_anchor && m_anchor->isVisible()) {
        // Hangs under the statusline's git segment, VSCode-style, and flips
        // above it when the window has no room below - which is the usual
        // case, the statusline sitting at the bottom of the editor column.
        const QPoint anchorTopLeft = m_anchor->mapTo(p, QPoint(0, 0));
        topLeft = QPoint(anchorTopLeft.x(), anchorTopLeft.y() + m_anchor->height() + kAnchorGap);
        if (topLeft.y() + height() > p->height())
            topLeft.setY(anchorTopLeft.y() - height() - kAnchorGap);
    } else {
        // No git segment on screen (not a repository): bottom-left corner,
        // where the segment would have been.
        topLeft = QPoint(kAnchorGap, p->height() - height() - kAnchorGap);
    }
    move(qBound(0, topLeft.x(), qMax(0, p->width() - width())),
         qBound(0, topLeft.y(), qMax(0, p->height() - height())));
}

bool BranchSwitcherWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
        reposition();
        return false;
    }
    if (watched != m_input)
        return QFrame::eventFilter(watched, event);

    if (event->type() == QEvent::FocusOut) {
        dismiss();
        return false;
    }
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        switch (ke->key()) {
        case Qt::Key_Escape:
            dismiss();
            return true;
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            // Swallowed so Tab never moves focus out of the input, which would
            // dismiss the popup. There is nothing to complete here.
            return true;
        case Qt::Key_Up:
            moveSelection(-1);
            return true;
        case Qt::Key_Down:
            moveSelection(1);
            return true;
        case Qt::Key_PageUp:
            moveSelection(-kMaxVisibleRows);
            return true;
        case Qt::Key_PageDown:
            moveSelection(kMaxVisibleRows);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            activateCurrent();
            return true;
        default:
            break;
        }
    }
    return QFrame::eventFilter(watched, event);
}
