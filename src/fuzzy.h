#pragma once

#include <QChar>
#include <QLatin1Char>
#include <QString>

/// Fuzzy subsequence score: every query char must appear in order in the
/// candidate (case-insensitive), or -1. Consecutive matches and matches on
/// word boundaries (start, after separators) score higher, approximating
/// fzf-style ranking without its full algorithm.
///
/// Shared by the pickers that filter a list as the user types (the project
/// switcher and the git branch switcher) so they rank alike.
inline int fuzzyScore(const QString &query, const QString &candidate)
{
    if (query.isEmpty())
        return 0;
    int score = 0;
    int qi = 0;
    bool prevMatched = false;
    for (int ci = 0; ci < candidate.size() && qi < query.size(); ++ci) {
        const QChar c = candidate.at(ci).toLower();
        if (c != query.at(qi).toLower()) {
            prevMatched = false;
            continue;
        }
        score += 2;
        if (prevMatched)
            score += 3;
        const QChar prev = ci > 0 ? candidate.at(ci - 1) : QChar();
        if (ci == 0 || prev == QLatin1Char('/') || prev == QLatin1Char('-')
            || prev == QLatin1Char('_') || prev == QLatin1Char('.') || prev == QLatin1Char(' '))
            score += 5;
        prevMatched = true;
        ++qi;
    }
    return qi == query.size() ? score : -1;
}
