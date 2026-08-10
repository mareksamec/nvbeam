#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QList>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QFrame>

#ifdef NEOBEAM_HAS_TERMINAL
#include <qtermwidget.h>
#endif

struct TerminalConfig {
    QString fontFamily = "Monospace";
    int fontSize = 10;
    bool followCwd = true;
    int height = 300;
    QString colorScheme = "Neoblue";
    QString foreground;       // override scheme text color (e.g. "#f8f8f2"); empty = scheme default
    QString background;       // override scheme background color; empty = scheme default
    QString shell;            // empty = system default
    QStringList shellArgs;
    QString execCommand;      // run after shell starts
    bool rightClickPaste = false;  // PuTTY-style right-click paste
    bool copyOnSelect = true;      // copy selection to PRIMARY + CLIPBOARD as it is made
    // Non-alphanumeric characters that count as part of a word for
    // double-click selection. Konsole's default is ":@-./_~"; the query-string
    // characters are added so a double-click grabs a whole URL, which is what
    // openSelectedLink() then works on.
    QString wordCharacters = ":@-./_~?&=%#+";
    bool antialiasing = true;
    QFont::HintingPreference hinting = QFont::PreferDefaultHinting;
    int scrollback = 1000;    // scrollback buffer size in lines (>= 1)
    // Keep each visited project's terminal(s) resident (own shell + scrollback)
    // instead of destroying them on project switch. See adoptProject().
    bool rememberScrollback = false;
    int maxCachedProjects = 5; // LRU cap on resident projects when the above is on (>= 1)
    QColor focusBorderColor = QColor("#61afef"); // 1px border around the focused split
    // Side-panel mode (terminal docked next to the minimap): splits stack
    // vertically and the panel enforces a minimum width instead of height.
    bool sidePanel = false;
};

/// VSCode-like embedded terminal panel with split support
class TerminalPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(const TerminalConfig &cfg, QWidget *parent = nullptr);

    /// Update the working directory (called when nvim cwd changes)
    void setWorkingDirectory(const QString &dir);

    /// Toggle panel visibility
    void toggle();

    /// Whether the panel is currently expanded (visible).
    bool isExpanded() const { return isVisible(); }

    // --- Session save/restore of the split layout ---

    /// Number of terminal splits currently open (>= 1).
    int splitCount() const;
    /// Opaque QSplitter state capturing split proportions.
    QByteArray splitterState() const;
    /// Recreate {count} terminal splits (in addition to the initial one already
    /// present), then optionally reapply saved split proportions.
    void restoreSplits(int count, const QByteArray &splitterState);

    /// Paste system clipboard into the active terminal widget
    void pasteClipboard();

    /// Give keyboard focus to the active terminal widget
    void focusTerminal();

    /// Focus the terminal split at the given index (0-based, in split order).
    /// Returns false if the index is out of range.
    bool focusTerminalAt(int index);

    /// Index (0-based, in split order) of the terminal split that contains
    /// the given widget, or -1 if it is not one of the open terminal splits.
    int indexOfFocused(QWidget *focused) const;

    /// Change the working directory of every open terminal split, regardless
    /// of the followCwd setting (explicit user action via the toolbar button).
    void broadcastCwd(const QString &dir);

    /// Grow (+1) or shrink (-1) the font size of every terminal split in this
    /// panel. The new size is clamped to a sane range and also applies to
    /// splits opened later.
    void changeFontSize(int delta);

    /// Point the panel at a project: dir becomes the working directory and
    /// command the exec command (side terminal: the per-project side_command).
    ///
    /// With [terminal] remember_scrollback off (default): shells that haven't
    /// started yet just get their startup dir/command fixed up. Running shells
    /// are kept and cd'd when neither the old nor the new command is set;
    /// otherwise they are replaced with one fresh shell in dir (typing "cd"
    /// into a running interactive tool would corrupt its input).
    ///
    /// With remember_scrollback on: dir's terminal(s) are kept resident in
    /// memory (see switchProject()) instead of being destroyed, so revisiting
    /// a project reattaches it exactly as left rather than respawning it.
    void adoptProject(const QString &dir, const QString &command);

    /// Open the first link found in the active split's selection. This is the
    /// fallback for links qtermwidget's own URL filter does not detect (TUI
    /// applications that wrap a URL themselves, or links written without a
    /// scheme): select the link text, then trigger this. Returns false when the
    /// selection holds no link.
    bool openSelectedLink();

signals:
    void visibilityChanged(bool visible);
    /// Emitted when the user clicks the "send cwd" toolbar button; MainWindow
    /// resolves the current nvim cwd and calls broadcastCwd() with it.
    void cwdBroadcastRequested();

protected:
    void showEvent(QShowEvent *event) override;
    /// Watches the internal terminal display widgets for the copy-on-select
    /// CLIPBOARD copy (qtermwidget only handles PRIMARY itself; see
    /// createTermWidget()).
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void addTerminal();
    void removeCurrentTerminal();

private:
    /// Background color for the panel chrome (toolbar, splitter, panel
    /// widget): the configured background override, else the scheme's
    /// [Background] color, else black. Keeps areas not covered by the
    /// terminal display from showing through as black bands.
    QColor panelBackgroundColor() const;

#ifdef NEOBEAM_HAS_TERMINAL
    QTermWidget *createTermWidget();
    /// Build the terminal QFont from the current config (family, size,
    /// hinting, antialiasing).
    QFont terminalFont() const;
    /// Color scheme name/path to pass to QTermWidget::setColorScheme(). When
    /// foreground/background overrides are configured, this generates a
    /// derived .colorscheme file in the cache dir and returns its full path;
    /// otherwise it returns the configured scheme name unchanged. The result
    /// is computed once and cached in m_schemeResolved.
    QString effectiveColorScheme();
    /// Wrap a terminal widget in a QFrame so its border is paintable via
    /// stylesheet (QTermWidget itself does its own custom painting and
    /// ignores QSS border properties).
    QFrame *wrapTerminal(QTermWidget *term);
    /// Apply the focus-highlight border to the split containing `focused`
    /// and clear it on every other split.
    void updateFocusHighlight(QTermWidget *focused);
    /// Right-click handler: offers Open/Copy Link when a link was detected
    /// under the cursor, and otherwise falls back to the configured
    /// PuTTY-style paste.
    void showContextMenu(QTermWidget *term, const QPoint &pos);

    /// One cached project's terminal state: its own split layout, terminals
    /// and their wrapper frames. Parked sessions are kept alive off-screen
    /// (reparented under m_parkingLot) rather than destroyed, so their
    /// shells keep running and their scrollback survives switching away.
    struct ProjectSession {
        QSplitter *splitter = nullptr;
        QList<QTermWidget *> terminals;
        QHash<QTermWidget *, QFrame *> wrapperFor;
        QTermWidget *currentTerm = nullptr;
        QString cwd;
        QString runningCommand;
    };

    /// Cache-aware counterpart to adoptProject()'s legacy body, used when
    /// [terminal] remember_scrollback is on: parks the active session (LRU
    /// order in m_lru) and reattaches or creates the target project's
    /// session, then evicts the least-recently-used session(s) beyond
    /// m_cfg.maxCachedProjects.
    void switchProject(const QString &dir, const QString &command);
    /// Destroys parked sessions beyond m_cfg.maxCachedProjects, oldest
    /// (least-recently-used) first. Never touches the active session.
    void evictExcessSessions();

    QTermWidget *m_currentTerm = nullptr;
    QList<QTermWidget *> m_terminals;
    QHash<QTermWidget *, QFrame *> m_wrapperFor;

    QHash<QString, ProjectSession> m_sessions; // parked projects, keyed by QDir::cleanPath(dir)
    QStringList m_lru;                         // most-recently-used key first, including the active one
    QString m_activeKey;                       // key of the session currently attached to m_splitter
    QWidget *m_parkingLot = nullptr;           // hidden holder keeping parked splitters' shells alive off-screen
#endif

    QSplitter *m_splitter = nullptr;
    QToolBar *m_toolbar = nullptr;
    TerminalConfig m_cfg;
    QString m_cwd;
    QString m_runningCommand; // exec command the currently running shells were started with
    QString m_schemeResolved; // cached result of effectiveColorScheme()

    static constexpr int kMinHeight = 80;  // minimum panel height in pixels when resizing
    static constexpr int kMinWidth = 150;  // minimum panel width in side-panel mode
    static constexpr int kMinFontSize = 4;   // font-size clamp for Ctrl+-/Ctrl+=/Ctrl+wheel
    static constexpr int kMaxFontSize = 72;
};
