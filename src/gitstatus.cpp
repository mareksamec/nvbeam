#include "gitstatus.h"
#include <QFile>
#include <QProcess>
#include <QRegularExpression>

GitStatus::GitStatus(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(200);
    connect(&m_debounce, &QTimer::timeout, this, &GitStatus::runStatus);

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        watchGit(); // re-add paths git just replaced by rename
        refresh();
    });
}

void GitStatus::setWorkingDirectory(const QString &dir)
{
    if (dir.isEmpty() || dir == m_dir)
        return;
    m_dir = dir;
    m_gitDir.clear();
    const QStringList watched = m_watcher.files();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);
    resolveGitDir();
}

void GitStatus::refresh()
{
    if (!m_debounce.isActive())
        m_debounce.start();
}

void GitStatus::resolveGitDir()
{
    auto *proc = new QProcess(this);
    const QString dir = m_dir;
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, dir](int code, QProcess::ExitStatus) {
        proc->deleteLater();
        if (dir != m_dir) // cwd moved on while resolving
            return;
        if (code == 0) {
            m_gitDir = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
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
    proc->start("git", {"-C", dir, "status", "--porcelain=v1", "-b", "--no-renames"});
}

void GitStatus::watchGit()
{
    if (m_gitDir.isEmpty())
        return;
    const QStringList watched = m_watcher.files();
    for (const QString &name : {QStringLiteral("/HEAD"), QStringLiteral("/index")}) {
        const QString path = m_gitDir + name;
        if (QFile::exists(path) && !watched.contains(path))
            m_watcher.addPath(path);
    }
}

void GitStatus::publish(const GitStatusInfo &info)
{
    if (m_hasLast && info == m_last)
        return;
    m_last = info;
    m_hasLast = true;
    emit changed(info);
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
        if (line.size() < 2)
            continue;
        // Two status letters per entry: X = index, Y = worktree.
        const QChar x = line.at(0);
        const QChar y = line.at(1);
        if (x == QLatin1Char('?')) {
            info.untracked++;
            continue;
        }
        if (x != QLatin1Char(' '))
            info.staged++;
        if (y != QLatin1Char(' '))
            info.modified++;
    }
    return info;
}
