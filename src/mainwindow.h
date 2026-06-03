#pragma once

#include <QMainWindow>
#include "config.h"

class GridWidget;
class NvimProcess;
class PopupmenuWidget;
class TablineWidget;
class MessageWidget;
class StatuslineWidget;
class TerminalPanel;
class MinimapWidget;
class ImageRenderer;

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
    PopupmenuWidget *m_popupmenu = nullptr;
    TablineWidget *m_tabline = nullptr;
    MessageWidget *m_messages = nullptr;
    StatuslineWidget *m_statusline = nullptr;
    TerminalPanel *m_terminal = nullptr;
    MinimapWidget *m_minimap = nullptr;
    ImageRenderer *m_imageRenderer = nullptr;
    Config m_cfg;
    int m_minimapLastTick = 0; // last changedtick seen from Neovim, used to skip highlight rebuilds on scroll
};
