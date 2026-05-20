#include "mainwindow.h"
#include "gridwidget.h"
#include "nvimprocess.h"
#include "uiwidgets.h"
#include "config.h"
#include <QFontMetrics>
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
    layout->addWidget(m_grid, 1);

    m_cmdline = new CmdlineWidget(cfg.accentColor, central);
    layout->addWidget(m_cmdline);

    m_messages = new MessageWidget(cfg.accentColor, central);
    layout->addWidget(m_messages);

    m_statusline = new StatuslineWidget(cfg.accentColor, central);
    if (cfg.extStatusline)
        layout->addWidget(m_statusline);

    setCentralWidget(central);

    // Popupmenu is a floating child of the grid
    m_popupmenu = new PopupmenuWidget(cfg.accentColor, m_grid);

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

    m_nvim->start(nvimArgs);
}

MainWindow::~MainWindow()
{
    if (m_nvim) {
        m_nvim->stop();
    }
}
