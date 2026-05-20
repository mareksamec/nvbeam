#pragma once

#include <QMainWindow>

class GridWidget;
class NvimProcess;
struct Config;

/// Main application window hosting the Neovim grid
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const Config &cfg, const QStringList &nvimArgs = {}, QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    NvimProcess *m_nvim = nullptr;
    GridWidget *m_grid = nullptr;
};
