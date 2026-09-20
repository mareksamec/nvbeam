#include "terminalpanel.h"

#include "colorscheme.h"

#include <QShowEvent>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QDir>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QFile>
#include <QMenu>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QDebug>

#ifdef NEOBEAM_HAS_TERMINAL
namespace {

/// Make neobeam-shipped schemes (Neoblue, the default, plus OneDark and
/// Dracula) resolvable by name in setColorScheme(). Called once; QTermWidget
/// keeps the dirs in a process-wide list.
void registerNeobeamSchemeDirs()
{
    static bool registered = false;
    if (registered)
        return;
    registered = true;
    for (const QString &dir : neobeamSchemeDirs()) {
        if (QDir(dir).exists())
            QTermWidget::addCustomColorSchemeDir(dir);
    }
}

// --- Link detection ---------------------------------------------------------
//
// qtermwidget detects links itself (UrlFilter, underlined on hover, opened on
// click) but only ones written with an explicit scheme and laid out on a single
// terminal line. The helpers below cover what its filter misses and cannot be
// taught, since the filter chain is not reachable through the public API:
// scheme-less links and links a TUI application wrapped across lines itself.

/// Characters a URL may be built from (RFC 3986 unreserved + reserved + the
/// percent sign of escapes). ASCII-only on purpose: the spaces, quotes and
/// box-drawing characters TUI applications frame their output with therefore
/// end a match instead of becoming part of the link.
const QString kUrlBodyChars = QStringLiteral("A-Za-z0-9\\-._~:/?#\\[\\]@!$&'()*+,;=%");

/// Single pass, leftmost match wins: scheme URL, www host, host with a path,
/// bare e-mail address. Hosts without a "www." prefix need a path so that
/// ordinary dotted words (vim.fn.getcwd, main.cpp) are not taken for links.
const QRegularExpression &linkPattern()
{
    static const QRegularExpression re(
        QStringLiteral("[A-Za-z][A-Za-z0-9+.\\-]*://[%1]+"
                       "|www\\.[A-Za-z0-9\\-]+(?:\\.[A-Za-z0-9\\-]+)+[%1]*"
                       "|[A-Za-z0-9\\-]+(?:\\.[A-Za-z0-9\\-]+)+/[%1]*"
                       "|[A-Za-z0-9._%+\\-]+@[A-Za-z0-9\\-]+(?:\\.[A-Za-z0-9\\-]+)+")
            .arg(kUrlBodyChars));
    return re;
}

/// Turn matched text into something openable: trailing sentence punctuation is
/// dropped, a scheme-less host gets https:// and a bare address mailto:.
/// Returns an invalid QUrl when nothing usable is left.
QUrl normalizeLink(QString text)
{
    static const QString kTrailing = QStringLiteral(".,;:!?'\"");
    while (!text.isEmpty() && kTrailing.contains(text.back()))
        text.chop(1);
    // A closing bracket belongs to the link only if it has an opener inside it
    // (wiki-style paths like /Foo_(bar) are common enough to matter).
    while (text.size() > 1) {
        const QChar last = text.back();
        const QChar opener = last == QLatin1Char(')') ? QLatin1Char('(')
                           : last == QLatin1Char(']') ? QLatin1Char('[')
                                                      : QChar();
        if (opener.isNull() || text.count(opener) >= text.count(last))
            break;
        text.chop(1);
    }
    if (text.isEmpty())
        return {};

    QUrl url(text, QUrl::StrictMode);
    if (url.scheme().isEmpty()) {
        const bool address = text.contains(QLatin1Char('@')) && !text.contains(QLatin1Char('/'));
        url = QUrl((address ? QStringLiteral("mailto:") : QStringLiteral("https://")) + text,
                   QUrl::StrictMode);
    }
    return url.isValid() ? url : QUrl();
}

/// First link in text, or an invalid QUrl when it holds none.
QUrl firstLinkIn(const QString &text)
{
    const QRegularExpressionMatch match = linkPattern().match(text);
    return match.hasMatch() ? normalizeLink(match.captured(0)) : QUrl();
}

/// Resolve the link in a terminal selection.
///
/// A selection whose lines hold nothing but URL characters - once the padding
/// and frame characters a TUI application surrounds its lines with are stripped
/// off both ends - is read as one link the application wrapped itself and joined
/// back together ("https://exa" + "│ mple.com/x"). Any line with an internal
/// separator means the selection is prose containing a link, which is then
/// searched line by line so nothing gets glued to the link.
QUrl linkInSelection(const QString &selection)
{
    static const QRegularExpression frame(
        QStringLiteral("^[^%1]+|[^%1]+$").arg(kUrlBodyChars));
    static const QRegularExpression separator(QStringLiteral("[^%1]").arg(kUrlBodyChars));

    const QStringList lines = selection.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QStringList fragments;
    bool wrapped = true;
    for (const QString &line : lines) {
        const QString inner = QString(line).remove(frame);
        if (inner.isEmpty())
            continue;
        if (inner.contains(separator))
            wrapped = false;
        fragments << inner;
    }

    if (wrapped && !fragments.isEmpty()) {
        const QUrl joined = firstLinkIn(fragments.join(QString()));
        if (joined.isValid())
            return joined;
    }
    for (const QString &line : lines) {
        const QUrl url = firstLinkIn(line);
        if (url.isValid())
            return url;
    }
    return {};
}

} // namespace
#endif

TerminalPanel::TerminalPanel(const TerminalConfig &cfg, QWidget *parent)
    : QWidget(parent)
    , m_cfg(cfg)
    , m_cwd(QDir::homePath())
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // The panel chrome (toolbar, splitter, this widget) matches the terminal
    // color scheme's background so that any area the terminal display does
    // not cover (margins, resize gaps) blends in instead of showing through
    // as black bands under non-black schemes.
    const QString bg = panelBackgroundColor().name();

    // Toolbar with terminal actions
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet(QStringLiteral(
        "QToolBar { background: %1; border: none; spacing: 4px; }"
        "QToolButton { color: #aaa; background: transparent; border: none; padding: 2px 6px; }"
        "QToolButton:hover { background: #333; border-radius: 3px; }").arg(bg));

    auto *addAct = m_toolbar->addAction("+", this, &TerminalPanel::addTerminal);
    addAct->setToolTip("New terminal");
    auto *cwdAct = m_toolbar->addAction("↓");
    cwdAct->setToolTip("Send nvim cwd to all terminals");
    connect(cwdAct, &QAction::triggered, this, &TerminalPanel::cwdBroadcastRequested);
    auto *closeAct = m_toolbar->addAction("✕", this, &TerminalPanel::removeCurrentTerminal);
    closeAct->setToolTip("Close terminal");
    // Side panel only: the change review panel opens directly next to this
    // one, showing what the agent running here has written, so its toggle
    // belongs on this toolbar and not on the bottom panel's.
    if (m_cfg.sidePanel) {
        auto *reviewAct = m_toolbar->addAction("◧");
        reviewAct->setToolTip("Review changes (Ctrl+Shift+G)");
        connect(reviewAct, &QAction::triggered, this, &TerminalPanel::changeReviewRequested);
    }

    layout->addWidget(m_toolbar);

    // Splitter holds terminal instances side by side (bottom panel) or
    // stacked vertically (side panel, where horizontal space is scarce).
    m_splitter = new QSplitter(m_cfg.sidePanel ? Qt::Vertical : Qt::Horizontal, this);
    m_splitter->setStyleSheet(QStringLiteral(
        "QSplitter { background: %1; }"
        "QSplitter::handle { background: #333; width: 2px; height: 2px; }").arg(bg));
    layout->addWidget(m_splitter, 1);

    setStyleSheet(QStringLiteral("background: %1; border-%2: 1px solid #333;")
                      .arg(bg, m_cfg.sidePanel ? QStringLiteral("left") : QStringLiteral("top")));
    // Resizable: enforce a sensible minimum rather than a fixed size so the
    // user can drag the surrounding splitter. The initial size is applied by
    // MainWindow via the splitter's sizes.
    if (m_cfg.sidePanel)
        setMinimumWidth(kMinWidth);
    else
        setMinimumHeight(kMinHeight);
    hide();

    // Create initial terminal
    addTerminal();
}

QColor TerminalPanel::panelBackgroundColor() const
{
#ifdef NEOBEAM_HAS_TERMINAL
    // Same precedence as effectiveColorScheme(): an explicit background
    // override wins over the scheme's own background.
    const QColor configured(m_cfg.background);
    if (configured.isValid())
        return configured;
    const QColor schemeBg = loadTerminalScheme(m_cfg.colorScheme).background;
    if (schemeBg.isValid())
        return schemeBg;
#endif
    return QColor(Qt::black);
}

void TerminalPanel::toggle()
{
    setVisible(!isVisible());
    emit visibilityChanged(isVisible());
}

int TerminalPanel::splitCount() const
{
#ifdef NEOBEAM_HAS_TERMINAL
    return m_terminals.size();
#else
    return m_splitter ? m_splitter->count() : 0;
#endif
}

QByteArray TerminalPanel::splitterState() const
{
    return m_splitter ? m_splitter->saveState() : QByteArray();
}

void TerminalPanel::restoreSplits(int count, const QByteArray &splitterState)
{
    // The constructor already created one terminal, so add the remainder.
    // Clamp to a sane upper bound to avoid pathological restores.
    constexpr int kMaxSplits = 16;
    const int target = qBound(1, count, kMaxSplits);
    for (int i = splitCount(); i < target; ++i)
        addTerminal();

    // Reapply proportions after the widgets exist. QSplitter::restoreState only
    // takes effect once the handle count matches the saved state.
    if (!splitterState.isEmpty() && m_splitter)
        m_splitter->restoreState(splitterState);
}

void TerminalPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef NEOBEAM_HAS_TERMINAL
    // All shells may have exited while the panel was hidden (their finished
    // handlers remove the splits); reopening should present a fresh terminal
    // rather than an empty panel.
    if (m_terminals.isEmpty())
        addTerminal();
    for (auto *term : m_terminals) {
        if (term->property("_nb_pending").toBool()) {
            term->setProperty("_nb_pending", false);
            term->startShellProgram();
            if (!m_cfg.execCommand.isEmpty()) {
                term->sendText(m_cfg.execCommand + "\n");
            }
            m_runningCommand = m_cfg.execCommand;
        }
    }
#endif
    emit visibilityChanged(true);
}

bool TerminalPanel::eventFilter(QObject *watched, QEvent *event)
{
#ifdef NEOBEAM_HAS_TERMINAL
    // Unsubmitted-input tracking for refreshPromptOnBranchChange(): every key
    // reaching the display widget updates the per-split dirty heuristic.
    if (event->type() == QEvent::KeyPress) {
        QTermWidget *term = nullptr;
        for (QObject *o = watched; o && !term; o = o->parent())
            term = qobject_cast<QTermWidget *>(o);
        if (term && m_terminals.contains(term))
            markLineDirty(term, static_cast<QKeyEvent *>(event));
    }

    // Copy-on-select, CLIPBOARD half: installed on each terminal's internal
    // Konsole::TerminalDisplay widget (createTermWidget). A left-button
    // release ends every selection gesture (drag, double-click word,
    // triple-click line, shift-extend).
    if (m_cfg.copyOnSelect
        && event->type() == QEvent::MouseButtonRelease
        && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
        QTermWidget *term = nullptr;
        for (QObject *o = watched; o && !term; o = o->parent())
            term = qobject_cast<QTermWidget *>(o);
        if (term && m_terminals.contains(term)) {
            // Deferred: the display's own release handler runs after this
            // filter and clears the selection when the release turns out to
            // be a plain click on an existing selection (aborted drag);
            // reading afterwards avoids copying a selection that is being
            // dismissed. PRIMARY is also rewritten so both targets always
            // hold the same text.
            QPointer<QTermWidget> guard(term);
            QTimer::singleShot(0, this, [guard]() {
                if (!guard)
                    return;
                const QString text = guard->selectedText(true);
                if (text.isEmpty())
                    return;
                QClipboard *cb = QApplication::clipboard();
                cb->setText(text, QClipboard::Clipboard);
                if (cb->supportsSelection())
                    cb->setText(text, QClipboard::Selection);
            });
        }
    }
#endif
    return QWidget::eventFilter(watched, event);
}

void TerminalPanel::setWorkingDirectory(const QString &dir)
{
    if (!m_cfg.followCwd || dir == m_cwd || dir.isEmpty())
        return;
    m_cwd = dir;
#ifdef NEOBEAM_HAS_TERMINAL
    if (m_currentTerm)
        m_currentTerm->changeDir(dir);
#endif
}

void TerminalPanel::broadcastCwd(const QString &dir)
{
    if (dir.isEmpty())
        return;
    m_cwd = dir;
#ifdef NEOBEAM_HAS_TERMINAL
    for (auto *term : m_terminals) {
        // Terminals whose shell hasn't started yet (panel still hidden) have
        // no running process for changeDir()'s "cd" keystrokes to reach, so
        // fix up their startup directory instead; it takes effect once
        // showEvent() calls startShellProgram() on them.
        if (term->property("_nb_pending").toBool())
            term->setWorkingDirectory(dir);
        else
            term->changeDir(dir);
    }
#endif
}

void TerminalPanel::adoptProject(const QString &dir, const QString &command)
{
#ifdef NEOBEAM_HAS_TERMINAL
    const QString previous = m_runningCommand;
    m_cfg.execCommand = command;

    bool anyStarted = false;
    for (auto *term : m_terminals) {
        if (!term->property("_nb_pending").toBool()) {
            anyStarted = true;
            break;
        }
    }

    // Nothing running yet (panel never shown, or shells still pending):
    // broadcastCwd fixes the pending shells' startup directory, and the new
    // exec command is picked up when showEvent starts them.
    // Running plain shells (no command on either side) are kept and cd'd.
    if (!anyStarted || (previous.isEmpty() && command.isEmpty())) {
        broadcastCwd(dir);
        return;
    }

    // A command is involved: replace every split with one fresh shell in the
    // project directory. addTerminal() starts it immediately when the panel
    // is visible, or leaves it pending for the next showEvent.
    m_cwd = dir;
    for (auto *term : m_terminals) {
        // The finished-handler must not run for shells we kill ourselves (it
        // would hide the panel once the list momentarily empties).
        disconnect(term, nullptr, this, nullptr);
        if (QFrame *wrapper = m_wrapperFor.take(term))
            wrapper->deleteLater(); // also deletes the child terminal widget
        else
            term->deleteLater();
    }
    m_terminals.clear();
    m_currentTerm = nullptr;
    addTerminal();
#else
    Q_UNUSED(dir);
    Q_UNUSED(command);
#endif
}

bool TerminalPanel::openSelectedLink()
{
#ifdef NEOBEAM_HAS_TERMINAL
    if (!m_currentTerm)
        return false;
    // Line breaks are kept (preserveLineBreaks) because linkInSelection needs
    // them to tell a link wrapped across lines from prose around a link.
    const QUrl url = linkInSelection(m_currentTerm->selectedText(true));
    if (!url.isValid()) {
        qWarning() << "No link found in the terminal selection";
        return false;
    }
    QDesktopServices::openUrl(url);
    return true;
#else
    return false;
#endif
}

void TerminalPanel::addTerminal()
{
#ifdef NEOBEAM_HAS_TERMINAL
    auto *term = createTermWidget();
    m_splitter->addWidget(wrapTerminal(term));
    m_currentTerm = term;
    m_terminals.append(term);
    term->setFocus();

    // QSplitter gives a freshly added pane only a sliver of leftover space by
    // default; redistribute so every split (old and new) gets an equal share
    // of the available space. Qt normalizes these as relative weights, so
    // equal values here mean an equal split regardless of handle overhead.
    const int count = m_splitter->count();
    const int avail = m_splitter->orientation() == Qt::Horizontal
                          ? m_splitter->width() : m_splitter->height();
    const int each = qMax(1, avail / qMax(1, count));
    m_splitter->setSizes(QList<int>(count, each));
#endif
}

void TerminalPanel::removeCurrentTerminal()
{
#ifdef NEOBEAM_HAS_TERMINAL
    if (!m_currentTerm || m_terminals.size() <= 1)
        return;

    m_terminals.removeOne(m_currentTerm);
    QFrame *wrapper = m_wrapperFor.take(m_currentTerm);
    m_lineDirty.remove(m_currentTerm);
    if (wrapper)
        wrapper->deleteLater(); // also deletes the child terminal widget
    else
        m_currentTerm->deleteLater();
    m_currentTerm = m_terminals.isEmpty() ? nullptr : m_terminals.last();
    if (m_currentTerm)
        m_currentTerm->setFocus();
#endif
}

#ifdef NEOBEAM_HAS_TERMINAL
void TerminalPanel::markLineDirty(QTermWidget *term, QKeyEvent *event)
{
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    const int key = event->key();
    if (key == Qt::Key_Return || key == Qt::Key_Enter
        || (ctrl && (key == Qt::Key_C || key == Qt::Key_U))) {
        m_lineDirty[term] = false;
        return;
    }
    m_lineDirty[term] = true;
}
#endif

void TerminalPanel::refreshPromptOnBranchChange()
{
#ifdef NEOBEAM_HAS_TERMINAL
    if (!m_cfg.refreshPromptOnGitChange)
        return;
    for (QTermWidget *term : m_terminals) {
        if (!m_lineDirty.value(term, false))
            term->sendText(QStringLiteral("\r"));
    }
#endif
}

void TerminalPanel::pasteClipboard()
{
#ifdef NEOBEAM_HAS_TERMINAL
    if (m_currentTerm)
        m_currentTerm->pasteClipboard();
#endif
}

void TerminalPanel::focusTerminal()
{
#ifdef NEOBEAM_HAS_TERMINAL
    if (m_currentTerm)
        m_currentTerm->setFocus();
#endif
}

bool TerminalPanel::focusTerminalAt(int index)
{
#ifdef NEOBEAM_HAS_TERMINAL
    if (index < 0 || index >= m_terminals.size())
        return false;
    m_terminals[index]->setFocus();
    return true;
#else
    Q_UNUSED(index);
    return false;
#endif
}

int TerminalPanel::indexOfFocused(QWidget *focused) const
{
#ifdef NEOBEAM_HAS_TERMINAL
    if (!focused)
        return -1;
    for (int i = 0; i < m_terminals.size(); ++i) {
        if (m_terminals[i] == focused || m_terminals[i]->isAncestorOf(focused))
            return i;
    }
#endif
    return -1;
}

void TerminalPanel::changeFontSize(int delta)
{
#ifdef NEOBEAM_HAS_TERMINAL
    const int size = qBound(kMinFontSize, m_cfg.fontSize + delta, kMaxFontSize);
    if (size == m_cfg.fontSize)
        return;
    m_cfg.fontSize = size;
    const QFont f = terminalFont();
    for (auto *term : m_terminals)
        term->setTerminalFont(f);
#else
    Q_UNUSED(delta);
#endif
}

#ifdef NEOBEAM_HAS_TERMINAL
QFont TerminalPanel::terminalFont() const
{
    QFont f(m_cfg.fontFamily, m_cfg.fontSize);
    f.setStyleHint(QFont::Monospace);
    f.setHintingPreference(m_cfg.hinting);
    if (!m_cfg.antialiasing)
        f.setStyleStrategy(QFont::NoAntialias);
    return f;
}

QString TerminalPanel::effectiveColorScheme()
{
    if (!m_schemeResolved.isEmpty())
        return m_schemeResolved;
    m_schemeResolved = m_cfg.colorScheme;

    const QColor fg(m_cfg.foreground);
    const QColor bg(m_cfg.background);
    if (!m_cfg.foreground.isEmpty() && !fg.isValid())
        qWarning() << "Invalid terminal foreground color:" << m_cfg.foreground;
    if (!m_cfg.background.isEmpty() && !bg.isValid())
        qWarning() << "Invalid terminal background color:" << m_cfg.background;
    if (!fg.isValid() && !bg.isValid())
        return m_schemeResolved;

    // Derive a scheme file with the overrides applied on top of the base
    // scheme's palette. QTermWidget has no per-color API; colors can only be
    // set through a .colorscheme file, so generate one in the cache dir.
    const QString basePath = findColorSchemeFile(m_cfg.colorScheme);
    if (basePath.isEmpty())
        qWarning() << "Color scheme" << m_cfg.colorScheme
                   << "not found; foreground/background overrides applied to the default palette";

    const QString outDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!QDir().mkpath(outDir))
        return m_schemeResolved;
    const QString outPath = outDir + QStringLiteral("/custom.colorscheme");
    QFile::remove(outPath);
    if (!basePath.isEmpty() && !QFile::copy(basePath, outPath)) {
        qWarning() << "Failed to write" << outPath << "- foreground/background overrides ignored";
        return m_schemeResolved;
    }

    // .colorscheme files are QSettings-compatible INI; rewriting just the
    // Foreground/Background groups keeps the base palette intact.
    QSettings out(outPath, QSettings::IniFormat);
    auto writeColor = [&out](const QString &group, const QColor &c) {
        out.setValue(group + QStringLiteral("/Color"),
                     QStringList{QString::number(c.red()),
                                 QString::number(c.green()),
                                 QString::number(c.blue())});
    };
    if (fg.isValid()) {
        writeColor(QStringLiteral("Foreground"), fg);
        writeColor(QStringLiteral("ForegroundIntense"), fg.lighter(120));
    }
    if (bg.isValid()) {
        writeColor(QStringLiteral("Background"), bg);
        writeColor(QStringLiteral("BackgroundIntense"), bg);
    }
    out.sync();

    m_schemeResolved = outPath;
    return m_schemeResolved;
}

QTermWidget *TerminalPanel::createTermWidget()
{
    registerNeobeamSchemeDirs();

    auto *term = new QTermWidget(0, this);

    term->setTerminalFont(terminalFont());

    // Color scheme (default "Neoblue", shipped with neobeam), with optional
    // foreground/background overrides baked into a derived scheme file.
    term->setColorScheme(effectiveColorScheme());
    term->setScrollBarPosition(QTermWidgetInterface::ScrollBarRight);
    term->setHistorySize(m_cfg.scrollback); // scrollback lines (in-memory buffer)
    term->setWorkingDirectory(m_cwd);
    term->setTerminalOpacity(1.0);

    // NOTE: Sixel image protocol is not supported by upstream QTermWidget.
    // Tracking: https://github.com/lxqt/qtermwidget — no sixel code exists.
    // When/if upstream adds sixel support, enable it here.

    // Installed unconditionally: copy-on-select (gated by m_cfg.copyOnSelect
    // inside eventFilter() itself - qtermwidget only mirrors the selection to
    // PRIMARY on its own, its copyAvailable signal is never emitted upstream
    // as of 2.4.0) and the "unsubmitted input" keystroke tracking used by
    // refreshPromptOnBranchChange() both watch this same internal display
    // widget.
    const auto children = term->findChildren<QWidget *>();
    for (QWidget *child : children) {
        if (child->inherits("Konsole::TerminalDisplay"))
            child->installEventFilter(this);
    }

    // Double-click selects a whole URL rather than stopping at the query
    // string, which also makes openSelectedLink() a one-gesture fallback.
    term->setWordCharacters(m_cfg.wordCharacters);

    // Right-click: link actions when a link sits under the cursor, otherwise
    // the configured PuTTY-style paste. Installed regardless of the paste
    // setting, since qtermwidget itself binds no context menu.
    term->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(term, &QWidget::customContextMenuRequested, this,
            [this, term](const QPoint &pos) { showContextMenu(term, pos); });

    // Shell program
    if (!m_cfg.shell.isEmpty())
        term->setShellProgram(m_cfg.shell);
    if (!m_cfg.shellArgs.isEmpty())
        term->setArgs(m_cfg.shellArgs);

    // Defer shell start until visible so terminal gets correct size
    if (isVisible()) {
        term->startShellProgram();
        if (!m_cfg.execCommand.isEmpty()) {
            term->sendText(m_cfg.execCommand + "\n");
        }
        m_runningCommand = m_cfg.execCommand;
    } else {
        term->setProperty("_nb_pending", true);
    }

    // Click opens URLs/emails under the cursor (QTermWidget underlines them on
    // hover and emits this on click; neobeam just has to act on it). Every
    // split of both panels is created here, so the bottom and the side terminal
    // handle links identically. Links its UrlFilter does not recognize -
    // scheme-less ones, or ones a TUI application wrapped across lines - are
    // covered by openSelectedLink() instead.
    connect(term, &QTermWidget::urlActivated, this, [](const QUrl &url, bool) {
        QDesktopServices::openUrl(url);
    });

    connect(term, &QTermWidget::termGetFocus, this, [this, term]() {
        m_currentTerm = term;
        updateFocusHighlight(term);
    });
    connect(term, &QTermWidget::termLostFocus, this, [this]() {
        // If focus moved to another split rather than away from the panel,
        // its termGetFocus (focus-out fires before focus-in) will run right
        // after this and re-highlight the right wrapper.
        updateFocusHighlight(nullptr);
    });
    connect(term, &QTermWidget::finished, this, [this, term]() {
        m_terminals.removeOne(term);
        QFrame *wrapper = m_wrapperFor.take(term);
        if (wrapper)
            wrapper->deleteLater(); // also deletes the child terminal widget
        else
            term->deleteLater();
        if (term == m_currentTerm)
            m_currentTerm = m_terminals.isEmpty() ? nullptr : m_terminals.last();
        if (m_terminals.isEmpty()) {
            hide();
            emit visibilityChanged(false); // keep the toggle button in sync
        }
    });

    return term;
}

QFrame *TerminalPanel::wrapTerminal(QTermWidget *term)
{
    auto *wrapper = new QFrame(this);
    wrapper->setFrameShape(QFrame::NoFrame);
    // Scoped by object name rather than the bare "QFrame" type selector, since
    // any QFrame-derived widget nested inside (e.g. QLabel) would otherwise
    // also pick up the border rule.
    wrapper->setObjectName("nbTermWrapper");
    auto *layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(1, 1, 1, 1); // room for the focus-highlight border
    layout->setSpacing(0);
    layout->addWidget(term);
    m_wrapperFor.insert(term, wrapper);
    updateFocusHighlight(m_currentTerm); // keep existing highlight consistent as splits are added/removed
    return wrapper;
}

void TerminalPanel::showContextMenu(QTermWidget *term, const QPoint &pos)
{
    // The hotspot's matched text is read up front instead of reusing the
    // hotspot's own QActions: the filter chain (and with it every hotspot) is
    // rebuilt on any terminal output, which would leave a menu built from those
    // actions holding dangling pointers.
    QUrl url;
    if (Filter::HotSpot *spot = term->getHotSpotAt(pos)) {
        if (spot->type() == Filter::HotSpot::Link) {
            if (auto *match = dynamic_cast<Konsole::RegExpFilter::HotSpot *>(spot))
                url = normalizeLink(match->capturedTexts().value(0));
        }
    }
    if (!url.isValid()) {
        if (m_cfg.rightClickPaste)
            term->pasteClipboard();
        return;
    }

    QMenu menu(this);
    const QString bg = panelBackgroundColor().name();
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: %1; color: #cdd6f4; border: 1px solid #333; }"
        "QMenu::item { padding: 4px 16px; }"
        "QMenu::item:selected { background: #333; }").arg(bg));
    QAction *openAct = menu.addAction(QStringLiteral("Open Link"));
    QAction *copyAct = menu.addAction(QStringLiteral("Copy Link"));
    const QAction *chosen = menu.exec(term->mapToGlobal(pos));
    if (chosen == openAct)
        QDesktopServices::openUrl(url);
    else if (chosen == copyAct)
        QApplication::clipboard()->setText(url.toString());
}

void TerminalPanel::updateFocusHighlight(QTermWidget *focused)
{
    for (auto it = m_wrapperFor.constBegin(); it != m_wrapperFor.constEnd(); ++it) {
        const bool isFocused = it.key() == focused;
        it.value()->setStyleSheet(isFocused
            ? QStringLiteral("QFrame#nbTermWrapper { border: 1px solid %1; }").arg(m_cfg.focusBorderColor.name())
            : QStringLiteral("QFrame#nbTermWrapper { border: 1px solid transparent; }"));
    }
}
#endif
