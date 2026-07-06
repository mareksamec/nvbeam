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
class SwitcherWidget : public QFrame
{
    Q_OBJECT

public:
    explicit SwitcherWidget(const QString &accentColor, QWidget *parent);

    /// Replace the item set, clear the query, recenter and show focused.
    void open(const QList<SwitcherItem> &items);

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
    void refilter(const QString &query);
    void moveSelection(int delta);
    void selectCurrent();
    void recenter();

    QLineEdit *m_input = nullptr;
    QListWidget *m_list = nullptr;
    QList<SwitcherItem> m_items;      // full item set, switcher order
    QList<int> m_visible;             // indices into m_items, filtered order
};
