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
class Session;
class QSplitter;

/// Main application window hosting the Neovim grid
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const Config &cfg, const QStringList &nvimArgs = {}, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    /// App-level filter used to catch Ctrl+Tab before the grid/terminal consume it.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Toggle keyboard focus between the Neovim grid and the terminal panel.
    void toggleGridTerminalFocus();


    NvimProcess *m_nvim = nullptr;
    GridWidget *m_grid = nullptr;
    PopupmenuWidget *m_popupmenu = nullptr;
    TablineWidget *m_tabline = nullptr;
    MessageWidget *m_messages = nullptr;
    StatuslineWidget *m_statusline = nullptr;
    TerminalPanel *m_terminal = nullptr;
    MinimapWidget *m_minimap = nullptr;
    ImageRenderer *m_imageRenderer = nullptr;
    QSplitter *m_vsplitter = nullptr; // vertical splitter: editor area vs terminal
    Session *m_session = nullptr;
    Config m_cfg;
    int m_minimapLastTick = 0; // last changedtick seen from Neovim, used to skip highlight rebuilds on scroll
};
