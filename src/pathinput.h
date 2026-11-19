#pragma once

#include <QString>

// What the switcher's path mode makes of the line the user types: a path that
// may start with "~", may be relative to nvim's cwd, and may restart midway.

/// An absolute path with the home directory shown as "~": "/home/me" is "~",
/// "/home/me/x" is "~/x". Any other path is returned unchanged.
QString collapseHome(const QString &path);

/// Inverse of collapseHome(): "~" and "~/..." become absolute. Anything else,
/// including a relative path, is returned as typed.
QString expandHome(const QString &path);

/// Drop everything before the last point where the text starts a new path, the
/// way the Emacs minibuffer does: a "//" restarts at the root and a "/~/" (or a
/// "/~" at the end) restarts at home. "~/~/Projects" is "~/Projects",
/// "~/code//etc" is "/etc". Text without a restart is returned unchanged.
QString restartPath(const QString &text);

/// The typed text as a path to look at: trimmed, restarted (restartPath()) and
/// with "~" expanded. Still relative when the text is, and a trailing
/// separator is kept, since path mode reads it as "list this directory".
QString expandPathInput(const QString &text);

/// The typed text as an absolute path: expandPathInput(), with an empty text
/// meaning `base` and a relative path anchored at `base` (nvim's cwd) rather
/// than at the process working directory.
QString resolvePath(const QString &text, const QString &base);
