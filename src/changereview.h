#pragma once

#include <QBoxLayout>
#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QPlainTextEdit>
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
class QScrollArea;
class QToolButton;
class QVBoxLayout;

/// What the panel compares the worktree against.
enum class DiffMode {
    Unstaged,  // the worktree against HEAD: what has been written but not committed
    Branch     // the merge base with the default branch against the worktree: everything
               // this branch adds, uncommitted work included
};

/// Look and behaviour of the change review panel, filled from [review] in
/// neobeam.toml (see Config).
struct ReviewConfig {
    int width = 600;           // startup width of the panel, in pixels
    int refreshMs = 2000;      // poll interval; floored at kMinRefreshMs, 0 = no poll
    int timeoutMs = 10000;     // a git run is killed after this long; 0 = wait forever
    QString base;              // revision to diff against; empty = worktree vs HEAD
    DiffMode mode = DiffMode::Unstaged;  // comparison the panel opens with
    QString baseBranch;        // branch mode's comparison branch; empty = auto-detect
    QString fontFamily = "Monospace";
    int fontSize = 10;         // points
    int maxLines = 20000;      // cap on the diff lines kept for one file
    int fileMaxHeight = 600;   // tallest an expanded file body grows before it scrolls itself
    bool showContext = false;  // expand a file straight to the full hunks instead of
                               // the changed lines only
    bool imagePreview = true;  // show a changed image as an image instead of git's
                               // "Binary files ... differ" line
    qint64 imageMaxBytes = 10 * 1024 * 1024;  // larger images are not decoded
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
    bool binary = false;   // git could not diff it as text ("Binary files ... differ")
    QString absolutePath;  // read from disk when expanded (untracked files, images)
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

/// The diff of one file, with the line numbers of the file it belongs to shown
/// in a gutter beside it. The numbers live in the gutter rather than in the
/// text so that a selection still copies as a usable diff.
class DiffBody : public QPlainTextEdit
{
    Q_OBJECT

public:
    DiffBody(const ReviewConfig &cfg, QWidget *parent = nullptr);

    /// One entry per line of the document: the number to show beside it, or 0
    /// for a line that has none (a hunk header).
    void setLineNumbers(const QList<int> &numbers);

    /// Width the gutter needs for the widest number it has to show.
    int gutterWidth() const;

    /// Draw the numbers of the lines currently on screen. Called by the gutter
    /// widget, which has no state of its own.
    void paintGutter(QPaintEvent *event);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    /// Reserve the gutter's width on the left of the viewport and place the
    /// gutter widget in it.
    void layOutGutter();

    ReviewConfig m_cfg;
    QWidget *m_gutter = nullptr;
    QList<int> m_numbers;
    int m_digits = 1;   // widest number currently shown, in digits
    int m_margin = -1;  // gutter width last reserved in the viewport margin
};

/// One version of an image, drawn scaled to fit over a checkerboard so a
/// transparent PNG reads on any scheme. A painted widget rather than a QLabel
/// with a scaled pixmap: the checkerboard has to go behind the image, and
/// rescaling on every resize of the panel would mean rebuilding that pixmap
/// anyway.
class ImagePane : public QWidget
{
    Q_OBJECT

public:
    ImagePane(const ReviewConfig &cfg, QWidget *parent = nullptr);

    /// Show `image`, or, with a null image, `note` in the dim color (a missing
    /// side of the comparison, an undecodable file, one over image_max_bytes).
    void setImage(const QImage &image, const QString &note = QString());
    QSize imageSize() const { return m_image.size(); }
    bool hasImage() const { return !m_image.isNull(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    ReviewConfig m_cfg;
    QImage m_image;
    QString m_note;
};

/// A changed image, old version beside new one, each captioned with its
/// dimensions and byte size. Used by FileCard in place of the diff editor when
/// the file is an image git could not diff as text.
class ImagePreview : public QWidget
{
    Q_OBJECT

public:
    ImagePreview(const ReviewConfig &cfg, QWidget *parent = nullptr);

    /// Fill one side. `bytes` empty means that side does not exist (a new or a
    /// deleted file), which the pane says rather than showing an empty frame.
    /// `note` replaces the image when the bytes cannot or must not be decoded.
    void setOld(const QByteArray &bytes, const QString &note = QString());
    void setNew(const QByteArray &bytes, const QString &note = QString());

    /// Height the two panes need, capped at ReviewConfig::fileMaxHeight.
    int preferredHeight() const;

signals:
    /// The panes switched between side by side and stacked, so the height they
    /// need has changed.
    void layoutChanged();

protected:
    /// Two panes side by side need width; below kStackWidth they stack.
    void resizeEvent(QResizeEvent *event) override;

private:
    /// Decode into `pane` and caption it, or explain why there is no image.
    void fill(ImagePane *pane, QLabel *caption, const QByteArray &bytes, const QString &note,
              const QString &absent);
    /// The line under the panes: what changed dimensionally, when anything did.
    void updateSummary();

    ReviewConfig m_cfg;
    QBoxLayout *m_panes = nullptr;
    ImagePane *m_oldPane = nullptr;
    ImagePane *m_newPane = nullptr;
    QLabel *m_oldCaption = nullptr;
    QLabel *m_newCaption = nullptr;
    QLabel *m_summary = nullptr;
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

    /// True when a refresh brought new content for an open row whose body has
    /// not been rebuilt yet. Rebuilding costs time proportional to the diff, so
    /// the panel only spends it on rows the user can actually see.
    bool needsRender() const { return m_pendingBody; }

    /// Rebuild the body queued by the last setFile(). No-op when nothing is
    /// pending.
    void renderPending();
    QString path() const { return m_file.path; }

    /// Hand this row the version of its image that lives in the comparison's
    /// base revision, as fetched by the panel (empty bytes = git had none).
    void setOldImage(const QByteArray &bytes);

signals:
    /// The user opened or closed this file; the panel remembers it by path so
    /// the state outlives a rebuild of the row.
    void expandedChanged(const QString &path, bool expanded);

    /// An image row was opened and needs its base-revision version. Only the
    /// panel can fetch it: it owns the git plumbing and knows the base.
    void oldImageRequested(const QString &path);

protected:
    /// The body wraps, so its height depends on the panel's width.
    void resizeEvent(QResizeEvent *event) override;

private:
    /// What the body shows: the lines themselves plus, per line, the number it
    /// carries in the file. With `context` off only the added/removed lines
    /// (and the hunk headers that locate them) are kept, but the hidden lines
    /// are still counted, so the numbers stay true.
    struct BodyText {
        QString text;
        QList<int> numbers;
    };
    BodyText bodyText() const;
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
    /// True when this row should show an image instead of a diff: the build can
    /// decode the suffix, git treats the file as binary (or it is untracked),
    /// and [review] image_preview is on.
    bool showsImage() const;
    /// Build the preview on first use, fill its new side from disk and ask the
    /// panel for the old one.
    void applyPreview();
    /// Give the preview the height its images need, once per layout pass.
    void fitPreview();

    ReviewConfig m_cfg;
    FileDiff m_file;
    QString m_shown;               // body text currently in the editor
    QToolButton *m_header = nullptr;
    QLabel *m_stats = nullptr;
    QToolButton *m_contextBtn = nullptr;
    DiffBody *m_body = nullptr;
    ImagePreview *m_preview = nullptr;  // built on first use, images only
    bool m_oldImageAsked = false;  // the panel has been asked for this version
    DiffHighlighter *m_highlighter = nullptr;
    bool m_context = false;        // show unchanged context lines too
    bool m_loaded = false;         // untracked content already read from disk
    bool m_pendingBody = false;    // new content waiting for renderPending()
    bool m_fitting = false;        // fitBody()/fitPreview() is on the stack
    int m_fitted = -1;             // body height fitBody() last applied
    int m_fittedPreview = -1;      // preview height fitPreview() last applied
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

    /// Refresh after the repository itself moved (a commit, a branch switch, a
    /// fetch). Unlike refresh() this also forgets the resolved default branch,
    /// which any of those can change.
    void repositoryChanged();

    /// Switch between the worktree and the branch comparison and refresh at
    /// once. Does nothing while an explicit [review] base pins the comparison.
    void toggleMode();

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

    /// The same run, answered with git's bytes undecoded. `git show` of an
    /// image blob has to come back this way: decoding it as UTF-8 would
    /// corrupt it.
    void runGitRaw(const QStringList &args, std::function<void(bool ok, const QByteArray &output)> done);

    /// Fetch `path` as it is in the comparison's base revision and hand it to
    /// that path's row. An untracked file (or a failing git) answers with no
    /// bytes, which the row shows as "added".
    void fetchOldImage(const QString &path);

    /// Start the diff run (and, when the comparison ends at the worktree, the
    /// untracked listing that follows it), unless one is already in flight.
    /// Branch mode resolves its base first, so one collect() can be a chain of
    /// git runs; m_running covers the whole chain, not each link.
    void collect();

    /// Run `git diff <base>` and show the result. `label` is what the empty
    /// state calls the other side of the comparison ("HEAD", a branch name).
    /// Untracked files are listed only when the diff ends at the worktree,
    /// which an explicit [review] base is the one case that does not.
    void collectDiff(const QString &base, const QString &label, bool includeUntracked);

    /// Answer with the branch to compare against in branch mode: [review]
    /// base_branch when set, then what the remote calls its default branch,
    /// then main/master. Cached per working directory.
    void resolveBaseBranch(std::function<void(bool ok, const QString &branch)> done);

    /// Walk the branch candidates from `index` on, answering with the first
    /// that names a commit.
    void probeBranch(int index, std::function<void(bool ok, const QString &branch)> done);

    /// End a collect() that cannot produce a diff: shows `message`, clears the
    /// rows and releases the one-run-at-a-time guard.
    void failCollect(const QString &stats, const QString &message);

    /// The comparison actually in force. An explicit [review] base pins it to
    /// the worktree path, since the mode has nothing left to choose.
    DiffMode effectiveMode() const;

    /// Label the mode button with the active comparison.
    void updateModeButton();

    /// Reconcile the rows on screen with a fresh file list: rows are created,
    /// updated in place or dropped, so an untouched file's row (and any
    /// selection inside it) is left exactly as it was.
    void apply(const QList<FileDiff> &files);

    /// Show a message instead of the file rows (no changes, not a repository,
    /// git failed).
    void showMessage(const QString &text);

    /// Split `git diff` output into one FileDiff per file.
    static QList<FileDiff> parseDiff(const QString &diff, int maxLines);

    /// Rebuild the bodies of the open rows that the viewport shows (plus a
    /// screenful of margin), and leave the rest pending until they scroll in.
    /// Called after every refresh, on scrolling and when a row is opened.
    void renderVisibleRows();

    ReviewConfig m_cfg;
    QString m_dir;                 // working directory the diff is taken in
    DiffMode m_mode = DiffMode::Unstaged;  // comparison in force (see effectiveMode)
    QString m_resolvedBranch;      // default branch found for m_dir; empty = not resolved yet
    QString m_compareLabel;        // what the current comparison is against, for the empty state
    QString m_compareBase;         // the revision behind that label, for `git show <base>:<path>`
    QToolButton *m_modeBtn = nullptr;  // worktree/branch toggle in the toolbar
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
    bool m_rendering = false;      // renderVisibleRows() is on the stack
    bool m_running = false;        // a git run is in flight
    bool m_rerun = false;          // a refresh arrived during that run
};
