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
    bool antialiasing = true;
    QFont::HintingPreference hinting = QFont::PreferDefaultHinting;
    int scrollback = 1000;    // scrollback buffer size in lines (>= 1)
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

signals:
    void visibilityChanged(bool visible);
    /// Emitted when the user clicks the "send cwd" toolbar button; MainWindow
    /// resolves the current nvim cwd and calls broadcastCwd() with it.
    void cwdBroadcastRequested();

protected:
    void showEvent(QShowEvent *event) override;

public slots:
    void addTerminal();
    void removeCurrentTerminal();

private:
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

    QTermWidget *m_currentTerm = nullptr;
    QList<QTermWidget *> m_terminals;
    QHash<QTermWidget *, QFrame *> m_wrapperFor;
#endif

    QSplitter *m_splitter = nullptr;
    QToolBar *m_toolbar = nullptr;
    TerminalConfig m_cfg;
    QString m_cwd;
    QString m_schemeResolved; // cached result of effectiveColorScheme()

    static constexpr int kMinHeight = 80;  // minimum panel height in pixels when resizing
    static constexpr int kMinWidth = 150;  // minimum panel width in side-panel mode
    static constexpr int kMinFontSize = 4;   // font-size clamp for Ctrl+-/Ctrl+=/Ctrl+wheel
    static constexpr int kMaxFontSize = 72;
};
