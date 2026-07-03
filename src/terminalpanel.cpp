#include "terminalpanel.h"
#include <QShowEvent>
#include <QVBoxLayout>
#include <QDir>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>

TerminalPanel::TerminalPanel(const TerminalConfig &cfg, QWidget *parent)
    : QWidget(parent)
    , m_cfg(cfg)
    , m_cwd(QDir::homePath())
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Toolbar with terminal actions
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet("QToolBar { background: #000; border: none; spacing: 4px; }"
                             "QToolButton { color: #aaa; background: transparent; border: none; padding: 2px 6px; }"
                             "QToolButton:hover { background: #333; border-radius: 3px; }");

    auto *addAct = m_toolbar->addAction("+", this, &TerminalPanel::addTerminal);
    addAct->setToolTip("New terminal");
    auto *cwdAct = m_toolbar->addAction("↓");
    cwdAct->setToolTip("Send nvim cwd to all terminals");
    connect(cwdAct, &QAction::triggered, this, &TerminalPanel::cwdBroadcastRequested);
    auto *closeAct = m_toolbar->addAction("✕", this, &TerminalPanel::removeCurrentTerminal);
    closeAct->setToolTip("Close terminal");

    layout->addWidget(m_toolbar);

    // Splitter holds terminal instances side by side (bottom panel) or
    // stacked vertically (side panel, where horizontal space is scarce).
    m_splitter = new QSplitter(m_cfg.sidePanel ? Qt::Vertical : Qt::Horizontal, this);
    m_splitter->setStyleSheet("QSplitter { background: #000; }"
                              "QSplitter::handle { background: #333; width: 2px; height: 2px; }");
    layout->addWidget(m_splitter, 1);

    setStyleSheet(m_cfg.sidePanel
        ? "background: #000; border-left: 1px solid #333;"
        : "background: #000; border-top: 1px solid #333;");
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
        }
    }
#endif
    emit visibilityChanged(true);
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
    if (wrapper)
        wrapper->deleteLater(); // also deletes the child terminal widget
    else
        m_currentTerm->deleteLater();
    m_currentTerm = m_terminals.isEmpty() ? nullptr : m_terminals.last();
    if (m_currentTerm)
        m_currentTerm->setFocus();
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

QTermWidget *TerminalPanel::createTermWidget()
{
    auto *term = new QTermWidget(0, this);

    term->setTerminalFont(terminalFont());

    // Color scheme (default "Linux" = white on black)
    term->setColorScheme(m_cfg.colorScheme);
    term->setScrollBarPosition(QTermWidgetInterface::ScrollBarRight);
    term->setHistorySize(m_cfg.scrollback); // scrollback lines (in-memory buffer)
    term->setWorkingDirectory(m_cwd);
    term->setTerminalOpacity(1.0);

    // NOTE: Sixel image protocol is not supported by upstream QTermWidget.
    // Tracking: https://github.com/lxqt/qtermwidget — no sixel code exists.
    // When/if upstream adds sixel support, enable it here.

    // Right-click paste (PuTTY-style)
    if (m_cfg.rightClickPaste) {
        term->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(term, &QWidget::customContextMenuRequested, this, [term]() {
            term->pasteClipboard();
        });
    }

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
    } else {
        term->setProperty("_nb_pending", true);
    }

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
