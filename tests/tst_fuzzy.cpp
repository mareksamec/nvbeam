#include "fuzzy.h"

#include <QTest>

/// fuzzyScore ranks the project and branch pickers, so what is tested here is
/// the ordering it produces, not the absolute numbers it happens to return.
class TestFuzzy : public QObject
{
    Q_OBJECT

private slots:
    void emptyQueryMatchesEverything();
    void requiresEveryCharacterInOrder();
    void isCaseInsensitive();
    void prefersConsecutiveMatches();
    void prefersWordBoundaries();
};

void TestFuzzy::emptyQueryMatchesEverything()
{
    QCOMPARE(fuzzyScore(QString(), QStringLiteral("anything")), 0);
    QCOMPARE(fuzzyScore(QString(), QString()), 0);
}

void TestFuzzy::requiresEveryCharacterInOrder()
{
    QVERIFY(fuzzyScore(QStringLiteral("nvb"), QStringLiteral("nvbeam")) > 0);
    // Present, but in the wrong order.
    QCOMPARE(fuzzyScore(QStringLiteral("bvn"), QStringLiteral("nvbeam")), -1);
    // Not present at all.
    QCOMPARE(fuzzyScore(QStringLiteral("xyz"), QStringLiteral("nvbeam")), -1);
    // Longer than the candidate.
    QCOMPARE(fuzzyScore(QStringLiteral("nvbeams"), QStringLiteral("nvbeam")), -1);
}

void TestFuzzy::isCaseInsensitive()
{
    QCOMPARE(fuzzyScore(QStringLiteral("NVB"), QStringLiteral("nvbeam")),
             fuzzyScore(QStringLiteral("nvb"), QStringLiteral("NVBEAM")));
    QVERIFY(fuzzyScore(QStringLiteral("NVB"), QStringLiteral("nvbeam")) > 0);
}

void TestFuzzy::prefersConsecutiveMatches()
{
    // Same characters, same boundary bonus on the first one; the run wins.
    QVERIFY(fuzzyScore(QStringLiteral("abc"), QStringLiteral("abcxxx"))
            > fuzzyScore(QStringLiteral("abc"), QStringLiteral("axbxcx")));
}

void TestFuzzy::prefersWordBoundaries()
{
    // "sw" on the segment start beats "sw" buried inside a word.
    QVERIFY(fuzzyScore(QStringLiteral("sw"), QStringLiteral("src/switcher.cpp"))
            > fuzzyScore(QStringLiteral("sw"), QStringLiteral("xxswxxxxxxxx.cpp")));
}

QTEST_GUILESS_MAIN(TestFuzzy)
#include "tst_fuzzy.moc"
