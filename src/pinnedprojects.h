#pragma once

#include "config.h"

#include <QList>
#include <QString>

/// Projects pinned from the switcher, stored in AppConfigLocation/projects.toml.
///
/// Deliberately not written into neobeam.toml: that file is user-authored and
/// ships with everything commented out, so rewriting it through a serializer
/// would drop the comments and the hand-made layout (the same reasoning that
/// keeps session state in its own file, see Session). This one is
/// machine-written and uses the same [[projects]] schema, so an entry can be
/// moved into neobeam.toml by copying the block.
namespace PinnedProjects {

/// Absolute path to the pinned-projects file.
QString filePath();

/// Every pinned project, file order. A malformed file reads as empty rather
/// than failing: it is app state, and losing a pin beats refusing to start.
QList<ProjectEntry> load();

/// Add a project, keyed by path. Returns false when the path is already
/// pinned or the file cannot be written.
bool add(const ProjectEntry &project);

/// Drop the project with this path. Returns false when it was not pinned or
/// the file cannot be written.
bool remove(const QString &path);

} // namespace PinnedProjects
