#pragma once

#include <QLoggingCategory>

// Debug trace categories. Disabled by default (QtWarningMsg threshold);
// enable per run with e.g.
//   QT_LOGGING_RULES="nvbeam.minimap.debug=true" nvbeam
// When stderr is not a tty (piped/redirected), Qt may route messages to
// journald instead — add QT_FORCE_STDERR_LOGGING=1 to keep them on stderr.
Q_DECLARE_LOGGING_CATEGORY(nbMinimap)  // nvbeam.minimap — view polls + content scans
Q_DECLARE_LOGGING_CATEGORY(nbRedraw)   // nvbeam.redraw  — per-event redraw stream (very chatty)
Q_DECLARE_LOGGING_CATEGORY(nbMessages) // nvbeam.messages — ext_messages msg_show events reaching the log
Q_DECLARE_LOGGING_CATEGORY(nbTitle)    // nvbeam.title   — window title recomputation (cwd/project changes)
Q_DECLARE_LOGGING_CATEGORY(nbFocus)    // nvbeam.focus   — FocusGained/FocusLost forwarded to nvim
Q_DECLARE_LOGGING_CATEGORY(nbSession)  // nvbeam.session — startup restore decisions (terminal panels)
