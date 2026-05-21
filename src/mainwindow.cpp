#include "mainwindow.h"
#include "gridwidget.h"
#include "nvimprocess.h"
#include "uiwidgets.h"
#include "terminalpanel.h"
#include "minimap.h"
#include "imagerenderer.h"
#include "config.h"
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>
MainWindow::MainWindow(const Config &cfg, const QStringList &nvimArgs, QWidget *parent)
    : QMainWindow(parent)
    , m_cfg(cfg)
{
    setWindowTitle("neobeam");
    resize(800, 600);

    // Central widget with layout for tabline + grid + cmdline/messages
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabline = new TablineWidget(cfg.accentColor, central);
    layout->addWidget(m_tabline);

    m_grid = new GridWidget(cfg.font(), central);

    // Grid + minimap side by side
    m_minimap = new MinimapWidget(central);
    auto *editorRow = new QHBoxLayout();
    editorRow->setContentsMargins(0, 0, 0, 0);
    editorRow->setSpacing(0);
    editorRow->addWidget(m_grid, 1);
    editorRow->addWidget(m_minimap);
    layout->addLayout(editorRow, 1);

    m_cmdline = new CmdlineWidget(cfg.accentColor, central);
    layout->addWidget(m_cmdline);

    m_messages = new MessageWidget(cfg.accentColor, central);
    layout->addWidget(m_messages);

    m_statusline = new StatuslineWidget(cfg.accentColor, central);
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
            if (m_cfg.extCmdline) opts["ext_cmdline"] = true;
            if (m_cfg.extPopupmenu) opts["ext_popupmenu"] = true;
            if (m_cfg.extTabline) opts["ext_tabline"] = true;
            if (m_cfg.extMessages) opts["ext_messages"] = true;
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
    connect(m_grid, &GridWidget::cmdlineShow, m_cmdline, &CmdlineWidget::show);
    connect(m_grid, &GridWidget::cmdlineHide, m_cmdline, &CmdlineWidget::hide);
    connect(m_grid, &GridWidget::cmdlinePos, m_cmdline, &CmdlineWidget::setPos);

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
        QString cmd = QString("nvim_win_set_cursor");
        m_nvim->request(cmd, {0, QVariantList{line + 1, 0}});
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
            QString lua = R"(
                local bufs = {}
                local cur = vim.api.nvim_get_current_buf()
                for _, b in ipairs(vim.api.nvim_list_bufs()) do
                    if vim.bo[b].buflisted then
                        table.insert(bufs, {
                            id = b,
                            name = vim.api.nvim_buf_get_name(b),
                            modified = vim.bo[b].modified
                        })
                    end
                end
                return {bufs = bufs, current = cur}
            )";
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
            QString lua = R"(
                local mode = vim.api.nvim_get_mode().mode
                local mode_map = {
                    n = "NORMAL", i = "INSERT", v = "VISUAL", V = "V-LINE",
                    ["\22"] = "V-BLOCK", c = "COMMAND", R = "REPLACE",
                    t = "TERMINAL", s = "SELECT", S = "S-LINE",
                }
                local buf = vim.api.nvim_get_current_buf()
                local name = vim.api.nvim_buf_get_name(buf)
                if name == "" then name = "[No Name]" end
                local pos = vim.api.nvim_win_get_cursor(0)
                local encoding = vim.o.fileencoding ~= "" and vim.o.fileencoding or vim.o.encoding
                local branch = ""
                if vim.b.gitsigns_head then branch = " " .. vim.b.gitsigns_head end
                local diag = ""
                local e = #vim.diagnostic.get(buf, {severity = vim.diagnostic.severity.ERROR})
                local w = #vim.diagnostic.get(buf, {severity = vim.diagnostic.severity.WARN})
                if e > 0 or w > 0 then
                    diag = "E:" .. e .. " W:" .. w
                end
                return {
                    mode = mode_map[mode] or mode,
                    file = name,
                    pos = pos[1] .. ":" .. pos[2],
                    encoding = encoding,
                    branch = branch,
                    diagnostics = diag,
                }
            )";
            m_nvim->request("nvim_exec_lua", {lua, QVariantList{}}, [this](const QVariant &result) {
                m_statusline->updateStatus(result.toMap());
            });
        });
        connect(m_grid, &GridWidget::flushed, statusTimer, [statusTimer]() {
            if (!statusTimer->isActive()) statusTimer->start();
        });
    }

    // Minimap: poll buffer content and highlights (throttled)
    {
        auto *minimapTimer = new QTimer(this);
        minimapTimer->setSingleShot(true);
        minimapTimer->setInterval(250);
        connect(minimapTimer, &QTimer::timeout, this, [this]() {
            QString lua = R"(
                local buf = vim.api.nvim_get_current_buf()
                local win = vim.api.nvim_get_current_win()
                local lines = vim.api.nvim_buf_get_lines(buf, 0, -1, false)
                local total = #lines
                local info = vim.fn.getwininfo(win)[1]
                local top = info.topline
                local bot = info.botline
                local cursor = vim.api.nvim_win_get_cursor(win)[1]
                -- Collect syntax highlight per line (first 80 chars)
                local hl_lines = {}
                local max_lines = math.min(total, 2000)
                for i = 1, max_lines do
                    local cols = {}
                    local line = lines[i] or ""
                    local len = math.min(#line, 80)
                    for c = 0, len - 1 do
                        local hl = vim.inspect_pos(buf, i - 1, c)
                        local fg = nil
                        if hl.syntax and #hl.syntax > 0 then
                            local id = vim.fn.synIDtrans(hl.syntax[#hl.syntax].hl_group_link or 0)
                            if id and id > 0 then
                                fg = vim.fn.synIDattr(id, "fg#")
                            end
                        end
                        if not fg or fg == "" then
                            if hl.treesitter and #hl.treesitter > 0 then
                                local ts = hl.treesitter[#hl.treesitter]
                                local hlid = vim.fn.hlID(ts.hl_group)
                                if hlid > 0 then
                                    fg = vim.fn.synIDattr(vim.fn.synIDtrans(hlid), "fg#")
                                end
                            end
                        end
                        table.insert(cols, fg or "")
                    end
                    table.insert(hl_lines, cols)
                end
                return {lines = hl_lines, total = total, top = top, bot = bot, cursor = cursor}
            )";
            m_nvim->request("nvim_exec_lua", {lua, QVariantList{}}, [this](const QVariant &result) {
                QVariantMap data = result.toMap();
                int total = data["total"].toInt();
                int top = data["top"].toInt() - 1;
                int bot = data["bot"].toInt();
                int cursor = data["cursor"].toInt() - 1;
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

                m_minimap->setContent(lines, total);
                m_minimap->setViewport(top, bot - top);
                m_minimap->setCursorLine(cursor);
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
