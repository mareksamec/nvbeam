#pragma once

#include <QObject>
#include <QString>
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

    bool operator==(const GitStatusInfo &other) const = default;
};

/// Push-model git status provider for the native statusline. Refreshes only
/// on explicit triggers — cwd changes, nvim buffer writes (rpcnotify from
/// lua/gitstatus.lua), and .git HEAD/index changes seen by a file watcher
/// (commits/branch switches made in the embedded terminal) — never on a
/// poll. All git invocations run asynchronously.
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
    /// change seen by the watcher) are debounced into a single git run.
    void refresh();

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

private:
    void resolveGitDir();
    void runStatus();
    /// (Re-)watch HEAD and index under the resolved git dir. Also called on
    /// every watcher event: git replaces both files atomically (rename over
    /// the old file), which silently drops them from the watcher.
    void watchGit();
    void publish(const GitStatusInfo &info);
    static GitStatusInfo parseStatus(const QString &output);

    QString m_dir;      // working directory (nvim cwd)
    QString m_gitDir;   // resolved absolute .git dir; empty = not a repo
    QTimer m_debounce;
    QFileSystemWatcher m_watcher;
    bool m_running = false;
    bool m_rerun = false;   // refresh arrived while a run was in flight
    GitStatusInfo m_last;
    bool m_hasLast = false;
};
