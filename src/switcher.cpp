#include "switcher.h"
#include "fuzzy.h"
#include <algorithm>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

namespace {

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

/// Inverse of collapseHome: "~" and "~/..." become absolute. Anything else,
/// including a bare relative path, is returned as typed - refilterPath()
/// resolves those against the base directory.
QString expandHome(const QString &path)
{
    if (path == QStringLiteral("~"))
        return QDir::homePath();
    if (path.startsWith(QStringLiteral("~/")))
        return QDir::homePath() + path.mid(1);
    return path;
}

/// Resolve a typed path to an absolute one: ~ expanded, an empty text meaning
/// `base`, and a relative path anchored at `base` (nvim's cwd) rather than at
/// the process working directory. A trailing separator is preserved, since
/// refilterPath() reads it as "list this directory".
QString resolvePath(const QString &text, const QString &base)
{
    const QString path = expandHome(text.trimmed());
    if (path.isEmpty())
        return base;
    return QDir::isAbsolutePath(path) ? path : QDir(base).filePath(path);
}

/// Folder/File switcher item for an existing filesystem path. Kind follows
/// what is on disk, so MainWindow opens a directory as a project directory and
/// a file as a file.
SwitcherItem itemForPath(const QFileInfo &info)
{
    SwitcherItem item;
    item.kind = info.isDir() ? SwitcherItem::Folder : SwitcherItem::File;
    item.label = info.fileName();
    item.path = info.absoluteFilePath();
    return item;
}

/// Row marker. A project pinned from here is hollow, a project declared in
/// nvbeam.toml is filled: only the hollow ones can be unpinned with Ctrl+P.
QChar itemGlyph(const SwitcherItem &item)
{
    switch (item.kind) {
    case SwitcherItem::Project: return item.pinned ? QChar(u'◇') : QChar(u'◆');
    case SwitcherItem::Folder:  return QChar(u'▸');
    case SwitcherItem::File:    return QChar(u'·');
    }
    return QChar(u' ');
}

constexpr int kMaxVisibleRows = 12;
/// How long a pin message stays in the footer before the hints return.
constexpr int kStatusMs = 2500;
/// Rows built per directory in path mode. Only the ones that survive the
/// fragment match and the score sort are counted, so this caps widget work,
/// not the directory read itself.
constexpr int kMaxPathEntries = 200;

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
        "QListWidget::item:selected { background: #313244; color: %1; }"
        "QLabel#nbSwitcherStatus { color: #6c7086; font-size: 9pt; }")
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

    // Footer: the keys that are not discoverable from the input line, and
    // where a pin reports what it did.
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("nbSwitcherStatus"));
    layout->addWidget(m_status);

    // A pin message reverts to the key hints on its own; the switcher is
    // usually gone before it fires, which is why it is single-shot and not a
    // state the list has to carry.
    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    m_statusTimer->setInterval(kStatusMs);
    connect(m_statusTimer, &QTimer::timeout, this, [this]() { setStatus(); });

    connect(m_input, &QLineEdit::textChanged, this, &SwitcherWidget::refilter);

    parent->installEventFilter(this); // recenter on parent resizes
    hide();
}

void SwitcherWidget::open(const QList<SwitcherItem> &items, const QString &baseDir)
{
    m_items = items;
    m_baseDir = baseDir.isEmpty() ? QDir::homePath() : baseDir;
    // Always reopen in project mode, prefilled with nothing: setMode() does
    // the clear and the refilter, including the already-empty case that
    // textChanged would not cover.
    setMode(Mode::Projects);
    recenter();
    show();
    raise();
    m_input->setFocus();
}

void SwitcherWidget::setItems(const QList<SwitcherItem> &items)
{
    const int row = m_list->currentRow();
    m_items = items;
    // Project mode reads m_items directly; path mode's candidates come from
    // the filesystem, so only the pinned marks its rows carry need refreshing.
    refilter(m_input->text());
    if (row >= 0 && row < m_list->count())
        m_list->setCurrentRow(row);
}

void SwitcherWidget::dismiss()
{
    if (!isVisible())
        return;
    hide();
    emit dismissed();
}

void SwitcherWidget::setMode(Mode mode)
{
    m_mode = mode;
    const bool path = mode == Mode::Path;
    m_input->setPlaceholderText(path
        ? QStringLiteral("Path to open - Tab completes, Ctrl+D returns to projects")
        : QStringLiteral("Search projects and recent files… (Ctrl+D: open a path)"));
    // Path mode starts at the base directory with a trailing separator, so
    // its contents are listed right away. Blocked because refilter() is
    // called once below either way (setText is silent when nothing changes).
    {
        QSignalBlocker blocker(m_input);
        m_input->setText(path ? collapseHome(m_baseDir) + QLatin1Char('/') : QString());
    }
    refilter(m_input->text());
    setStatus();
}

void SwitcherWidget::showMessage(const QString &message)
{
    setStatus(message);
}

void SwitcherWidget::askSideCommand(const QString &dir, const QString &suggestion)
{
    m_pinDir = dir;
    m_pinReturnMode = m_mode;
    m_pinReturnText = m_input->text();
    m_mode = Mode::PinCommand;

    m_input->setPlaceholderText(QStringLiteral("Side command for %1 (empty = plain shell)")
                                    .arg(QFileInfo(dir).fileName()));
    {
        // Blocked: the text is a command, and refilter() would read it as a
        // query or a path. The list stays hidden for the same reason.
        QSignalBlocker blocker(m_input);
        m_input->setText(suggestion);
        m_input->selectAll();
    }
    m_list->hide();
    setStatus();
    adjustSize();
    recenter();
}

void SwitcherWidget::cancelPin()
{
    m_pinDir.clear();
    m_mode = m_pinReturnMode;
    {
        QSignalBlocker blocker(m_input);
        m_input->setText(m_pinReturnText);
    }
    // setMode() would reset the text; this restores the interrupted line and
    // rebuilds the list under it.
    m_input->setPlaceholderText(m_mode == Mode::Path
        ? QStringLiteral("Path to open - Tab completes, Ctrl+D returns to projects")
        : QStringLiteral("Search projects and recent files… (Ctrl+D: open a path)"));
    refilter(m_input->text());
    setStatus();
}

void SwitcherWidget::setStatus(const QString &message)
{
    if (!message.isEmpty()) {
        m_status->setText(message);
        m_statusTimer->start();
        return;
    }
    m_statusTimer->stop();
    switch (m_mode) {
    case Mode::PinCommand:
        m_status->setText(QStringLiteral("Enter pins with this command · Esc cancels the pin"));
        break;
    case Mode::Path:
        m_status->setText(
            QStringLiteral("Tab completes · Ctrl+P pins this folder · Ctrl+D returns to projects"));
        break;
    case Mode::Projects:
        m_status->setText(QStringLiteral("Ctrl+P pins the highlighted folder · Ctrl+D opens a path"));
        break;
    }
}

const QList<SwitcherItem> &SwitcherWidget::source() const
{
    return m_mode == Mode::Path ? m_pathItems : m_items;
}

void SwitcherWidget::refilter(const QString &text)
{
    if (m_mode == Mode::Path)
        refilterPath(text);
    else
        refilterProjects(text);
}

void SwitcherWidget::refilterProjects(const QString &query)
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

    QList<int> indices;
    indices.reserve(scored.size());
    for (const Scored &s : scored)
        indices.append(s.index);
    showRows(indices);
}

void SwitcherWidget::refilterPath(const QString &raw)
{
    // Split at the last separator: what precedes it names the directory to
    // list, what follows is the fragment to match. "~/Proj" lists the home
    // directory and matches "Proj"; "~/Projects/" lists that folder and
    // matches everything in it. Without a separator the text is a name
    // relative to the base directory.
    const QString text = expandHome(raw.trimmed());
    const int slash = text.lastIndexOf(QLatin1Char('/'));
    const QString dirText = slash < 0 ? QString() : text.left(slash + 1);
    const QString fragment = slash < 0 ? text : text.mid(slash + 1);
    const QDir dir(resolvePath(dirText, m_baseDir));

    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (fragment.startsWith(QLatin1Char('.')))
        filters |= QDir::Hidden;   // dotfiles only once the fragment asks for them
    const QFileInfoList entries =
        dir.entryInfoList(filters, QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);

    struct Scored {
        SwitcherItem item;
        int score;
    };
    QList<Scored> scored;
    scored.reserve(entries.size());
    for (const QFileInfo &info : entries) {
        const int score = fuzzyScore(fragment, info.fileName());
        if (score >= 0)
            scored.append({itemForPath(info), score});
    }
    // Stable, so an empty fragment (every score 0) keeps QDir's directories-
    // first, name-ordered listing.
    std::stable_sort(scored.begin(), scored.end(),
                     [](const Scored &a, const Scored &b) { return a.score > b.score; });

    m_pathItems.clear();
    QList<int> indices;
    for (const Scored &s : scored) {
        if (m_pathItems.size() >= kMaxPathEntries)
            break;
        indices.append(m_pathItems.size());
        m_pathItems.append(s.item);
    }
    showRows(indices);
}

void SwitcherWidget::fillFromRow(int row)
{
    if (m_mode != Mode::Path || row < 0 || row >= m_visible.size())
        return;
    const SwitcherItem &item = m_pathItems.at(m_visible.at(row));
    // A directory gets the trailing separator that makes refilterPath list its
    // contents once the completion is confirmed, which is what turns Enter on
    // a folder into descending the tree.
    QString text = collapseHome(item.path);
    if (item.kind == SwitcherItem::Folder)
        text += QLatin1Char('/');
    // Blocked: refiltering here would rebuild the candidate list from the text
    // just written, so the next Tab would cycle a different set (for a folder,
    // its children) instead of the alternatives to this one.
    {
        QSignalBlocker blocker(m_input);
        m_input->setText(text);
    }
    m_list->setCurrentRow(row);
    m_completionPending = true;
}

void SwitcherWidget::cycleCompletion()
{
    if (m_mode != Mode::Path || m_list->count() == 0)
        return;
    // First Tab takes the highlighted candidate as it is; each Tab after that
    // steps to the next one, wrapping, so Tab alone walks the whole list.
    const int row = m_completionPending
        ? (m_list->currentRow() + 1) % m_list->count()
        : qMax(0, m_list->currentRow());
    fillFromRow(row);
}

void SwitcherWidget::completePath()
{
    fillFromRow(m_list->currentRow());
    if (m_completionPending) {
        m_completionPending = false;
        refilter(m_input->text());   // confirm in the same keystroke
    }
}

void SwitcherWidget::showRows(const QList<int> &indices)
{
    const QList<SwitcherItem> &src = source();
    m_visible = indices;
    m_selectionMoved = false;
    m_completionPending = false;   // the candidate set is being replaced
    m_list->clear();
    for (int index : indices) {
        const SwitcherItem &item = src.at(index);
        auto *row = new QListWidgetItem(QStringLiteral("%1 %2   %3")
                                            .arg(itemGlyph(item))
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
    m_selectionMoved = true;
}

void SwitcherWidget::selectCurrent()
{
    const int row = m_list->currentRow();
    // Enter after Tab confirms the completion rather than opening it: the
    // filter runs on the completed text, so a folder's contents come up and
    // the next Enter opens whatever the confirmed path names.
    if (m_completionPending) {
        m_completionPending = false;
        refilter(m_input->text());
        return;
    }
    // Path mode, selection still on the row the filter picked: an exactly
    // typed path wins over the highlighted entry, so "~/Projects/" + Enter
    // opens that folder instead of its first child. Once the user has moved
    // the selection, the highlighted entry is what they mean.
    if (m_mode == Mode::Path && !m_selectionMoved) {
        const QFileInfo info(resolvePath(m_input->text(), m_baseDir));
        if (info.exists()) {
            hide();
            emit itemSelected(itemForPath(info));
            return;
        }
    }
    if (row < 0 || row >= m_visible.size())
        return;
    const SwitcherItem item = source().at(m_visible.at(row));
    hide();
    emit itemSelected(item);
}

void SwitcherWidget::togglePin()
{
    // Path mode pins what is typed, even when the list below it highlights a
    // child entry: the input line is what the user is looking at. Project
    // mode pins the highlighted row.
    QString dir;
    bool pinned = false;
    if (m_mode == Mode::Path) {
        dir = QDir::cleanPath(resolvePath(m_input->text(), m_baseDir));
        for (const SwitcherItem &item : m_items) {
            if (item.path == dir) {
                pinned = item.pinned;
                // A folder already declared in nvbeam.toml is a project the
                // app must not touch, and saying so beats a silent no-op.
                if (item.kind == SwitcherItem::Project && !item.pinned) {
                    setStatus(QStringLiteral("Already a project in nvbeam.toml"));
                    return;
                }
                break;
            }
        }
    } else {
        const int row = m_list->currentRow();
        if (row < 0 || row >= m_visible.size())
            return;
        const SwitcherItem &item = source().at(m_visible.at(row));
        if (item.kind == SwitcherItem::File) {
            setStatus(QStringLiteral("Only folders can be pinned"));
            return;
        }
        if (item.kind == SwitcherItem::Project && !item.pinned) {
            setStatus(QStringLiteral("Declared in nvbeam.toml, remove it there"));
            return;
        }
        dir = item.path;
        pinned = item.pinned;
    }

    if (dir.isEmpty())
        return;
    if (!QFileInfo(dir).isDir()) {
        setStatus(QStringLiteral("Only folders can be pinned"));
        return;
    }

    if (pinned)
        emit unpinRequested(dir);
    else
        emit pinRequested(dir);
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
        // The side command prompt is a plain edit line: only Enter and Esc
        // mean anything, everything else is ordinary text editing.
        if (m_mode == Mode::PinCommand) {
            switch (ke->key()) {
            case Qt::Key_Escape:
                cancelPin();
                setStatus(QStringLiteral("Pin cancelled"));
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter: {
                const QString dir = m_pinDir;
                const QString command = m_input->text().trimmed();
                cancelPin();
                emit pinConfirmed(dir, command);
                return true;
            }
            default:
                return QFrame::eventFilter(watched, event);
            }
        }

        switch (ke->key()) {
        case Qt::Key_Escape:
            dismiss();
            return true;
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            // Path mode: the completion key. Project mode: nothing to
            // complete, and swallowed anyway so Tab never moves focus out of
            // the input (focus loss dismisses the switcher).
            cycleCompletion();
            return true;
        case Qt::Key_P:
            // Ctrl+P pins (or unpins) the folder the switcher points at.
            if (ke->modifiers().testFlag(Qt::ControlModifier)) {
                togglePin();
                return true;
            }
            break;
        case Qt::Key_D:
            // Ctrl+D toggles the two modes. It shadows QLineEdit's X11
            // delete-forward binding, which Del already covers.
            if (ke->modifiers().testFlag(Qt::ControlModifier)) {
                setMode(m_mode == Mode::Path ? Mode::Projects : Mode::Path);
                return true;
            }
            break;
        case Qt::Key_Right:
            // Fill and confirm at once, and only at the end of the line, so
            // Right keeps working as cursor movement while editing mid-string.
            if (m_mode == Mode::Path
                && m_input->cursorPosition() == m_input->text().size()
                && !m_input->hasSelectedText()) {
                completePath();
                return true;
            }
            break;
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
