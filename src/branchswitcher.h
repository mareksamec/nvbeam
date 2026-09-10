#pragma once

#include <QElapsedTimer>
#include <QFrame>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <QString>

#include "gitstatus.h"   // GitBranch

class QLabel;
class QLineEdit;
class QListWidget;

/// VSCode-style git branch picker: a small popup anchored under the
/// statusline's git segment, with a filter line above the repository's
/// branches. Typing fuzzy-filters the list; a typed name no branch has adds a
/// "create branch" row that checks the new branch out (`git checkout -b`).
///
/// The widget runs no git itself. It asks GitStatus - which already resolved
/// the repository and owns every git invocation - through branchesRequested()
/// and checkoutRequested(), and is answered through setBranches() and
/// checkoutFinished(). A failed checkout keeps the popup open showing git's
/// own message, since that is usually a refusal the user has to act on.
class BranchSwitcherWidget : public QFrame
{
    Q_OBJECT

public:
    /// `highlightColor` tints the focus border, the selected row and its
    /// text. MainWindow passes [ui] focus_border_color (OneDark blue by
    /// default) rather than the green accent the project switcher uses, so
    /// the two pickers are told apart at a glance.
    ///
    /// `fontFamily` and `fontPointSize` come from [ui] branch_switcher_font
    /// and branch_switcher_font_size: the size applies to the list rows, and
    /// the input line above them renders one point larger. A monospace family
    /// is what lets showRows() line the date column up.
    explicit BranchSwitcherWidget(const QString &highlightColor, const QString &fontFamily,
                                  int fontPointSize, QWidget *parent);

    /// Ask for a fresh branch list and show the popup under `anchor` (the
    /// statusline's git segment; the bottom-left of the window when it is
    /// hidden or null) once that list is in - see reveal() for why it is not
    /// shown right away.
    void open(QWidget *anchor);

    /// Open the popup, or close it when it is already up. Clicking the git
    /// segment and Ctrl+Shift+B both go through here.
    void toggle(QWidget *anchor);

    /// Hide the popup (no checkout).
    void dismiss();

public slots:
    /// Fill the list from GitStatus::branchesReady().
    void setBranches(const QList<GitBranch> &branches, const QString &error);

    /// Result of the requested checkout: close on success, otherwise stay open
    /// with git's message so the user can read why it was refused.
    void checkoutFinished(bool ok, const QString &branch, const QString &error);

signals:
    /// Ask GitStatus for the branch list (on every open, so the list cannot
    /// be stale after a fetch or a branch made in the terminal).
    void branchesRequested();
    /// Check out `branch`, creating it from HEAD first when `create` is true.
    void checkoutRequested(const QString &branch, bool create);
    void dismissed();

protected:
    /// Keeps the popup anchored while the parent window resizes, and dismisses
    /// on focus loss or Esc (filters on the embedded line edit).
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Row index standing for the "create branch" entry rather than for an
    /// entry in m_branches.
    static constexpr int kCreateRow = -1;

    void refilter(const QString &query);
    /// Fill the list widget; `indices` index into m_branches, except for
    /// kCreateRow.
    void showRows(const QList<int> &indices);
    /// Whether the typed text is offerable as a new branch: a non-empty name
    /// without whitespace that no listed branch already has.
    bool canCreate(const QString &name) const;
    /// Width the popup takes at the parent's current size. Shared by
    /// reposition() and showRows(), which sizes the name column from it.
    int popupWidth() const;
    /// Show the popup, at whatever geometry it has been laid out to. Called
    /// when the branch list lands, so the first frame drawn is already the
    /// final size and position: revealing an empty list instead made the
    /// popup appear under the statusline (small enough to fit there) and
    /// then jump above it a few milliseconds later, once the rows arrived
    /// and it no longer fit. The timer is the fallback for a git slow enough
    /// that showing nothing would read as the click having missed.
    void reveal();
    /// Drop an open that has been asked for but not yet revealed.
    void cancelPendingOpen();
    void moveSelection(int delta);
    /// Check out the highlighted row (or create the branch named in the input
    /// when the "create branch" row is highlighted).
    void activateCurrent();
    /// Message line under the list: git errors, progress, why the list is
    /// empty. An empty text hides it.
    void setStatus(const QString &text, bool error = false);
    void reposition();

    QLineEdit *m_input = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_status = nullptr;
    QList<GitBranch> m_branches;      // full list, as GitStatus reported it
    QList<int> m_visible;             // indices into m_branches, filtered order
    QPointer<QWidget> m_anchor;       // git segment the popup hangs under
    bool m_busy = false;              // a checkout is in flight; input is frozen
    QElapsedTimer m_dismissed;        // since the last close, for toggle()
    QTimer m_revealTimer;             // fallback reveal when the branch list is slow
    bool m_pendingOpen = false;       // opened, waiting on the list before showing
    // Which side of the anchor the popup sits on. Re-decided on every layout
    // until the branch list has landed - the empty loading-state layout is
    // too short to choose from - then frozen for the rest of the open, so
    // typing a filter shrinks the popup without sending it hopping across
    // the statusline.
    bool m_placeAbove = false;
    bool m_sideFrozen = false;
};
