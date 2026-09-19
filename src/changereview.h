#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QSyntaxHighlighter>
#ifdef NEOBEAM_HAS_SYNTAX
#include <KSyntaxHighlighting/AbstractHighlighter>
#include <KSyntaxHighlighting/Format>
#endif
#include <QTextCharFormat>
#include <QTimer>
#include <QWidget>
#include <functional>

class QLabel;
class QPlainTextEdit;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

/// Look and behaviour of the change review panel, filled from [review] in
/// neobeam.toml (see Config).
struct ReviewConfig {
    int width = 600;           // startup width of the panel, in pixels
    int refreshMs = 2000;      // poll interval; floored at kMinRefreshMs, 0 = no poll
    int timeoutMs = 10000;     // a git run is killed after this long; 0 = wait forever
    QString base;              // revision to diff against; empty = worktree vs HEAD
    QString fontFamily = "Monospace";
    int fontSize = 10;         // points
    int maxLines = 20000;      // cap on the diff lines kept for one file
    int fileMaxHeight = 600;   // tallest an expanded file body grows before it scrolls itself
    bool showContext = false;  // expand a file straight to the full hunks instead of
                               // the changed lines only
    bool syntax = true;        // color the code inside a diff line by the file's language
                               // (needs a build with KSyntaxHighlighting)
    // Palette, taken from the terminal's color scheme (see reviewPalette() in
    // mainwindow.cpp) so the panel and the side terminal it opens next to are
    // one piece of furniture. The defaults below are only what a build with no
    // readable scheme falls back to.
    QColor background{"#1e1e1e"};
    QColor foreground{"#c8c8c8"};
    QColor added{"#98c379"};   // "+" lines; the scheme's green
    QColor removed{"#e06c75"}; // "-" lines; the scheme's red
    QColor hunk{"#61afef"};    // "@@" hunk headers; the scheme's blue
    QColor file{"#c678dd"};    // file name in a row's header; the scheme's magenta
    QColor dim{"#888888"};     // directories, tags, messages, toolbar text
    QColor selection{"#44475a"};  // selected diff text
    QColor hover{"#2f2f2f"};   // button under the pointer, and a pressed toggle
    QColor rowHeader{"#2a2a2a"};  // band behind a file row's name, separating one file
                                  // from the next
    QColor rowBorder{"#3a3a3a"};  // hairline under that band
};

/// One changed file, as parsed out of `git diff` (or listed as untracked).
struct FileDiff {
    QString path;          // repository-relative path, as git reports it
    QString name;          // basename, shown first in the header row
    QString dir;           // parent directory, shown dimmed next to the name
    QString tag;           // "new", "deleted", "renamed", "untracked"; empty for a plain edit
    int added = 0;
    int removed = 0;
    bool untracked = false;
    QString absolutePath;  // untracked files are read from disk when expanded
    QString body;          // the file's hunks; empty for an untracked file, whose
                           // content is read from disk when it is first expanded
};

/// Colors a unified diff held in a plain-text document. A highlighter rather
/// than rich text on purpose: the document stays plain, so a copied selection
/// pastes as the diff, and Qt re-highlights only the blocks that changed.
///
/// With KSyntaxHighlighting available it also colors the code itself: the
/// added/removed color then colors the marker column only, the text carries the
/// language's own colors, and the line's green or red background is painted by
/// FileCard as a full-width selection behind it. The
/// language is fed the line without its leading +/- marker and the formats it
/// returns are shifted back by that one character, so the document's text is
/// still the diff, character for character.
class DiffHighlighter : public QSyntaxHighlighter
#ifdef NEOBEAM_HAS_SYNTAX
                      , public KSyntaxHighlighting::AbstractHighlighter
#endif
{
    Q_OBJECT

public:
    DiffHighlighter(const ReviewConfig &cfg, QTextDocument *document);

    /// Pick the language definition from a file path. A path with no known
    /// definition (or a build without KSyntaxHighlighting) leaves the diff
    /// coloring on its own.
    void setFileName(const QString &path);

protected:
    void highlightBlock(const QString &text) override;
#ifdef NEOBEAM_HAS_SYNTAX
    /// Called by highlightLine() for each token of the code on the line.
    void applyFormat(int offset, int length,
                     const KSyntaxHighlighting::Format &format) override;
#endif

private:
    ReviewConfig m_cfg;
    QTextCharFormat m_added;
    QTextCharFormat m_removed;
    QTextCharFormat m_hunk;
#ifdef NEOBEAM_HAS_SYNTAX
    int m_offset = 0;               // width of the marker column skipped on that line
#endif
};

/// One file's row in the review panel: a header button that expands to reveal
/// that file's diff. Collapsed by default, the way the Claude Code review pane
/// behaves, so a change touching twenty files is still readable at a glance.
class FileCard : public QWidget
{
    Q_OBJECT

public:
    FileCard(const ReviewConfig &cfg, const FileDiff &file, QWidget *parent = nullptr);

    /// Adopt fresh content for the same path: stats and, when it changed, the
    /// body text. An untouched body is left alone so a selection in it survives.
    void setFile(const FileDiff &file);

    void setExpanded(bool expanded);
    bool isExpanded() const;
    QString path() const { return m_file.path; }

signals:
    /// The user opened or closed this file; the panel remembers it by path so
    /// the state outlives a rebuild of the row.
    void expandedChanged(const QString &path, bool expanded);

protected:
    /// The body wraps, so its height depends on the panel's width.
    void resizeEvent(QResizeEvent *event) override;

private:
    /// Compose what the body shows: the whole hunks with `context` on, only the
    /// added/removed lines (and the hunk headers that locate them) with it off.
    QString bodyText() const;
    /// Put bodyText() into the editor and size the editor to its content.
    void applyBody();
    /// Fit the editor to the wrapped text, capped at ReviewConfig::fileMaxHeight.
    void fitBody();
    /// Paint an added or removed line's background across the editor's whole
    /// width. A character format would stop at the end of the text, so the
    /// stripe is drawn as a full-width extra selection per changed line.
    void paintChangedLines();
    /// Read an untracked file from disk and turn it into added lines.
    void loadUntracked();

    ReviewConfig m_cfg;
    FileDiff m_file;
    QString m_shown;               // body text currently in the editor
    QToolButton *m_header = nullptr;
    QLabel *m_stats = nullptr;
    QToolButton *m_contextBtn = nullptr;
    QPlainTextEdit *m_body = nullptr;
    DiffHighlighter *m_highlighter = nullptr;
    bool m_context = false;        // show unchanged context lines too
    bool m_loaded = false;         // untracked content already read from disk
};

/// Panel showing the changes already written to the worktree: `git diff`
/// against [review] base (HEAD by default) plus the untracked files, one
/// expandable row per file.
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

    /// Open or close every file row at once (the header's ⌄/⌃ buttons).
    void expandAll();
    void collapseAll();

signals:
    /// Emitted on show/hide, so the toolbar button that opens the panel can
    /// track its state (mirrors TerminalPanel).
    void visibilityChanged(bool visible);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    /// One `git -C <dir> <args>` run, answered on the UI thread exactly once:
    /// on exit, on a failure to start, or on ReviewConfig::timeoutMs elapsing,
    /// whichever comes first. `ok` is false for all three failures, and
    /// `output` then carries git's stderr or the reason it was given up on.
    /// Answering once is what keeps m_running from being stranded.
    void runGit(const QStringList &args, std::function<void(bool ok, const QString &output)> done);

    /// Start the diff run (and, with an empty base, the untracked listing that
    /// follows it), unless one is already in flight.
    void collect();

    /// Reconcile the rows on screen with a fresh file list: rows are created,
    /// updated in place or dropped, so an untouched file's row (and any
    /// selection inside it) is left exactly as it was.
    void apply(const QList<FileDiff> &files);

    /// Show a message instead of the file rows (no changes, not a repository,
    /// git failed).
    void showMessage(const QString &text);

    /// Split `git diff` output into one FileDiff per file.
    static QList<FileDiff> parseDiff(const QString &diff, int maxLines);

    ReviewConfig m_cfg;
    QString m_dir;                 // working directory the diff is taken in
    QLabel *m_stats = nullptr;     // header summary
    QLabel *m_message = nullptr;   // shown instead of the rows when there is nothing to show
    QScrollArea *m_scroll = nullptr;
    QWidget *m_list = nullptr;     // row container inside the scroll area
    QVBoxLayout *m_listLayout = nullptr;
    QHash<QString, FileCard *> m_cards;
    QStringList m_order;           // paths currently laid out, in display order
    QSet<QString> m_expanded;      // paths the user opened, remembered across refreshes
    QTimer m_poll;
    QElapsedTimer m_runStarted;    // when the in-flight run began, for the watchdog
    bool m_running = false;        // a git run is in flight
    bool m_rerun = false;          // a refresh arrived during that run
};
