#pragma once

#include <QMainWindow>
#include <QByteArray>
#include "config.h"
#include "switcher.h"

class GridWidget;
class NvimProcess;
class PopupmenuWidget;
class TablineWidget;
class MessageWidget;
class CmdlineWidget;
class StatuslineWidget;
class BranchSwitcherWidget;
class TerminalPanel;
class MinimapWidget;
class ImageRenderer;
class GitStatus;
class ChangeReviewPanel;
class Session;
class QSplitter;
class QFrame;
class QTimer;
class QPushButton;

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
    /// Refuse Tab focus navigation while a terminal has focus, so Shift+Tab
    /// reaches the shell instead of moving focus out of the panel.
    bool focusNextPrevChild(bool next) override;

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

    /// Open the git branch switcher under the statusline's git segment, or
    /// close it when it is already up (the git segment click and
    /// Ctrl+Shift+B both land here). No-op without a git segment.
    void toggleBranchSwitcher();

    /// Projects declared in neobeam.toml, then the ones pinned from the
    /// switcher, then recent folders and files (deduplicated, stale paths
    /// pruned).
    QList<SwitcherItem> switcherItems() const;

    /// Ctrl+P in the switcher on an unpinned folder: open the switcher's side
    /// command prompt, prefilled with what the side terminal is running.
    void askPinSideCommand(const QString &dir);
    /// The prompt was confirmed: write the pin, then refresh the open switcher
    /// and report what happened in its footer. unpinProject() is the reverse,
    /// and needs no prompt.
    void pinProject(const QString &dir, const QString &sideCommand);
    void unpinProject(const QString &dir);

    /// Point nvim, the bottom terminal splits and the side terminal at the
    /// selected project/folder/file and record it as recently used.
    void applySwitcherSelection(const SwitcherItem &item);

    /// Set the side terminal's exec command to the [[projects]] entry
    /// matching dir, or clear it when dir is not a pinned project.
    void applyProjectSideCommand(const QString &dir);

    /// Track nvim's cwd for the window title (and feed the git segment),
    /// then re-render the title via applyWindowTitle().
    void updateTitleCwd(const QString &cwd);

    /// Compose and set the window title. With [ui] window_title set, renders
    /// that template (%p project, %d/%D cwd, %f/%F file, %% literal).
    /// Otherwise: "Neobeam - ~/path", plain "Neobeam" for an empty/home cwd
    /// (nvim's default startup cwd, not a project), or "<project> - ~/path"
    /// with no "Neobeam" while a switcher-selected project is active (cwd
    /// still in its directory).
    void applyWindowTitle();

    /// Request nvim resize grid 1 to the given cell size, deduped against the
    /// last size actually sent so an unchanged request (e.g. a duplicate
    /// fired by GridWidget's own resizeEvent right after a pre-warmed call)
    /// is a no-op.
    void resizeNvimGrid(int cols, int rows);

    /// Open the change review panel to the right of the side terminal, or
    /// close it when it is already up (the side terminal's review button and
    /// Ctrl+Shift+G both land here).
    void toggleChangeReview();

    /// Which pane Ctrl+Shift+X is currently giving the whole window to.
    enum class MaximizedPane { None, Editor, Terminal, SideTerminal };

    /// Ctrl+Shift+X: give the whole window to the pane that holds focus - the
    /// nvim buffer area, the bottom terminal or the side terminal - by hiding
    /// the others; pressing it again restores exactly the ones that were
    /// actually visible before (a panel the user had already closed stays
    /// closed) along with the splitter proportions they had.
    void toggleMaximizedMode();
    void enterMaximized(MaximizedPane pane);
    void leaveMaximized();

    NvimProcess *m_nvim = nullptr;
    GridWidget *m_grid = nullptr;
    PopupmenuWidget *m_popupmenu = nullptr;
    TablineWidget *m_tabline = nullptr;
    MessageWidget *m_messages = nullptr;
    CmdlineWidget *m_cmdline = nullptr;
    StatuslineWidget *m_statusline = nullptr;
    TerminalPanel *m_terminal = nullptr;
    TerminalPanel *m_sideTerminal = nullptr;
    QPushButton *m_termBtn = nullptr;     // bottom-right toggle, kept in sync with m_terminal
    QPushButton *m_sideTermBtn = nullptr; // bottom-right toggle, kept in sync with m_sideTerminal
    MinimapWidget *m_minimap = nullptr;
    GitStatus *m_gitStatus = nullptr; // push-model git provider for the statusline
    ChangeReviewPanel *m_review = nullptr; // worktree diff panel, right of the side terminal
    QFrame *m_editorArea = nullptr; // border-highlighted when the grid has focus
    ImageRenderer *m_imageRenderer = nullptr;
    QSplitter *m_vsplitter = nullptr; // vertical splitter: editor row vs bottom terminal
    QSplitter *m_hsplitter = nullptr; // horizontal splitter: editor area vs side terminal
    SwitcherWidget *m_switcher = nullptr;
    BranchSwitcherWidget *m_branchSwitcher = nullptr; // git branch picker under the statusline
    Session *m_session = nullptr;
    Config m_cfg;
    int m_lastGridCols = -1;   // last size actually sent via resizeNvimGrid(), to dedupe
    int m_lastGridRows = -1;
    int m_minimapLastTick = 0; // last changedtick seen from Neovim, used to skip highlight rebuilds on scroll
    QTimer *m_minimapViewTimer = nullptr; // cheap viewport poll, per flush
    QTimer *m_minimapScanTimer = nullptr; // debounced content rebuild (treesitter scan)

    bool m_quitting = false;          // window closing; don't restart nvim on exit
    bool m_sideTerminalSuppressed = false; // side terminal kept collapsed at startup (no side
                                           // command); its saved state is not overwritten on close
    bool m_showSwitcherOnReady = false; // open the switcher once nvim attaches
    MaximizedPane m_maximized = MaximizedPane::None; // Ctrl+Shift+X: pane filling the window
    bool m_maxEditorWasVisible = false;       // pane state captured when m_maximized was entered,
    bool m_maxTerminalWasVisible = false;     // so leaving it restores only what was actually open
    bool m_maxSideTerminalWasVisible = false;
    bool m_maxReviewWasVisible = false;
    bool m_maxMessagesWasVisible = false;
    QByteArray m_maxVSplitterState;  // splitter proportions captured alongside, since hiding a
    QByteArray m_maxHSplitterState;  // child hands its space to its siblings for good
    QString m_pendingFolder;          // folder from argv, applied on first nvim ready
    QString m_lastMruFile;            // change gates for the recent-files poll
    QString m_lastMruCwd;
    QString m_titleCwd;               // cwd currently shown in the window title
    QString m_titleProject;           // switcher-selected project name shown in the title
    QString m_titleProjectDir;        // its directory; leaving it drops the name
    QString m_titleFile;              // current buffer's file path, for %f/%F in [ui] window_title
};
