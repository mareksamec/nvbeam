#pragma once

#include <QColor>
#include <functional>
#include <QString>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPlainTextEdit;

/// Look and behaviour of the change review panel, filled from [review] in
/// neobeam.toml (see Config).
struct ReviewConfig {
    int width = 600;           // startup width of the panel, in pixels
    int refreshMs = 2000;      // poll interval; floored at kMinRefreshMs, 0 = no poll
    QString base;              // revision to diff against; empty = worktree vs HEAD
    QString fontFamily = "Monospace";
    int fontSize = 10;         // points
    int maxLines = 20000;      // cap on rendered diff lines
    QColor added{"#98c379"};   // "+" lines
    QColor removed{"#e06c75"}; // "-" lines
    QColor hunk{"#61afef"};    // "@@" hunk headers
    QColor file{"#c678dd"};    // "diff --git", "index", "---", "+++" header lines
    QColor background{"#1e1e1e"};
    QColor foreground{"#c8c8c8"};
};

/// Colors a unified diff held in a plain-text document. A highlighter rather
/// than rich text on purpose: the document stays plain, so a copied selection
/// pastes as the diff itself, and Qt re-highlights only the blocks that changed.
class DiffHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    DiffHighlighter(const ReviewConfig &cfg, QTextDocument *document);

protected:
    void highlightBlock(const QString &text) override;

private:
    QTextCharFormat m_added;
    QTextCharFormat m_removed;
    QTextCharFormat m_hunk;
    QTextCharFormat m_file;
};

/// Panel showing the changes already written to the worktree: `git diff`
/// against [review] base (HEAD by default) plus the names of untracked files.
///
/// It exists because the IDE bridge's diff is the agent's permission-request
/// surface and therefore never appears in auto-accept or bypass mode, while the
/// changes themselves are on disk regardless of the mode.
class ChangeReviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ChangeReviewPanel(const ReviewConfig &cfg, QWidget *parent = nullptr);

    /// Point the panel at a working directory (nvim's cwd). Refreshes when the
    /// panel is open.
    void setWorkingDirectory(const QString &dir);

public slots:
    /// Re-read the worktree. Bursts collapse: a request arriving while git is
    /// still running is remembered and served by a single extra run.
    void refresh();

signals:
    /// Emitted on show/hide, so the toolbar button that opens the panel can
    /// track its state (mirrors TerminalPanel).
    void visibilityChanged(bool visible);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    /// One `git -C <dir> <args>` run, answered on the UI thread. `ok` is false
    /// when git failed to start or exited non-zero, and `output` then carries
    /// its stderr.
    void runGit(const QStringList &args, std::function<void(bool ok, const QString &output)> done);

    /// Start the diff run (and, with an empty base, the untracked listing that
    /// follows it), unless one is already in flight.
    void collect();

    /// Replace what is on screen, but only when the text actually differs:
    /// an unchanged poll must not move the scrollbar or drop a selection.
    void render(const QString &text);

    /// "12 files, +340 -58" for the header, counted from the rendered diff.
    static QString statsFor(const QString &diff, int untracked);

    ReviewConfig m_cfg;
    QString m_dir;              // working directory the diff is taken in
    QLabel *m_stats = nullptr;  // header summary
    QPlainTextEdit *m_view = nullptr;
    DiffHighlighter *m_highlighter = nullptr;
    QTimer m_poll;
    bool m_running = false;     // a git run is in flight
    bool m_rerun = false;       // a refresh arrived during that run
    QString m_text;             // text currently displayed, compared against on refresh
};
