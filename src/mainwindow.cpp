#include "mainwindow.h"
#include "gridwidget.h"
#include "nvimprocess.h"
#include <QFontMetrics>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("neobeam");
    resize(800, 600);

    m_grid = new GridWidget(this);
    setCentralWidget(m_grid);

    m_nvim = new NvimProcess(this);
    connect(m_nvim, &NvimProcess::redraw, m_grid, &GridWidget::handleRedraw);
    connect(m_nvim, &NvimProcess::finished, this, &QMainWindow::close);
    connect(m_grid, &GridWidget::gridSizeReady, this, [this](int rows, int cols) {
        m_nvim->request("nvim_ui_try_resize", {cols, rows});
    });
    connect(m_nvim, &NvimProcess::ready, this, [this]() {
        QFontMetrics fm(m_grid->font());
        int cols = m_grid->width() / fm.horizontalAdvance('M');
        int rows = m_grid->height() / fm.height();
        if (cols < 1) cols = 80;
        if (rows < 1) rows = 24;

        QVariantMap opts;
        opts["ext_linegrid"] = true;
        m_nvim->request("nvim_ui_attach", {cols, rows, opts});
    });

    m_nvim->start();
}

MainWindow::~MainWindow()
{
    if (m_nvim) {
        m_nvim->stop();
    }
}
