#include "mainwindow.h"
#include "gridwidget.h"
#include "nvimprocess.h"
#include "config.h"
#include <QFontMetrics>
#include <QTimer>
MainWindow::MainWindow(const QStringList &nvimArgs, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("neobeam");
    resize(800, 600);

    Config cfg = Config::load();

    m_grid = new GridWidget(cfg.font(), this);
    setCentralWidget(m_grid);

    m_nvim = new NvimProcess(this);
    connect(m_nvim, &NvimProcess::redraw, m_grid, &GridWidget::handleRedraw);
    connect(m_nvim, &NvimProcess::finished, this, &QMainWindow::close);
    connect(m_grid, &GridWidget::gridSizeReady, this, [this](int rows, int cols) {
        m_nvim->request("nvim_ui_try_resize", {cols, rows});
    });
    connect(m_nvim, &NvimProcess::ready, this, [this]() {
        // Defer attach until event loop has processed the show and we have real geometry
        QTimer::singleShot(0, this, [this]() {
            int cols = m_grid->width() / m_grid->cellWidth();
            int rows = m_grid->height() / m_grid->cellHeight();
            if (cols < 1) cols = 80;
            if (rows < 1) rows = 24;

            QVariantMap opts;
            opts["ext_linegrid"] = true;
            opts["ext_hlstate"] = true;
            m_nvim->request("nvim_ui_attach", {cols, rows, opts});
        });
    });

    connect(m_grid, &GridWidget::inputKey, this, [this](const QString &keys) {
        m_nvim->request("nvim_input", {keys});
    });

    connect(m_grid, &GridWidget::inputMouse, this,
            [this](const QString &button, const QString &action,
                   const QString &mod, int grid, int row, int col) {
        m_nvim->request("nvim_input_mouse", {button, action, mod, grid, row, col});
    });

    m_nvim->start(nvimArgs);
}

MainWindow::~MainWindow()
{
    if (m_nvim) {
        m_nvim->stop();
    }
}
