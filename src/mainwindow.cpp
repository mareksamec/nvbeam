#include "mainwindow.h"
#include "gridwidget.h"
#include "nvimprocess.h"
#include "uiwidgets.h"
#include "terminalpanel.h"
#include "minimap.h"
#include "imagerenderer.h"
#include "config.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

/// Load a Lua script from the lua/ directory next to the executable
static QString loadLua(const QString &name)
{
    // Try relative to executable first, then source tree fallback
    QStringList searchPaths = {
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

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabline = new TablineWidget(cfg.accentColor, central);
    layout->addWidget(m_tabline);

    m_grid = new GridWidget(cfg.font(), central);
    m_grid->setMouseConfig(cfg.mouseTerminalSelection, cfg.mouseAutoCopy,
                           QColor(cfg.mouseSelectionColor));

    // Grid + minimap side by side
    m_minimap = new MinimapWidget(central);
    auto *editorRow = new QHBoxLayout();
    editorRow->setContentsMargins(0, 0, 0, 0);
    editorRow->setSpacing(0);
    editorRow->addWidget(m_grid, 1);
    editorRow->addWidget(m_minimap);
    layout->addLayout(editorRow, 1);

    m_messages = new MessageWidget(cfg.accentColor, central);
    layout->addWidget(m_messages);

    m_statusline = new StatuslineWidget(cfg.accentColor, cfg.statuslineDivider, central);
    if (cfg.extStatusline)
        layout->addWidget(m_statusline);

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
    m_terminal = new TerminalPanel(termCfg, central);
    layout->addWidget(m_terminal);

    // Ctrl+` toggles terminal panel
    auto *termShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft), this);
    connect(termShortcut, &QShortcut::activated, m_terminal, &TerminalPanel::toggle);

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
    connect(m_nvim, &NvimProcess::ready, this, [this]() {
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
}

MainWindow::~MainWindow()
{
    if (m_nvim) {
        m_nvim->stop();
    }
}
