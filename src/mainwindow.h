#pragma once

#include <QMainWindow>
#include "config.h"
#include "switcher.h"

class GridWidget;
class NvimProcess;
class PopupmenuWidget;
class TablineWidget;
class MessageWidget;
class CmdlineWidget;
class StatuslineWidget;
class TerminalPanel;
class MinimapWidget;
class ImageRenderer;
class GitStatus;
class Session;
class QSplitter;
class QFrame;
class QTimer;

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

    /// Show the project switcher with a freshly built item list.
    void openSwitcher();

    /// Pinned projects followed by recent folders and files (deduplicated,
    /// stale paths pruned).
    QList<SwitcherItem> switcherItems() const;

    /// Point nvim, the bottom terminal splits and the side terminal at the
    /// selected project/folder/file and record it as recently used.
    void applySwitcherSelection(const SwitcherItem &item);

    /// Set the side terminal's exec command to the [[projects]] entry
    /// matching dir, or clear it when dir is not a pinned project.
    void applyProjectSideCommand(const QString &dir);

    /// Reflect nvim's cwd in the window title ("Neobeam - ~/path"). The home
    /// directory (nvim's default startup cwd, not a project) and an empty cwd
    /// show the plain "Neobeam" title.
    void updateTitleCwd(const QString &cwd);


    NvimProcess *m_nvim = nullptr;
    GridWidget *m_grid = nullptr;
    PopupmenuWidget *m_popupmenu = nullptr;
    TablineWidget *m_tabline = nullptr;
    MessageWidget *m_messages = nullptr;
    CmdlineWidget *m_cmdline = nullptr;
    StatuslineWidget *m_statusline = nullptr;
    TerminalPanel *m_terminal = nullptr;
    TerminalPanel *m_sideTerminal = nullptr;
    MinimapWidget *m_minimap = nullptr;
    GitStatus *m_gitStatus = nullptr; // push-model git provider for the statusline
    QFrame *m_editorArea = nullptr; // border-highlighted when the grid has focus
    ImageRenderer *m_imageRenderer = nullptr;
    QSplitter *m_vsplitter = nullptr; // vertical splitter: editor area vs terminal
    SwitcherWidget *m_switcher = nullptr;
    Session *m_session = nullptr;
    Config m_cfg;
    int m_minimapLastTick = 0; // last changedtick seen from Neovim, used to skip highlight rebuilds on scroll
    QTimer *m_minimapViewTimer = nullptr; // cheap viewport poll, per flush
    QTimer *m_minimapScanTimer = nullptr; // debounced content rebuild (treesitter scan)

    bool m_quitting = false;          // window closing; don't restart nvim on exit
    bool m_showSwitcherOnReady = false; // open the switcher once nvim attaches
    QString m_pendingFolder;          // folder from argv, applied on first nvim ready
    QString m_lastMruFile;            // change gates for the recent-files poll
    QString m_lastMruCwd;
    QString m_titleCwd;               // cwd currently shown in the window title
};
