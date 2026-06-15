#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QList>
#include <QString>
#include <QStringList>

#ifdef NEOBEAM_HAS_TERMINAL
#include <qtermwidget.h>
#endif

struct TerminalConfig {
    QString fontFamily = "Monospace";
    int fontSize = 10;
    bool followCwd = true;
    int height = 300;
    QString colorScheme = "Linux";
    QString shell;            // empty = system default
    QStringList shellArgs;
    QString execCommand;      // run after shell starts
    bool rightClickPaste = false;  // PuTTY-style right-click paste
    bool antialiasing = true;
    QFont::HintingPreference hinting = QFont::PreferDefaultHinting;
    int scrollback = 1000;    // scrollback buffer size in lines (>= 1)
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

signals:
    void visibilityChanged(bool visible);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void addTerminal();
    void splitHorizontal();
    void removeCurrentTerminal();

private:
#ifdef NEOBEAM_HAS_TERMINAL
    QTermWidget *createTermWidget();
    QTermWidget *m_currentTerm = nullptr;
    QList<QTermWidget *> m_terminals;
#endif

    QSplitter *m_splitter = nullptr;
    QToolBar *m_toolbar = nullptr;
    TerminalConfig m_cfg;
    QString m_cwd;

    static constexpr int kMinHeight = 80; // minimum panel height in pixels when resizing
};
