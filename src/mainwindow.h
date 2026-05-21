#pragma once

#include <QMainWindow>
#include "config.h"

class GridWidget;
class NvimProcess;
class CmdlineWidget;
class PopupmenuWidget;
class TablineWidget;
class MessageWidget;
class StatuslineWidget;

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
    CmdlineWidget *m_cmdline = nullptr;
    PopupmenuWidget *m_popupmenu = nullptr;
    TablineWidget *m_tabline = nullptr;
    MessageWidget *m_messages = nullptr;
    StatuslineWidget *m_statusline = nullptr;
    Config m_cfg;
};
