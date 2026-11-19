#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <atomic>

// The file list behind the switcher's recursive path search: every file and
// folder below a root directory, as paths relative to it, and how a typed
// fragment ranks them. Folders end in '/', which is how the two are told apart.

/// How much of a tree is collected.
struct PathIndexLimits {
    int maxDepth = 6;        // folder levels the walk descends below the root
    int maxEntries = 50000;  // paths kept, from either source
};

/// A collected file list. `truncated` is set when maxEntries cut it short.
struct PathIndex {
    QStringList paths;
    bool truncated = false;
};

/// The output of `git ls-files -z` (NUL separated paths relative to the root)
/// as a PathIndex: the files plus every folder above them, each folder once and
/// listed before its contents.
PathIndex pathIndexFromGitLsFiles(const QByteArray &output, int maxEntries);

/// Walk the folders below `root`, breadth first, so the shallow entries are the
/// ones kept when maxEntries cuts the walk short. `.git` is skipped; hidden
/// folders, symlinked folders and folders holding a `.git` of their own (nested
/// repositories) are listed but not descended into. `cancel`, when given and
/// set from another thread, stops the walk early.
PathIndex walkPathIndex(const QString &root, const PathIndexLimits &limits,
                        const std::atomic<bool> *cancel = nullptr);

/// True when any part of the relative `path` starts with '.'.
bool isHiddenPath(const QString &path);

/// Up to `limit` entries of `paths` that `fragment` fuzzy matches, best first.
/// A match in the entry's own name counts double, as in project mode, and a
/// match spanning folders ("srcsw" for "src/switcher.cpp") comes from the
/// scorer itself. Hidden paths only take part when the fragment starts with
/// '.'. Equal scores keep shorter paths first, then alphabetical order.
QStringList rankPaths(const QStringList &paths, const QString &fragment, int limit);
