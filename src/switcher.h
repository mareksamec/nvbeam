#pragma once

#include "pathindex.h"
#include "theme.h"

#include <QFrame>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include <atomic>
#include <memory>

class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QThread;
class QTimer;

/// One entry in the project switcher list.
struct SwitcherItem {
    /// CreateFolder is the switcher's own "+ Create folder" row in path mode;
    /// it never leaves the switcher (itemSelected is not emitted for it).
    enum Kind { Project, Folder, File, CreateFolder };
    Kind kind = Folder;
    QString label;        // display name (project name or path basename)
    QString path;         // absolute path (folder for Project/Folder, file for File)
    QStringList sideCommands;  // side terminal commands (projects only)
    bool pinned = false;  // a Project the app itself pinned, so Ctrl+P can
                          // unpin it again; a project declared in nvbeam.toml
                          // is not pinned and cannot be removed from here
};

/// KRunner-style overlay picker: a search field with a fuzzy-filtered list of
/// pinned projects and recently opened folders/files. Centered over the main
/// window; shown when nvbeam starts without arguments, when nvim exits (with
/// switcher.keep_running), or on Ctrl+Shift+P.
///
/// Ctrl+D switches the same field between two modes: the project/recent search
/// and a filesystem path mode where Tab completes what is typed against the
/// real directory tree, so any folder or file can be opened without being
/// pinned or recently used. Path mode can also create the typed folder
/// (Ctrl+N, the folder button in the input, or the "+ Create folder" row).
class SwitcherWidget : public QFrame
{
    Q_OBJECT

public:
    explicit SwitcherWidget(const Theme &theme, QWidget *parent);
    /// Stops a folder walk still running for path mode and waits for it.
    ~SwitcherWidget() override;

    /// Depth and size limits of path mode's recursive search.
    void setPathLimits(const PathIndexLimits &limits);

    /// Replace the item set, reset to project mode, recenter and show focused.
    /// `baseDir` is where path mode starts and what relative paths resolve
    /// against (nvim's cwd); empty falls back to the home directory.
    void open(const QList<SwitcherItem> &items, const QString &baseDir = QString());

    /// Replace the item set of an open switcher, keeping the mode, the typed
    /// text and the highlighted row. Used after a pin changes the list.
    void setItems(const QList<SwitcherItem> &items);

    /// Hide the switcher (no selection).
    void dismiss();

    /// Show a short message in the footer, replacing the key hints until it
    /// times out. What a pin actually did is known by MainWindow, which owns
    /// the store, so it reports the outcome here.
    void showMessage(const QString &message);

    /// Turn the input line into the side command prompt for a pin, prefilled
    /// with what the side terminal is running. Enter pins with whatever the
    /// line holds, Esc abandons the pin and restores the list.
    void askSideCommand(const QString &dir, const QString &suggestion);

signals:
    void itemSelected(const SwitcherItem &item);
    void dismissed();
    /// Ctrl+P on a directory that is not pinned yet (the path typed in path
    /// mode, or the highlighted row in project mode). MainWindow answers with
    /// askSideCommand(), which is what actually opens the prompt: only it
    /// knows what the side terminal is running.
    void pinRequested(const QString &dir);
    /// The side command prompt was confirmed. An empty command means a plain
    /// shell, exactly as an absent side_command does. The prompt covers the
    /// first command only; side_command_2/_3 are hand-written.
    void pinConfirmed(const QString &dir, const QString &sideCommand);
    /// Ctrl+P on a directory this app pinned earlier.
    void unpinRequested(const QString &dir);

protected:
    /// Keeps the box centered while the parent window resizes and dismisses
    /// on focus loss or Esc (filters on the embedded line edit).
    bool eventFilter(QObject *watched, QEvent *event) override;
    /// Closing the switcher abandons the file lists being built for it.
    void hideEvent(QHideEvent *event) override;

private:
    /// What the text in the input line means.
    enum class Mode {
        Projects,   // fuzzy query over m_items (pinned projects and recents)
        Path,       // filesystem path, completed against real directories
        PinCommand  // side command for the folder being pinned
    };

    /// Switch modes (Ctrl+D). The text does not carry over - a fuzzy query and
    /// a path do not share a syntax - so path mode starts prefilled with the
    /// base directory and project mode starts empty.
    void setMode(Mode mode);
    /// textChanged handler: dispatches to the active mode's filter.
    void refilter(const QString &text);
    void refilterProjects(const QString &query);
    /// Rebuild the candidate list from the directory the typed path names,
    /// matching the part after the last separator (see kMaxPathEntries). A
    /// fragment of kRecursiveMinChars or more searches the whole tree below
    /// that directory once its file list is ready (see requestPathIndex()).
    void refilterPath(const QString &text);
    /// Candidates for `fragment` from the file list of `root`, the tree below
    /// it: fills m_pathItems and returns the indices to show.
    QList<int> recursiveRows(const QString &root, const PathIndex &index, const QString &fragment);
    /// Start building the file list of `root`, unless it is ready or being
    /// built: `git ls-files` inside a repository, a folder walk on a worker
    /// thread otherwise. pathIndexReady() takes the result.
    void requestPathIndex(const QString &root);
    void startPathWalk(const QString &root);
    /// Store a finished file list and show it if the input still asks for it.
    /// `generation` tells a result for an earlier opening of the switcher.
    void pathIndexReady(quint64 generation, const QString &root, const PathIndex &index);
    /// Drop every file list and stop the walks building one.
    void resetPathIndexes();
    /// Tab in path mode: put the highlighted candidate in the input, and on
    /// every Tab after that move one row down and put that one in instead.
    /// The candidate list is deliberately not refiltered while cycling, so
    /// the set being cycled cannot change under the user.
    void cycleCompletion();
    /// Write candidate `row` into the input without refiltering (directories
    /// get a trailing separator). Leaves the completion pending: Enter
    /// confirms it, which refilters and so descends into a directory.
    void fillFromRow(int row);
    /// Right at the end of the line: fill and confirm in one step.
    void completePath();
    /// Fill the list from the active source list; `indices` index into it.
    void showRows(const QList<int> &indices);
    /// The list the visible rows come from: path candidates or the item set.
    const QList<SwitcherItem> &source() const;
    void moveSelection(int delta);
    void selectCurrent();
    /// Ctrl+N, the folder button and the "+ Create folder" row. Project mode
    /// switches to path mode so the new folder's path can be typed; path mode
    /// creates the folder the input names and lists it, so Enter opens it and
    /// Ctrl+P pins it through the existing flows.
    void newFolder();
    /// Append the "+ Create folder" row to `indices` (and m_pathItems) when the
    /// typed path `raw` names something that does not exist yet.
    void appendCreateRow(const QString &raw, QList<int> &indices);
    /// Ctrl+P: pin the directory the switcher is pointing at, or unpin it when
    /// it is already an app-pinned project. Emits nothing for anything that
    /// cannot be a project (a file, a path that does not exist) and for a
    /// project declared in nvbeam.toml, which only the user can remove.
    void togglePin();
    /// Footer text: a transient message when one is set, the mode's key hints
    /// otherwise.
    void setStatus(const QString &message = QString());
    /// Leave the side command prompt without pinning, restoring the mode and
    /// the text the prompt interrupted.
    void cancelPin();
    void recenter();

    QLineEdit *m_input = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_status = nullptr;       // key hints, and pin feedback
    QTimer *m_statusTimer = nullptr;  // clears a transient message
    QAction *m_newFolderAction = nullptr; // folder button inside the input
    QList<SwitcherItem> m_items;      // full item set, switcher order
    QList<SwitcherItem> m_pathItems;  // path-mode candidates, display order
    QList<int> m_visible;             // indices into source(), filtered order
    Mode m_mode = Mode::Projects;
    QString m_baseDir;                // path mode's starting/anchor directory
    bool m_selectionMoved = false;    // user moved off the top row since the
                                      // last filter (path mode Enter rule)
    bool m_completionPending = false; // Tab filled a candidate in that Enter
                                      // has not confirmed yet
    QString m_pinDir;                 // folder the side command prompt is for
    Mode m_pinReturnMode = Mode::Projects; // mode the prompt interrupted
    QString m_pinReturnText;          // input text it interrupted

    // Path mode's recursive search: one file list per root directory, kept
    // while the switcher is open.
    PathIndexLimits m_pathLimits;
    QHash<QString, PathIndex> m_pathIndexes;  // ready file lists, by root
    QSet<QString> m_pathIndexing;     // roots whose file list is being built
    quint64 m_pathGeneration = 0;     // bumped by resetPathIndexes()
    std::shared_ptr<std::atomic<bool>> m_cancelWalks =
        std::make_shared<std::atomic<bool>>(false);  // set to stop the walks
    QList<QThread *> m_walks;         // walk threads still running
};
