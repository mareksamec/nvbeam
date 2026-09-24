#include "gitstatus.h"
#include <utility>
#include <QFile>
#include <QProcess>
#include <QSet>
#include <QRegularExpression>

GitStatus::GitStatus(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(200);
    connect(&m_debounce, &QTimer::timeout, this, &GitStatus::runStatus);

    // Both signals mean the same thing here: something under the git dir
    // moved. directoryChanged is the reliable half (git renames files into
    // place, which kills a file watch), fileChanged is kept because it fires
    // for in-place rewrites too.
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &GitStatus::rewatchAndRefresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &GitStatus::rewatchAndRefresh);
}

void GitStatus::setWorkingDirectory(const QString &dir)
{
    if (dir.isEmpty() || dir == m_dir)
        return;
    m_dir = dir;
    m_gitDir.clear();
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);
    resolveGitDir();
}

void GitStatus::refresh()
{
    // A directory that was not a repository has no git dir and therefore no
    // watch: runStatus() would return immediately, forever. Retry the resolve
    // so a repository created after the cwd was first seen is picked up.
    if (m_gitDir.isEmpty()) {
        if (!m_dir.isEmpty())
            resolveGitDir();
        return;
    }
    if (!m_debounce.isActive())
        m_debounce.start();
}

void GitStatus::rewatchAndRefresh()
{
    watchGit();
    refresh();
}

void GitStatus::resolveGitDir()
{
    if (m_resolving) // refresh() retries on every activation while non-repo
        return;
    m_resolving = true;
    auto *proc = new QProcess(this);
    const QString dir = m_dir;
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, dir](int code, QProcess::ExitStatus) {
        proc->deleteLater();
        m_resolving = false;
        if (dir != m_dir) // cwd moved on while resolving
            return;
        const QString gitDir = code == 0
            ? QString::fromUtf8(proc->readAllStandardOutput()).trimmed() : QString();
        // An empty path on a zero exit would leave refresh() retrying the
        // resolve forever, one process per call.
        if (!gitDir.isEmpty()) {
            m_gitDir = gitDir;
            watchGit();
            refresh();
        } else {
            publish(GitStatusInfo{}); // not a repo: hide the segment
        }
    });
    // FailedToStart (git not installed) never reaches finished().
    connect(proc, &QProcess::errorOccurred, this,
            [this, proc](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        m_resolving = false;
        publish(GitStatusInfo{});
    });
    proc->start("git", {"-C", dir, "rev-parse", "--absolute-git-dir"});
}

void GitStatus::runStatus()
{
    if (m_dir.isEmpty() || m_gitDir.isEmpty())
        return;
    if (m_running) {
        m_rerun = true;
        return;
    }
    m_running = true;
    auto *proc = new QProcess(this);
    const QString dir = m_dir;
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, dir](int code, QProcess::ExitStatus) {
        proc->deleteLater();
        m_running = false;
        if (m_rerun) {
            m_rerun = false;
            refresh();
        }
        if (dir != m_dir)
            return;
        if (code == 0)
            publish(parseStatus(QString::fromUtf8(proc->readAllStandardOutput())));
        else
            publish(GitStatusInfo{});
    });
    connect(proc, &QProcess::errorOccurred, this,
            [this, proc](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        m_running = false;
        publish(GitStatusInfo{});
    });
    // --no-optional-locks: plain `git status` may refresh the on-disk index,
    // which the watcher would see as a change and answer with another status
    // run. Same reason editors and shell prompts set GIT_OPTIONAL_LOCKS=0.
    proc->start("git", {"-C", dir, "--no-optional-locks",
                        "status", "--porcelain=v1", "-b", "--no-renames"});
}

void GitStatus::watchGit()
{
    if (m_gitDir.isEmpty())
        return;
    // Re-add unconditionally rather than skipping paths the watcher still
    // lists. After git renames a new file over HEAD or index, the path
    // resolves again while the watch behind it points at the unlinked old
    // inode, so "already watched" is not the same as "still working". Qt
    // ignores a duplicate addPath, and dropping a live watch for one event
    // costs nothing.
    m_watcher.removePaths(m_watcher.files() + m_watcher.directories());
    for (const QString &name : {QStringLiteral(""),          // the git dir itself
                                QStringLiteral("/refs/heads"),
                                QStringLiteral("/HEAD"),
                                QStringLiteral("/index")}) {
        const QString path = m_gitDir + name;
        if (QFile::exists(path))
            m_watcher.addPath(path); // Qt picks file vs directory from the path
    }
}

void GitStatus::publish(const GitStatusInfo &info)
{
    if (m_hasLast && info == m_last)
        return;
    const bool branchSwitched = m_hasLast && m_last.valid && info.valid
                                 && info.branch != m_last.branch;
    m_last = info;
    m_hasLast = true;
    emit changed(info);
    if (branchSwitched)
        emit branchChanged(info.branch);
}

GitStatusInfo GitStatus::parseStatus(const QString &output)
{
    GitStatusInfo info;
    info.valid = true;
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        // Header: "## main...origin/main [ahead 1, behind 2]",
        // "## HEAD (no branch)" (detached), "## No commits yet on main".
        if (line.startsWith(QLatin1String("## "))) {
            const QString rest = line.mid(3);
            if (rest.startsWith(QLatin1String("No commits yet on "))) {
                info.branch = rest.mid(18);
            } else if (rest.startsWith(QLatin1String("HEAD "))) {
                info.branch = QStringLiteral("detached");
            } else {
                int end = rest.indexOf(QLatin1String("..."));
                if (end < 0)
                    end = rest.indexOf(QLatin1Char(' '));
                info.branch = end < 0 ? rest : rest.left(end);
            }
            static const QRegularExpression aheadRe(QStringLiteral("\\[ahead (\\d+)"));
            static const QRegularExpression behindRe(QStringLiteral("behind (\\d+)\\]"));
            const auto ahead = aheadRe.match(rest);
            if (ahead.hasMatch())
                info.ahead = ahead.captured(1).toInt();
            const auto behind = behindRe.match(rest);
            if (behind.hasMatch())
                info.behind = behind.captured(1).toInt();
            continue;
        }
        if (line.size() < 4)
            continue;
        // Two status letters per entry: X = index, Y = worktree, then a
        // space, then the path (quoted by git itself if it needs escaping -
        // left as-is here rather than unescaped, see parseStatus() callers).
        const QChar x = line.at(0);
        const QChar y = line.at(1);
        if (x == QLatin1Char('?')) {
            info.untracked++;
            info.untrackedFiles << line.mid(3);
            continue;
        }
        info.changedFiles << line.mid(3);
        if (x != QLatin1Char(' '))
            info.staged++;
        if (y != QLatin1Char(' '))
            info.modified++;
    }
    return info;
}

/// Fields requested per ref, tab-separated: the "*" marker for the checked-out
/// branch, the full refname (which is what tells local and remote apart), the
/// short name shown in the list, and the tip commit's relative date. Tabs
/// cannot appear in a refname, so splitting on them is unambiguous.
static const QString kBranchFormat =
    QStringLiteral("%(HEAD)%09%(refname)%09%(refname:short)%09%(committerdate:relative)");

void GitStatus::requestBranches()
{
    if (m_dir.isEmpty() || m_gitDir.isEmpty()) {
        emit branchesReady({}, QStringLiteral("Not a git repository"));
        return;
    }
    auto *proc = new QProcess(this);
    const QString dir = m_dir;
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, dir](int code, QProcess::ExitStatus) {
        proc->deleteLater();
        if (dir != m_dir) // cwd moved on while listing
            return;
        if (code == 0) {
            emit branchesReady(parseBranches(QString::fromUtf8(proc->readAllStandardOutput())),
                               QString());
        } else {
            emit branchesReady({}, QString::fromUtf8(proc->readAllStandardError()).trimmed());
        }
    });
    connect(proc, &QProcess::errorOccurred, this,
            [this, proc](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        emit branchesReady({}, QStringLiteral("git is not available"));
    });
    // for-each-ref reads refs only - no index refresh, so unlike `git status`
    // it needs no --no-optional-locks and cannot wake the .git watcher.
    proc->start("git", {"-C", dir, "for-each-ref", "--sort=-committerdate",
                        "--format=" + kBranchFormat, "refs/heads", "refs/remotes"});
}

void GitStatus::checkoutBranch(const QString &branch, bool create)
{
    if (branch.isEmpty() || m_checkingOut)
        return;
    if (m_dir.isEmpty() || m_gitDir.isEmpty()) {
        emit checkoutFinished(false, branch, QStringLiteral("Not a git repository"));
        return;
    }
    m_checkingOut = true;
    auto *proc = new QProcess(this);
    const QString dir = m_dir;
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, branch](int code, QProcess::ExitStatus) {
        proc->deleteLater();
        m_checkingOut = false;
        const bool ok = code == 0;
        // git reports both refusals and the "Switched to branch" note on
        // stderr, so it is only read when the run actually failed.
        const QString error = ok
            ? QString() : QString::fromUtf8(proc->readAllStandardError()).trimmed();
        if (ok)
            refresh();   // don't wait for the .git watcher to notice HEAD moved
        emit checkoutFinished(ok, branch, error);
    });
    connect(proc, &QProcess::errorOccurred, this,
            [this, proc, branch](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        m_checkingOut = false;
        emit checkoutFinished(false, branch, QStringLiteral("git is not available"));
    });
    // `checkout` rather than the newer `switch`: it is what every supported
    // git has, and checking out a remote-only name (see GitBranch::target)
    // creates the local tracking branch through the same DWIM rule.
    QStringList args{"-C", dir, "checkout"};
    if (create)
        args << QStringLiteral("-b");
    args << branch;
    proc->start("git", args);
}

QList<GitBranch> GitStatus::parseBranches(const QString &output)
{
    QList<GitBranch> locals;
    QList<GitBranch> remotes;
    QSet<QString> localNames;

    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList fields = line.split(QLatin1Char('\t'));
        if (fields.size() < 4)
            continue;
        GitBranch branch;
        branch.current = fields.at(0).trimmed() == QLatin1String("*");
        const QString refName = fields.at(1);
        branch.remote = refName.startsWith(QLatin1String("refs/remotes/"));
        branch.name = fields.at(2);
        branch.detail = fields.at(3);
        if (branch.remote) {
            // "refs/remotes/origin/HEAD" is a symref standing for the remote's
            // default branch, which is listed under its own name anyway.
            // Matched on the full refname: git shortens that particular ref to
            // the bare remote name ("origin"), not to "origin/HEAD".
            if (refName.endsWith(QLatin1String("/HEAD")))
                continue;
            // "origin/feature/x" -> "feature/x": the name to hand checkout,
            // which git resolves to the remote branch and tracks.
            const int slash = branch.name.indexOf(QLatin1Char('/'));
            if (slash < 0)
                continue;
            branch.target = branch.name.mid(slash + 1);
            remotes.append(branch);
        } else {
            branch.target = branch.name;
            localNames.insert(branch.name);
            locals.append(branch);
        }
    }

    // Remote-only branches lead the list: one that has just appeared - pushed
    // from another machine or made in the forge's web UI - is what the user is
    // most likely reaching for, and it has no local row to be found under.
    // Within each group git's own commit-date order is kept.
    //
    // A remote branch that already has a local counterpart would check out the
    // very same local branch, so only the local row is offered.
    QList<GitBranch> result;
    result.reserve(locals.size() + remotes.size());
    for (const GitBranch &branch : std::as_const(remotes)) {
        if (!localNames.contains(branch.target))
            result.append(branch);
    }
    result.append(locals);
    return result;
}
