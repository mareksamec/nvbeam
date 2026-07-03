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
class QFrame;

/// Main application window hosting the Neovim grid
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const Config &cfg, const QStringList &nvimArgs = {}, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    /// App-level filter used to catch Ctrl+Tab and the terminal font-size
    /// keys/wheel before the grid/terminal consume them.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Cycle keyboard focus through the Neovim grid and each terminal split,
    /// in split order, wrapping around. forward=false reverses direction
    /// (Ctrl+Shift+Tab).
    void cycleGridTerminalFocus(bool forward);

    /// The terminal panel (bottom or side) containing the given widget, or
    /// nullptr if the widget is not inside either panel.
    TerminalPanel *terminalPanelFor(QWidget *widget) const;


    NvimProcess *m_nvim = nullptr;
    GridWidget *m_grid = nullptr;
    PopupmenuWidget *m_popupmenu = nullptr;
    TablineWidget *m_tabline = nullptr;
    MessageWidget *m_messages = nullptr;
    StatuslineWidget *m_statusline = nullptr;
    TerminalPanel *m_terminal = nullptr;
    TerminalPanel *m_sideTerminal = nullptr;
    MinimapWidget *m_minimap = nullptr;
    QFrame *m_editorArea = nullptr; // border-highlighted when the grid has focus
    ImageRenderer *m_imageRenderer = nullptr;
    QSplitter *m_vsplitter = nullptr; // vertical splitter: editor area vs terminal
    Session *m_session = nullptr;
    Config m_cfg;
    int m_minimapLastTick = 0; // last changedtick seen from Neovim, used to skip highlight rebuilds on scroll
};
