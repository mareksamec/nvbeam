#pragma once

#include <QFrame>
#include <QList>
#include <QString>

class QLabel;
class QLineEdit;
class QListWidget;
class QTimer;

/// One entry in the project switcher list.
struct SwitcherItem {
    enum Kind { Project, Folder, File };
    Kind kind = Folder;
    QString label;        // display name (project name or path basename)
    QString path;         // absolute path (folder for Project/Folder, file for File)
    QString sideCommand;  // side terminal command (projects only)
    bool pinned = false;  // a Project the app itself pinned, so Ctrl+P can
                          // unpin it again; a project declared in neobeam.toml
                          // is not pinned and cannot be removed from here
};

/// KRunner-style overlay picker: a search field with a fuzzy-filtered list of
/// pinned projects and recently opened folders/files. Centered over the main
/// window; shown when neobeam starts without arguments, when nvim exits (with
/// switcher.keep_running), or on Ctrl+Shift+P.
///
/// Ctrl+D switches the same field between two modes: the project/recent search
/// and a filesystem path mode where Tab completes what is typed against the
/// real directory tree, so any folder or file can be opened without being
/// pinned or recently used.
class SwitcherWidget : public QFrame
{
    Q_OBJECT

public:
    explicit SwitcherWidget(const QString &accentColor, QWidget *parent);

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

signals:
    void itemSelected(const SwitcherItem &item);
    void dismissed();
    /// Ctrl+P on a directory that is not pinned yet (the path typed in path
    /// mode, or the highlighted row in project mode).
    void pinRequested(const QString &dir);
    /// Ctrl+P on a directory this app pinned earlier.
    void unpinRequested(const QString &dir);

protected:
    /// Keeps the box centered while the parent window resizes and dismisses
    /// on focus loss or Esc (filters on the embedded line edit).
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// What the text in the input line means.
    enum class Mode {
        Projects,   // fuzzy query over m_items (pinned projects and recents)
        Path        // filesystem path, completed against real directories
    };

    /// Switch modes (Ctrl+D). The text does not carry over - a fuzzy query and
    /// a path do not share a syntax - so path mode starts prefilled with the
    /// base directory and project mode starts empty.
    void setMode(Mode mode);
    /// textChanged handler: dispatches to the active mode's filter.
    void refilter(const QString &text);
    void refilterProjects(const QString &query);
    /// Rebuild the candidate list from the directory the typed path names,
    /// matching the part after the last separator (see kMaxPathEntries).
    void refilterPath(const QString &text);
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
    /// Ctrl+P: pin the directory the switcher is pointing at, or unpin it when
    /// it is already an app-pinned project. Emits nothing for anything that
    /// cannot be a project (a file, a path that does not exist) and for a
    /// project declared in neobeam.toml, which only the user can remove.
    void togglePin();
    /// Footer text: a transient message when one is set, the mode's key hints
    /// otherwise.
    void setStatus(const QString &message = QString());
    void recenter();

    QLineEdit *m_input = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_status = nullptr;       // key hints, and pin feedback
    QTimer *m_statusTimer = nullptr;  // clears a transient message
    QList<SwitcherItem> m_items;      // full item set, switcher order
    QList<SwitcherItem> m_pathItems;  // path-mode candidates, display order
    QList<int> m_visible;             // indices into source(), filtered order
    Mode m_mode = Mode::Projects;
    QString m_baseDir;                // path mode's starting/anchor directory
    bool m_selectionMoved = false;    // user moved off the top row since the
                                      // last filter (path mode Enter rule)
    bool m_completionPending = false; // Tab filled a candidate in that Enter
                                      // has not confirmed yet
};
