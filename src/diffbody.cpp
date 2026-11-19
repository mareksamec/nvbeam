#include "diffbody.h"

#include <QRegularExpression>
#include <QStringList>

#include <limits>

namespace {

enum class LineKind { Hunk, Added, Removed, Context, Marker };

LineKind kindOf(const QString &line)
{
    if (line.startsWith(QLatin1String("@@")))
        return LineKind::Hunk;
    if (line.startsWith(QLatin1Char('+')))
        return LineKind::Added;
    if (line.startsWith(QLatin1Char('-')))
        return LineKind::Removed;
    if (line.startsWith(QLatin1Char('\\')))
        return LineKind::Marker;
    return LineKind::Context;
}

bool isChange(LineKind kind)
{
    return kind == LineKind::Added || kind == LineKind::Removed;
}

} // namespace

DiffBodyText diffBodyText(const QString &body, int contextLines)
{
    // "@@ -old,count +new,count @@": the two numbers the lines after it are
    // counted from.
    static const QRegularExpression hunkHeader(
        QStringLiteral("^@@ -(\\d+)(?:,\\d+)? \\+(\\d+)"));

    const QStringList lines = body.split(QLatin1Char('\n'));
    const int count = lines.size();

    // First pass: what each line is and the number it carries.
    QList<LineKind> kinds(count);
    QList<int> numbers(count, 0);
    int oldNo = 1;
    int newNo = 1;
    for (int i = 0; i < count; ++i) {
        const QString &line = lines.at(i);
        kinds[i] = kindOf(line);
        switch (kinds.at(i)) {
        case LineKind::Hunk:
            if (const QRegularExpressionMatch match = hunkHeader.match(line); match.hasMatch()) {
                oldNo = match.captured(1).toInt();
                newNo = match.captured(2).toInt();
            }
            break;
        case LineKind::Added:
            numbers[i] = newNo++;
            break;
        case LineKind::Removed:
            numbers[i] = oldNo++;
            break;
        case LineKind::Context:
            // A context line exists in both files; its number in the new one
            // is what the reader is looking for. Counted even when it is
            // hidden, which is what keeps the numbers around it true.
            numbers[i] = newNo++;
            ++oldNo;
            break;
        case LineKind::Marker:
            break;
        }
    }

    // Second pass: how far each line is from the nearest change of its hunk,
    // looking up and then down. A hunk header ends the reach of the changes
    // above it.
    constexpr int kFar = std::numeric_limits<int>::max();
    QList<int> distance(count, kFar);
    if (contextLines > 0) {
        int sinceChange = kFar;
        for (int i = 0; i < count; ++i) {
            if (kinds.at(i) == LineKind::Hunk)
                sinceChange = kFar;
            else if (isChange(kinds.at(i)))
                sinceChange = 0;
            else if (sinceChange != kFar)
                ++sinceChange;
            distance[i] = sinceChange;
        }
        int untilChange = kFar;
        for (int i = count - 1; i >= 0; --i) {
            if (kinds.at(i) == LineKind::Hunk)
                untilChange = kFar;
            else if (isChange(kinds.at(i)))
                untilChange = 0;
            else if (untilChange != kFar)
                ++untilChange;
            distance[i] = qMin(distance.at(i), untilChange);
        }
    }

    DiffBodyText out;
    QStringList kept;
    bool previousKept = false;
    for (int i = 0; i < count; ++i) {
        bool keep = false;
        switch (kinds.at(i)) {
        case LineKind::Hunk:
        case LineKind::Added:
        case LineKind::Removed:
            keep = true;
            break;
        case LineKind::Context:
            keep = contextLines == kAllContextLines || distance.at(i) <= contextLines;
            break;
        case LineKind::Marker:
            keep = previousKept;
            break;
        }
        previousKept = keep;
        if (!keep)
            continue;
        kept << lines.at(i);
        out.numbers << numbers.at(i);
    }
    out.text = kept.join(QLatin1Char('\n'));
    return out;
}
