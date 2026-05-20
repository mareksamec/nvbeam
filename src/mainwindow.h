#pragma once

#include <QMainWindow>

class GridWidget;
class NvimProcess;

/// Main application window hosting the Neovim grid
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    NvimProcess *m_nvim = nullptr;
    GridWidget *m_grid = nullptr;
};
