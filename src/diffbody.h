#pragma once

#include <QList>
#include <QString>

/// What an expanded file in the review panel shows: the lines themselves plus,
/// per line, the number it carries in the file (0 for a line with no number,
/// such as a hunk header).
struct DiffBodyText {
    QString text;
    QList<int> numbers;
};

/// Pass to diffBodyText() as `contextLines` to keep every context line.
constexpr int kAllContextLines = -1;

/// Turn one file's hunks (`body`, the unified diff lines after git's file
/// header) into the text the panel shows.
///
/// Hunk headers, added and removed lines are always kept. An unchanged context
/// line is kept when it is at most `contextLines` lines away from an added or
/// removed line of the same hunk; `0` keeps none of them and kAllContextLines
/// keeps all. Hidden lines are still counted, so the numbers stay true. A
/// "\ No newline at end of file" marker belongs to the line above it: it is
/// shown with that line and carries no number of its own.
DiffBodyText diffBodyText(const QString &body, int contextLines);
