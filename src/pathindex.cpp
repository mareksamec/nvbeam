#include "pathindex.h"

#include "fuzzy.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QQueue>
#include <QSet>

#include <algorithm>

namespace {

/// The entry's own name: the last part of the path, without a folder's
/// trailing '/'.
QStringView baseName(QStringView path)
{
    if (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    return path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
}

} // namespace

PathIndex pathIndexFromGitLsFiles(const QByteArray &output, int maxEntries)
{
    PathIndex index;
    QSet<QString> folders;
    for (const QByteArray &raw : output.split('\0')) {
        if (raw.isEmpty())
            continue;
        const QString file = QString::fromUtf8(raw);
        // Every folder above the file, outermost first, so a folder is listed
        // before what is inside it.
        for (int slash = file.indexOf(QLatin1Char('/')); slash >= 0;
             slash = file.indexOf(QLatin1Char('/'), slash + 1)) {
            const QString folder = file.left(slash + 1);
            if (folders.contains(folder))
                continue;
            if (index.paths.size() >= maxEntries) {
                index.truncated = true;
                return index;
            }
            folders.insert(folder);
            index.paths << folder;
        }
        if (index.paths.size() >= maxEntries) {
            index.truncated = true;
            return index;
        }
        index.paths << file;
    }
    return index;
}

PathIndex walkPathIndex(const QString &root, const PathIndexLimits &limits,
                        const std::atomic<bool> *cancel)
{
    struct Pending {
        QString relative;  // "" for the root, otherwise ends in '/'
        int depth;         // 0 for the root
    };

    PathIndex index;
    QQueue<Pending> queue;
    queue.enqueue({QString(), 0});
    while (!queue.isEmpty()) {
        if (cancel && cancel->load())
            return index;
        const Pending dir = queue.dequeue();
        QDirIterator it(root + QLatin1Char('/') + dir.relative,
                        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        while (it.hasNext()) {
            it.next();
            const QFileInfo info = it.fileInfo();
            const QString name = info.fileName();
            if (name == QLatin1String(".git"))
                continue;
            if (index.paths.size() >= limits.maxEntries) {
                index.truncated = true;
                return index;
            }
            const bool folder = info.isDir();
            const QString relative = dir.relative + name + (folder ? QStringLiteral("/") : QString());
            index.paths << relative;
            // Listed, but only real, visible folders that are not a repository
            // of their own are walked into.
            if (folder && dir.depth + 1 < limits.maxDepth && !info.isSymLink()
                && !name.startsWith(QLatin1Char('.'))
                && !QFileInfo::exists(info.filePath() + QStringLiteral("/.git")))
                queue.enqueue({relative, dir.depth + 1});
        }
    }
    return index;
}

bool isHiddenPath(const QString &path)
{
    if (path.startsWith(QLatin1Char('.')))
        return true;
    return path.contains(QStringLiteral("/."));
}

QStringList rankPaths(const QStringList &paths, const QString &fragment, int limit)
{
    struct Scored {
        const QString *path;
        int score;
    };
    const bool showHidden = fragment.startsWith(QLatin1Char('.'));
    QList<Scored> scored;
    for (const QString &path : paths) {
        if (!showHidden && isHiddenPath(path))
            continue;
        // The scorer would count a folder's trailing '/' as a boundary; the
        // name without it is what the user is typing.
        const QStringView view = path.endsWith(QLatin1Char('/')) ? QStringView(path).chopped(1)
                                                                  : QStringView(path);
        const int pathScore = fuzzyScore(fragment, view.toString());
        if (pathScore < 0)
            continue;
        const int nameScore = fuzzyScore(fragment, baseName(view).toString());
        scored.append({&path, qMax(nameScore >= 0 ? nameScore * 2 : -1, pathScore)});
    }

    const auto better = [](const Scored &a, const Scored &b) {
        if (a.score != b.score)
            return a.score > b.score;
        if (a.path->size() != b.path->size())
            return a.path->size() < b.path->size();
        return a.path->compare(*b.path, Qt::CaseInsensitive) < 0;
    };
    // Only the best `limit` are shown, so only those need to be in order.
    const qsizetype keep = qMin<qsizetype>(limit, scored.size());
    std::partial_sort(scored.begin(), scored.begin() + keep, scored.end(), better);

    QStringList out;
    out.reserve(keep);
    for (qsizetype i = 0; i < keep; ++i)
        out << *scored.at(i).path;
    return out;
}
