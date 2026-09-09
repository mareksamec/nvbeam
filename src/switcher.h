#pragma once

#include <QFrame>
#include <QList>
#include <QString>

class QLineEdit;
class QListWidget;

/// One entry in the project switcher list.
struct SwitcherItem {
    enum Kind { Project, Folder, File };
    Kind kind = Folder;
    QString label;        // display name (project name or path basename)
    QString path;         // absolute path (folder for Project/Folder, file for File)
    QString sideCommand;  // side terminal command (projects only)
};

/// KRunner-style overlay picker: a search field with a fuzzy-filtered list of
/// pinned projects and recently opened folders/files. Centered over the main
/// window; shown when neobeam starts without arguments, when nvim exits (with
/// switcher.keep_running), or on Ctrl+Shift+P.
///
/// Tab switches the same field between two modes: the project/recent search
/// and a filesystem path mode that completes what is typed against the real
/// directory tree, so any folder or file can be opened without being pinned or
/// recently used.
class SwitcherWidget : public QFrame
{
    Q_OBJECT

public:
    explicit SwitcherWidget(const QString &accentColor, QWidget *parent);

    /// Replace the item set, reset to project mode, recenter and show focused.
    /// `baseDir` is where path mode starts and what relative paths resolve
    /// against (nvim's cwd); empty falls back to the home directory.
    void open(const QList<SwitcherItem> &items, const QString &baseDir = QString());

    /// Hide the switcher (no selection).
    void dismiss();

signals:
    void itemSelected(const SwitcherItem &item);
    void dismissed();

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

    /// Switch modes (Tab). The text does not carry over - a fuzzy query and a
    /// path do not share a syntax - so path mode starts prefilled with the
    /// base directory and project mode starts empty.
    void setMode(Mode mode);
    /// textChanged handler: dispatches to the active mode's filter.
    void refilter(const QString &text);
    void refilterProjects(const QString &query);
    /// Rebuild the candidate list from the directory the typed path names,
    /// matching the part after the last separator (see kMaxPathEntries).
    void refilterPath(const QString &text);
    /// Replace the input with the highlighted entry's path (Right at the end
    /// of the line, path mode only); directories get a trailing separator, so
    /// the list moves on to their contents.
    void completePath();
    /// Fill the list from the active source list; `indices` index into it.
    void showRows(const QList<int> &indices);
    /// The list the visible rows come from: path candidates or the item set.
    const QList<SwitcherItem> &source() const;
    void moveSelection(int delta);
    void selectCurrent();
    void recenter();

    QLineEdit *m_input = nullptr;
    QListWidget *m_list = nullptr;
    QList<SwitcherItem> m_items;      // full item set, switcher order
    QList<SwitcherItem> m_pathItems;  // path-mode candidates, display order
    QList<int> m_visible;             // indices into source(), filtered order
    Mode m_mode = Mode::Projects;
    QString m_baseDir;                // path mode's starting/anchor directory
    bool m_selectionMoved = false;    // user moved off the top row since the
                                      // last filter (path mode Enter rule)
};
