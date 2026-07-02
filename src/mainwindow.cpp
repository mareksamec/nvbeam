#include "mainwindow.h"
#include "gridwidget.h"
#include "nvimprocess.h"
#include "uiwidgets.h"
#include "terminalpanel.h"
#include "minimap.h"
#include "imagerenderer.h"
#include "config.h"
#include "session.h"
#include <QApplication>
#include <QCloseEvent>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QKeyEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

/// Load a Lua script from the lua/ directory next to the executable
static QString loadLua(const QString &name)
{
    // Try the installed share dir first (/usr/bin/neobeam -> /usr/share/neobeam/lua),
    // then the source-tree layout used when running from the build directory.
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/../share/neobeam/lua/" + name,
        QCoreApplication::applicationDirPath() + "/../lua/" + name,
        QCoreApplication::applicationDirPath() + "/lua/" + name,
    };
    for (const auto &path : searchPaths) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString::fromUtf8(f.readAll());
    }
    qWarning("Failed to load lua/%s", qPrintable(name));
    return {};
}

MainWindow::MainWindow(const Config &cfg, const QStringList &nvimArgs, QWidget *parent)
    : QMainWindow(parent)
    , m_cfg(cfg)
{
    setWindowTitle("neobeam");
    resize(800, 600);

    // A single positional argument that is an existing directory means the
    // user opened a folder (e.g. `neobeam .`). Window geometry and terminal
    // split layout still restore normally; only the nvim/terminal cwd is
    // overridden to point at the folder (handled once nvim is ready, below).
    QString openFolder;
    if (nvimArgs.size() == 1) {
        QFileInfo info(nvimArgs.first());
        if (info.isDir())
            openFolder = info.absoluteFilePath();
    }

    m_session = new Session();

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Vertical splitter: the editor area (top) and the terminal panel (bottom)
    // are resizable relative to each other by dragging the handle.
    m_vsplitter = new QSplitter(Qt::Vertical, central);
    m_vsplitter->setStyleSheet("QSplitter::handle { background: #333; height: 2px; }");
    m_vsplitter->setChildrenCollapsible(false);

    // Editor area container: tabline, grid+minimap row, messages, statusline.
    auto *editorArea = new QWidget(m_vsplitter);
    auto *editorLayout = new QVBoxLayout(editorArea);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    m_tabline = new TablineWidget(cfg.accentColor, editorArea);
    editorLayout->addWidget(m_tabline);

    m_grid = new GridWidget(cfg.font(), editorArea);
    m_grid->setMouseConfig(cfg.mouseTerminalSelection, cfg.mouseAutoCopy,
                           QColor(cfg.mouseSelectionColor));

    // Grid + minimap side by side
    m_minimap = new MinimapWidget(editorArea);
    auto *editorRow = new QHBoxLayout();
    editorRow->setContentsMargins(0, 0, 0, 0);
    editorRow->setSpacing(0);
    editorRow->addWidget(m_grid, 1);
    editorRow->addWidget(m_minimap);
    editorLayout->addLayout(editorRow, 1);

    m_messages = new MessageWidget(cfg.accentColor, editorArea);
    editorLayout->addWidget(m_messages);

    m_statusline = new StatuslineWidget(cfg.accentColor, cfg.statuslineDivider, editorArea);
    if (cfg.extStatusline)
        editorLayout->addWidget(m_statusline);

    // Embedded terminal panel (VSCode-like, below statusline)
    TerminalConfig termCfg;
    termCfg.fontFamily = cfg.terminalFont.isEmpty() ? cfg.fontFamily : cfg.terminalFont;
    termCfg.fontSize = cfg.terminalFontSize > 0 ? cfg.terminalFontSize : cfg.fontSize;
    termCfg.followCwd = cfg.terminalFollowCwd;
    termCfg.height = cfg.terminalHeight;
    termCfg.colorScheme = cfg.terminalColorScheme;
    termCfg.shell = cfg.terminalShell;
    termCfg.shellArgs = cfg.terminalShellArgs;
    termCfg.execCommand = cfg.terminalExecCommand;
    termCfg.rightClickPaste = cfg.terminalRightClickPaste;
    termCfg.antialiasing = cfg.terminalAntialiasing;
    termCfg.hinting = cfg.terminalHinting;
    termCfg.scrollback = cfg.terminalScrollback;
    m_terminal = new TerminalPanel(termCfg, m_vsplitter);

    // Assemble the vertical splitter: editor area on top, terminal below.
    m_vsplitter->addWidget(editorArea);
    m_vsplitter->addWidget(m_terminal);
    m_vsplitter->setStretchFactor(0, 1); // editor area takes extra space
    m_vsplitter->setStretchFactor(1, 0);
    layout->addWidget(m_vsplitter, 1);

    // Ctrl+` toggles terminal panel
    auto *termShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft), this);
    connect(termShortcut, &QShortcut::activated, m_terminal, &TerminalPanel::toggle);

    // Ctrl+Shift+E splits the terminal; Ctrl+Shift+W closes the active split
    auto *termSplitShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), this);
    connect(termSplitShortcut, &QShortcut::activated, m_terminal, &TerminalPanel::addTerminal);
    auto *termCloseSplitShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W), this);
    connect(termCloseSplitShortcut, &QShortcut::activated, m_terminal, &TerminalPanel::removeCurrentTerminal);

    // Ctrl+Shift+Down performs the same action as the terminal's ↓ toolbar button.
    auto *termCwdShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down), this);
    connect(termCwdShortcut, &QShortcut::activated, m_terminal, &TerminalPanel::cwdBroadcastRequested);

    // Arrow-down toolbar button: broadcast nvim's current cwd to all terminal splits.
    connect(m_terminal, &TerminalPanel::cwdBroadcastRequested, this, [this]() {
        m_nvim->request("nvim_exec_lua",
            {QStringLiteral("return vim.fn.getcwd()"), QVariantList{}},
            [this](const QVariant &result) {
                m_terminal->broadcastCwd(result.toString());
            });
    });

    // Paste shortcuts: route to terminal or nvim depending on focus
    auto pasteHandler = [this]() {
        if (m_terminal->isVisible() && m_terminal->isAncestorOf(focusWidget())) {
            m_terminal->pasteClipboard();
        } else {
            QString text = QGuiApplication::clipboard()->text();
            if (!text.isEmpty())
                m_nvim->request("nvim_paste", {text, true, -1});
        }
    };
    auto *pasteShortcut1 = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Insert), this);
    connect(pasteShortcut1, &QShortcut::activated, this, pasteHandler);
    auto *pasteShortcut2 = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), this);
    connect(pasteShortcut2, &QShortcut::activated, this, pasteHandler);

    // Bottom-right toggle buttons (minimap, terminal)
    auto *btnBar = new QWidget(central);
    auto *btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 4, 0);
    btnLayout->setSpacing(4);
    btnLayout->addStretch();

    auto btnStyle = QStringLiteral(
        "QPushButton { color: #888; background: #1a1b26; border: 1px solid #333; "
        "border-radius: 3px; padding: 2px 8px; font-size: 9pt; }"
        "QPushButton:hover { color: #cdd6f4; background: #313244; }"
        "QPushButton:checked { color: #50fa7b; border-color: #50fa7b; }");

    auto *minimapBtn = new QPushButton("⊟", central);
    minimapBtn->setToolTip("Toggle minimap");
    minimapBtn->setCheckable(true);
    minimapBtn->setChecked(true);
    minimapBtn->setFixedSize(28, 20);
    minimapBtn->setStyleSheet(btnStyle);
    btnLayout->addWidget(minimapBtn);

    auto *termBtn = new QPushButton("⌨", central);
    termBtn->setToolTip("Toggle terminal");
    termBtn->setCheckable(true);
    termBtn->setChecked(false);
    termBtn->setFixedSize(28, 20);
    termBtn->setStyleSheet(btnStyle);
    btnLayout->addWidget(termBtn);

    btnBar->setFixedHeight(24);
    layout->addWidget(btnBar);

    connect(minimapBtn, &QPushButton::toggled, m_minimap, &QWidget::setVisible);
    connect(termBtn, &QPushButton::toggled, m_terminal, &QWidget::setVisible);
    connect(m_terminal, &TerminalPanel::visibilityChanged, termBtn, &QPushButton::setChecked);

    setCentralWidget(central);

    // --- Session restore ---
    // Terminal layout: restore split count + proportions before deciding
    // visibility, so the panel comes back exactly as it was left.
    const bool restoreTerm = cfg.sessionRestore && cfg.sessionRestoreTerminal
                             && m_session->hasTerminalState();
    if (restoreTerm)
        m_terminal->restoreSplits(m_session->terminalSplitCount(),
                                  m_session->terminalSplitterState());

    // Expanded state: when session restore is active it takes precedence over
    // the start_expanded config default; otherwise fall back to the config.
    const bool startExpanded = restoreTerm ? m_session->terminalExpanded()
                                           : cfg.terminalStartExpanded;
    // Toggling the button (after connections are wired) keeps panel and button
    // in sync and triggers TerminalPanel::showEvent, which starts the shell.
    if (startExpanded)
        termBtn->setChecked(true);

    // Editor/terminal split proportions. If no saved state, seed the terminal
    // with its configured startup height.
    if (cfg.sessionRestore && !m_session->verticalSplitterState().isEmpty()) {
        m_vsplitter->restoreState(m_session->verticalSplitterState());
    } else {
        const int h = qMax(1, height());
        m_vsplitter->setSizes({ qMax(1, h - cfg.terminalHeight), cfg.terminalHeight });
    }

    // Window geometry (best-effort; the window manager / Wayland compositor may
    // override size and will ignore position on Wayland). Applied before show().
    if (cfg.sessionRestore && cfg.sessionRestoreWindow
        && !m_session->windowGeometry().isEmpty()) {
        restoreGeometry(m_session->windowGeometry());
    }

    // Popupmenu is a floating child of the grid
    m_popupmenu = new PopupmenuWidget(cfg.accentColor, m_grid);

    // Image renderer overlay (for future vim.ui.img API)
    m_imageRenderer = new ImageRenderer(m_grid);

    m_nvim = new NvimProcess(this);
    connect(m_nvim, &NvimProcess::redraw, m_grid, &GridWidget::handleRedraw);
    connect(m_nvim, &NvimProcess::finished, this, &QMainWindow::close);
    connect(m_grid, &GridWidget::gridSizeReady, this, [this](int rows, int cols) {
        m_nvim->request("nvim_ui_try_resize_grid", {1, cols, rows});
    });
    connect(m_nvim, &NvimProcess::ready, this, [this, openFolder]() {
        // Folder open (e.g. `neobeam .`): point both nvim's cwd and every
        // terminal split (including any restored from the session) at the
        // folder, overriding whatever cwd nvim/the terminals started with.
        if (!openFolder.isEmpty()) {
            m_nvim->request("nvim_set_current_dir", {openFolder});
            m_terminal->broadcastCwd(openFolder);
        }
        QTimer::singleShot(0, this, [this]() {
            int cols = m_grid->width() / m_grid->cellWidth();
            int rows = m_grid->height() / m_grid->cellHeight();
            if (cols < 1) cols = 80;
            if (rows < 1) rows = 24;

            QVariantMap opts;
            opts["ext_linegrid"] = true;
            opts["ext_hlstate"] = true;
            opts["ext_multigrid"] = true;
            if (m_cfg.extPopupmenu) opts["ext_popupmenu"] = true;
            if (m_cfg.extTabline) opts["ext_tabline"] = true;
            m_nvim->request("nvim_ui_attach", {cols, rows, opts});
        });
    });

    // Input
    connect(m_grid, &GridWidget::inputKey, this, [this](const QString &keys) {
        m_nvim->request("nvim_input", {keys});
    });
    connect(m_grid, &GridWidget::inputMouse, this,
            [this](const QString &button, const QString &action,
                   const QString &mod, int grid, int row, int col) {
        m_nvim->request("nvim_input_mouse", {button, action, mod, grid, row, col});
    });

    // File drops onto the buffer area: open each dropped file in nvim.
    //
    // Targeting: the drop resolves the window under the cursor; we focus it
    // first so the file opens where it was dropped (multi-grid aware).
    //
    // Modifiers select how the file is opened (mirrors common editor habits):
    //   (none)  :edit      open in the target window
    //   Shift   :split     open in a horizontal split
    //   Ctrl    :vsplit    open in a vertical split
    //   Alt     :tabedit   open in a new tab
    //
    // Using nvim_cmd with a list of args lets Neovim handle path escaping
    // (spaces, %, # etc.) instead of building a fragile command string.
    connect(m_grid, &GridWidget::filesDropped, this,
            [this](const QStringList &paths, int win, Qt::KeyboardModifiers mods) {
        // Focus the window under the drop point, if known.
        if (win >= 0)
            m_nvim->request("nvim_set_current_win", { win });

        QString openCmd = "edit";
        if (mods & Qt::AltModifier)
            openCmd = "tabedit";
        else if (mods & Qt::ControlModifier)
            openCmd = "vsplit";
        else if (mods & Qt::ShiftModifier)
            openCmd = "split";

        for (const QString &path : paths) {
            QVariantMap cmd;
            cmd["cmd"] = openCmd;
            cmd["args"] = QVariantList{ path };
            m_nvim->request("nvim_cmd", { cmd, QVariantMap{} });
        }
    });

    // Externalized UI signals
    connect(m_grid, &GridWidget::popupmenuShow, this,
            [this](const QVariantList &items, int selected, int row, int col, int grid) {
        Q_UNUSED(grid)
        m_popupmenu->show(items, selected, row, col, grid,
                          m_grid->cellWidth(), m_grid->cellHeight());
    });
    connect(m_grid, &GridWidget::popupmenuSelect, m_popupmenu, &PopupmenuWidget::select);
    connect(m_grid, &GridWidget::popupmenuHide, m_popupmenu, &PopupmenuWidget::hidePopup);

    connect(m_grid, &GridWidget::tablineUpdate, m_tabline, &TablineWidget::update);
    connect(m_tabline, &TablineWidget::tabSelected, this, [this](int tabId) {
        m_nvim->request("nvim_set_current_tabpage", {tabId});
    });

    connect(m_tabline, &TablineWidget::bufferSelected, this, [this](int bufId) {
        m_nvim->request("nvim_set_current_buf", {bufId});
    });

    // Minimap click scrolls to line
    connect(m_minimap, &MinimapWidget::lineClicked, this, [this](int line) {
        m_nvim->request("nvim_win_set_cursor", {0, QVariantList{line + 1, 0}});
    });

    connect(m_grid, &GridWidget::msgShow, m_messages, &MessageWidget::showMsg);
    connect(m_grid, &GridWidget::msgClear, m_messages, &MessageWidget::clear);
    connect(m_grid, &GridWidget::msgShowmode, m_messages, &MessageWidget::showMode);
    connect(m_grid, &GridWidget::msgRuler, m_messages, &MessageWidget::showRuler);

    // Bufferline: refresh buffer list on each flush (throttled)
    if (cfg.bufferline) {
        auto *bufTimer = new QTimer(this);
        bufTimer->setSingleShot(true);
        bufTimer->setInterval(100);
        connect(bufTimer, &QTimer::timeout, this, [this]() {
            QString lua = loadLua("bufferline.lua");
            if (lua.isEmpty()) return;
            m_nvim->request("nvim_exec_lua", {lua, QVariantList{}}, [this](const QVariant &result) {
                QVariantMap data = result.toMap();
                int currentBuf = data["current"].toInt();
                QVariantList bufs = data["bufs"].toList();
                m_tabline->updateBuffers(bufs, currentBuf);
            });
        });
        connect(m_grid, &GridWidget::flushed, bufTimer, [bufTimer]() {
            if (!bufTimer->isActive()) bufTimer->start();
        });
    }

    // Statusline: poll Neovim for mode, file, position, diagnostics (throttled)
    if (cfg.extStatusline) {
        auto *statusTimer = new QTimer(this);
        statusTimer->setSingleShot(true);
        statusTimer->setInterval(100);
        connect(statusTimer, &QTimer::timeout, this, [this]() {
            QString lua = loadLua("statusline.lua");
            if (lua.isEmpty()) return;
            m_nvim->request("nvim_exec_lua", {lua, QVariantList{}}, [this](const QVariant &result) {
                m_statusline->updateStatus(result.toMap());
            });
        });
        connect(m_grid, &GridWidget::flushed, statusTimer, [statusTimer]() {
            if (!statusTimer->isActive()) statusTimer->start();
        });
    }

    // Minimap: poll buffer content and highlights (throttled).
    // Passes last known changedtick to Lua so it can skip the treesitter scan when only scroll changed.
    {
        auto *minimapTimer = new QTimer(this);
        minimapTimer->setSingleShot(true);
        minimapTimer->setInterval(20);
        connect(minimapTimer, &QTimer::timeout, this, [this]() {
            QString lua = loadLua("minimap.lua");
            if (lua.isEmpty()) return;
            // Pass last tick as argument; Lua returns full data only when tick changed.
            m_nvim->request("nvim_exec_lua", {lua, QVariantList{m_minimapLastTick}}, [this](const QVariant &result) {
                QVariantMap data = result.toMap();
                int tick   = data["tick"].toInt();
                int total  = data["total"].toInt();
                int top    = data["top"].toInt() - 1;
                int bot    = data["bot"].toInt();
                int cursor = data["cursor"].toInt() - 1;
                int visibleLines = bot - top;

                if (!data.contains("lines")) {
                    // Tick unchanged: only scroll/cursor moved — cheap overlay-only repaint.
                    m_minimap->setViewport(top, visibleLines, cursor);
                    return;
                }

                // Tick changed: rebuild highlight pixmap.
                // Only "lock in" this tick once the scan covered the whole capped
                // span (max 2000 lines, matching minimap.lua). This prevents a
                // partial scan from suppressing future refreshes via the tick gate.
                int scanned = data["scanned"].toInt();
                if (scanned >= qMin(total, 2000))
                    m_minimapLastTick = tick;
                QVariantList hlLines = data["lines"].toList();
                QVector<MinimapLine> lines;
                lines.reserve(hlLines.size());
                for (const auto &lineVar : hlLines) {
                    MinimapLine ml;
                    QVariantList cols = lineVar.toList();
                    ml.colors.reserve(cols.size());
                    for (const auto &colVar : cols) {
                        QString hex = colVar.toString();
                        ml.colors.append(hex.isEmpty() ? QColor(0x56, 0x5f, 0x89) : QColor(hex));
                    }
                    lines.append(ml);
                }
                m_minimap->updateContent(lines, total, top, visibleLines, cursor);
            });
        });
        connect(m_grid, &GridWidget::flushed, minimapTimer, [minimapTimer]() {
            if (!minimapTimer->isActive()) minimapTimer->start();
        });
    }

    // Terminal cwd tracking: poll nvim cwd and update terminal panel
    if (cfg.terminalFollowCwd) {
        auto *cwdTimer = new QTimer(this);
        cwdTimer->setInterval(500);
        connect(cwdTimer, &QTimer::timeout, this, [this]() {
            m_nvim->request("nvim_exec_lua",
                {QStringLiteral("return vim.fn.getcwd()"), QVariantList{}},
                [this](const QVariant &result) {
                    m_terminal->setWorkingDirectory(result.toString());
                });
        });
        cwdTimer->start();
    }

    m_nvim->start(nvimArgs);

    // Ctrl+Tab switches focus between the editor grid and the terminal. It is
    // caught by an application-level event filter because both the grid
    // (GridWidget::event) and QTermWidget consume Tab before a QShortcut would
    // see it.
    qApp->installEventFilter(this);

    // Focus the editor grid on startup rather than the terminal. The terminal
    // panel grabs focus in its showEvent when started expanded, so defer this
    // until the event loop has processed those show events.
    QTimer::singleShot(0, this, [this]() { m_grid->setFocus(); });
}

MainWindow::~MainWindow()
{
    if (m_nvim) {
        m_nvim->stop();
    }
    delete m_session;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Persist session state. We always write it; the restore side is gated by
    // config, so saving unconditionally keeps the file ready if the user later
    // enables restore.
    if (m_session) {
        m_session->setWindowGeometry(saveGeometry());
        if (m_vsplitter)
            m_session->setVerticalSplitterState(m_vsplitter->saveState());
        if (m_terminal) {
            m_session->setTerminalSplitCount(m_terminal->splitCount());
            m_session->setTerminalSplitterState(m_terminal->splitterState());
            m_session->setTerminalExpanded(m_terminal->isExpanded());
        }
        m_session->sync();
    }
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        // Key_Backtab is what Ctrl+Shift+Tab produces; treat both directions
        // the same since there are only two panes to cycle between.
        if ((ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab)
            && (ke->modifiers() & Qt::ControlModifier)) {
            toggleGridTerminalFocus();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::toggleGridTerminalFocus()
{
    QWidget *focused = QApplication::focusWidget();
    const bool termFocused = m_terminal && m_terminal->isVisible()
                             && focused && m_terminal->isAncestorOf(focused);
    if (termFocused) {
        m_grid->setFocus();
    } else if (m_terminal && m_terminal->isVisible()) {
        m_terminal->focusTerminal();
    } else {
        // Terminal hidden: nothing to switch to, keep the grid focused.
        m_grid->setFocus();
    }
}
