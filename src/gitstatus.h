#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QFileSystemWatcher>

/// Snapshot of the repository state shown in the statusline git segment.
struct GitStatusInfo {
    bool valid = false;   // false: not a git repo, or git unavailable
    QString branch;
    int staged = 0;       // index changes
    int modified = 0;     // unstaged worktree changes
    int untracked = 0;
    int ahead = 0;        // commits ahead/behind upstream
    int behind = 0;
    // Paths of tracked (staged/modified) and untracked entries, each in the
    // order `git status` reports them, kept separate so the hover tooltip
    // can color them differently. Kept here rather than re-parsed on hover
    // since the parse already has them.
    QStringList changedFiles;
    QStringList untrackedFiles;

    bool operator==(const GitStatusInfo &other) const = default;
};

/// One branch offered by the branch switcher.
struct GitBranch {
    QString name;     // short ref name as git reports it ("main", "origin/feature")
    QString target;   // what to hand `git checkout`: for a remote-only branch this is
                      // the name without the remote prefix, so git creates a local
                      // tracking branch instead of detaching HEAD
    QString detail;   // relative date of the tip commit, shown beside the name
    bool remote = false;
    bool current = false;
};

/// Push-model git status provider for the native statusline. Refreshes only
/// on explicit triggers - cwd changes, nvim buffer writes (rpcnotify from
/// lua/gitstatus.lua), .git changes seen by a file watcher (commits/branch
/// switches made in the embedded terminal or in any other process), and
/// window activation (see MainWindow::eventFilter, which covers the cases a
/// watcher structurally cannot see: no inotify, network mounts, a repo created
/// while the app was unfocused) - never on a poll. All git invocations run
/// asynchronously.
class GitStatus : public QObject
{
    Q_OBJECT

public:
    explicit GitStatus(QObject *parent = nullptr);

    /// Point the provider at a new working directory (nvim's cwd):
    /// re-resolves the repository, rewires the .git watcher and refreshes.
    void setWorkingDirectory(const QString &dir);

public slots:
    /// Request a refresh. Bursts (a save firing BufWritePost plus the index
    /// change seen by the watcher) are debounced into a single git run. When
    /// the directory was not a repository the last time it was resolved, the
    /// resolve is retried instead, so a `git init`/`git clone` into the
    /// current cwd is picked up without waiting for the cwd to change.
    void refresh();

    /// Ask for the repository's branches (branch switcher). Answered
    /// asynchronously with branchesReady(); remote-only branches come first,
    /// followed by the local ones, most recently used first (see
    /// parseBranches for what "used" means).
    void requestBranches();

    /// Check out `branch`, or create it from the current HEAD and switch to it
    /// when `create` is true (`git checkout -b`). Answered asynchronously with
    /// checkoutFinished(); a successful run also triggers a status refresh
    /// instead of waiting for the .git watcher.
    void checkoutBranch(const QString &branch, bool create);

signals:
    /// Emitted when the repository state changed since the last emission.
    /// info.valid is false when the directory is not a git repo (the
    /// statusline hides the segment then).
    void changed(const GitStatusInfo &info);

    /// Emitted specifically when the branch name changed (checkout/switch,
    /// including ones made outside nvim via the .git/HEAD watcher), as
    /// opposed to a status-count-only update. Terminals whose shell prompt
    /// shows the branch (zsh vcs_info, etc.) go stale after this.
    void branchChanged(const QString &branch);

    /// Answer to requestBranches(). `error` is empty on success; it carries
    /// git's own message (or "not a git repository") when the list could not
    /// be produced, so the switcher can say why it is empty.
    void branchesReady(const QList<GitBranch> &branches, const QString &error);

    /// Answer to checkoutBranch(). `error` carries git's stderr when ok is
    /// false - typically the "local changes would be overwritten" refusal,
    /// which the user has to see rather than have swallowed.
    void checkoutFinished(bool ok, const QString &branch, const QString &error);

public:
    // --- Output parsers ---
    // Pure functions over git's own output: no process, no state, no signals.
    // Public so the unit tests can feed them recorded output directly.

    static GitStatusInfo parseStatus(const QString &output);

    /// Last time each branch was checked out, from the HEAD reflog, as a unix
    /// timestamp keyed by short branch name. A branch just created off an old
    /// base has no commit of its own, so its tip date says nothing about how
    /// recently it was worked on; this is what does.
    static QHash<QString, qint64> parseReflogActivity(const QString &output);

    /// Turn `git for-each-ref` output into the switcher's branch list: the
    /// remote branches that have no local counterpart first (checking those
    /// out is what creates the local tracking branch), left in git's own
    /// commit-date order, then the locals, ordered by the more recent of their
    /// tip commit and their last checkout in `activity`.
    static QList<GitBranch> parseBranches(const QString &output,
                                          const QHash<QString, qint64> &activity);

private:
    void resolveGitDir();
    void runStatus();
    /// (Re-)watch the resolved git dir, its refs/heads subdirectory, and the
    /// HEAD and index files inside it. The two directories are what makes the
    /// watch survive: git publishes HEAD, index and every ref by writing a
    /// lockfile and renaming it over the target, so a watch on the file itself
    /// is left pointing at the unlinked old inode, while a directory inode is
    /// never replaced.
    void watchGit();
    /// Handler for both watcher signals: re-arms the watch (see watchGit) and
    /// requests a refresh.
    void rewatchAndRefresh();
    void publish(const GitStatusInfo &info);
    /// Run the branch listing itself, ranking the result with `activity`.
    /// Called once the HEAD reflog has been read (or has failed).
    void runBranchList(const QString &dir, const QHash<QString, qint64> &activity);

    QString m_dir;      // working directory (nvim cwd)
    QString m_gitDir;   // resolved absolute .git dir; empty = not a repo
    QTimer m_debounce;
    QFileSystemWatcher m_watcher;
    bool m_resolving = false;   // a rev-parse is in flight
    bool m_running = false;
    bool m_rerun = false;   // refresh arrived while a run was in flight
    bool m_checkingOut = false; // a checkout is in flight; ignore further requests
    GitStatusInfo m_last;
    bool m_hasLast = false;
};
