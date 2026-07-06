#include "switcher.h"
#include <algorithm>
#include <QDir>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace {

/// Fuzzy subsequence score: every query char must appear in order in the
/// candidate (case-insensitive), or -1. Consecutive matches and matches on
/// word boundaries (start, after separators) score higher, approximating
/// fzf-style ranking without its full algorithm.
int fuzzyScore(const QString &query, const QString &candidate)
{
    if (query.isEmpty())
        return 0;
    int score = 0;
    int qi = 0;
    bool prevMatched = false;
    for (int ci = 0; ci < candidate.size() && qi < query.size(); ++ci) {
        const QChar c = candidate.at(ci).toLower();
        if (c != query.at(qi).toLower()) {
            prevMatched = false;
            continue;
        }
        score += 2;
        if (prevMatched)
            score += 3;
        const QChar prev = ci > 0 ? candidate.at(ci - 1) : QChar();
        if (ci == 0 || prev == QLatin1Char('/') || prev == QLatin1Char('-')
            || prev == QLatin1Char('_') || prev == QLatin1Char('.') || prev == QLatin1Char(' '))
            score += 5;
        prevMatched = true;
        ++qi;
    }
    return qi == query.size() ? score : -1;
}

/// Best score for an item: its label counts double (matching the project or
/// file name should beat matching somewhere in a long path).
int itemScore(const QString &query, const SwitcherItem &item)
{
    const int labelScore = fuzzyScore(query, item.label);
    const int pathScore = fuzzyScore(query, item.path);
    return qMax(labelScore >= 0 ? labelScore * 2 : -1, pathScore);
}

QString collapseHome(const QString &path)
{
    const QString home = QDir::homePath();
    if (path == home)
        return QStringLiteral("~");
    if (path.startsWith(home + QLatin1Char('/')))
        return QStringLiteral("~") + path.mid(home.size());
    return path;
}

QChar kindGlyph(SwitcherItem::Kind kind)
{
    switch (kind) {
    case SwitcherItem::Project: return QChar(u'◆');
    case SwitcherItem::Folder:  return QChar(u'▸');
    case SwitcherItem::File:    return QChar(u'·');
    }
    return QChar(u' ');
}

constexpr int kMaxVisibleRows = 12;

} // namespace

SwitcherWidget::SwitcherWidget(const QString &accentColor, QWidget *parent)
    : QFrame(parent)
{
    setObjectName("nbSwitcher");
    setStyleSheet(QStringLiteral(
        "QFrame#nbSwitcher { background: #1a1b26; border: 1px solid #444; border-radius: 6px; }"
        "QLineEdit { background: #24283b; color: #cdd6f4; border: 1px solid #333; "
        "border-radius: 4px; padding: 6px 8px; font-size: 12pt; }"
        "QLineEdit:focus { border-color: %1; }"
        "QListWidget { background: transparent; color: #a9b1d6; border: none; font-size: 10pt; }"
        "QListWidget::item { padding: 5px 8px; border-radius: 4px; }"
        "QListWidget::item:selected { background: #313244; color: %1; }")
            .arg(accentColor));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QStringLiteral("Search projects and recent files…"));
    m_input->installEventFilter(this);
    layout->addWidget(m_input);

    m_list = new QListWidget(this);
    // Focus stays in the line edit; clicks on items still select and activate,
    // and they must not steal focus (focus loss dismisses the switcher).
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setUniformItemSizes(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *) {
        selectCurrent();
    });
    layout->addWidget(m_list);

    connect(m_input, &QLineEdit::textChanged, this, &SwitcherWidget::refilter);

    parent->installEventFilter(this); // recenter on parent resizes
    hide();
}

void SwitcherWidget::open(const QList<SwitcherItem> &items)
{
    m_items = items;
    // Clearing triggers refilter via textChanged only if there was text;
    // refilter explicitly to cover the already-empty case.
    m_input->clear();
    refilter(QString());
    recenter();
    show();
    raise();
    m_input->setFocus();
}

void SwitcherWidget::dismiss()
{
    if (!isVisible())
        return;
    hide();
    emit dismissed();
}

void SwitcherWidget::refilter(const QString &query)
{
    struct Scored {
        int index;
        int score;
    };
    QList<Scored> scored;
    for (int i = 0; i < m_items.size(); ++i) {
        int score = itemScore(query, m_items.at(i));
        if (score < 0)
            continue;
        // Projects float above equally scored recents.
        if (m_items.at(i).kind == SwitcherItem::Project)
            score += 1;
        scored.append({i, score});
    }
    if (!query.isEmpty()) {
        std::stable_sort(scored.begin(), scored.end(),
                         [](const Scored &a, const Scored &b) { return a.score > b.score; });
    }

    m_visible.clear();
    m_list->clear();
    for (const Scored &s : scored) {
        m_visible.append(s.index);
        const SwitcherItem &item = m_items.at(s.index);
        auto *row = new QListWidgetItem(QStringLiteral("%1 %2   %3")
                                            .arg(kindGlyph(item.kind))
                                            .arg(item.label, collapseHome(item.path)));
        row->setToolTip(item.path);
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
    // The box height follows the list height; keep it centered as it changes.
    adjustSize();
    recenter();
}

void SwitcherWidget::moveSelection(int delta)
{
    if (m_list->count() == 0)
        return;
    const int row = qBound(0, m_list->currentRow() + delta, m_list->count() - 1);
    m_list->setCurrentRow(row);
}

void SwitcherWidget::selectCurrent()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_visible.size())
        return;
    const SwitcherItem item = m_items.at(m_visible.at(row));
    hide();
    emit itemSelected(item);
}

void SwitcherWidget::recenter()
{
    QWidget *p = parentWidget();
    if (!p)
        return;
    const int w = qMin(640, qMax(360, p->width() * 55 / 100));
    setFixedWidth(w);
    adjustSize();
    // Slightly above center, KRunner-style.
    move((p->width() - width()) / 2, qMax(0, p->height() / 5));
}

bool SwitcherWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
        recenter();
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
            selectCurrent();
            return true;
        default:
            break;
        }
    }
    return QFrame::eventFilter(watched, event);
}
