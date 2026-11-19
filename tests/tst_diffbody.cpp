#include "diffbody.h"

#include <QTest>

/// diffBodyText(): which lines of a file's hunks the review panel shows, and
/// the file line number each one carries.
class TestDiffBody : public QObject
{
    Q_OBJECT

private slots:
    void keepsContextNearChanges_data()
    {
        QTest::addColumn<QString>("body");
        QTest::addColumn<int>("contextLines");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QList<int>>("numbers");

        // One change in the middle of six context lines, starting at line 10.
        const QString oneChange = QStringLiteral(
            "@@ -10,7 +10,7 @@\n c1\n c2\n c3\n-old\n+new\n c4\n c5\n c6");

        QTest::newRow("0 keeps the changes only")
            << oneChange << 0
            << QStringLiteral("@@ -10,7 +10,7 @@\n-old\n+new")
            << QList<int>{0, 13, 13};
        QTest::newRow("2 keeps two lines on each side")
            << oneChange << 2
            << QStringLiteral("@@ -10,7 +10,7 @@\n c2\n c3\n-old\n+new\n c4\n c5")
            << QList<int>{0, 11, 12, 13, 13, 14, 15};
        QTest::newRow("all keeps every line")
            << oneChange << kAllContextLines << oneChange
            << QList<int>{0, 10, 11, 12, 13, 13, 14, 15, 16};

        // Two changes four lines apart: with 2 lines of context the gap
        // between them is covered exactly, with 1 its middle line is hidden.
        const QString twoChanges = QStringLiteral(
            "@@ -1,7 +1,7 @@\n+a\n c1\n c2\n c3\n+b");
        QTest::newRow("context of two changes overlaps")
            << twoChanges << 2 << twoChanges
            << QList<int>{0, 1, 2, 3, 4, 5};
        QTest::newRow("gap between two changes")
            << twoChanges << 1
            << QStringLiteral("@@ -1,7 +1,7 @@\n+a\n c1\n c3\n+b")
            << QList<int>{0, 1, 2, 4, 5};

        // A change near the end of one hunk does not reach into the next.
        QTest::newRow("context stops at the hunk header")
            << QStringLiteral("@@ -1,2 +1,2 @@\n c1\n+a\n@@ -20,2 +20,2 @@\n c20\n c21\n+b")
            << 1
            << QStringLiteral("@@ -1,2 +1,2 @@\n c1\n+a\n@@ -20,2 +20,2 @@\n c21\n+b")
            << QList<int>{0, 1, 2, 0, 21, 22};

        // The marker belongs to the line above and has no number of its own,
        // so the line after it keeps the right one.
        QTest::newRow("no-newline marker follows its line")
            << QStringLiteral("@@ -5,2 +5,2 @@\n c5\n-last\n\\ No newline at end of file\n+last")
            << 0
            << QStringLiteral("@@ -5,2 +5,2 @@\n-last\n\\ No newline at end of file\n+last")
            << QList<int>{0, 6, 0, 6};
        QTest::newRow("no-newline marker hidden with its line")
            << QStringLiteral("@@ -5,2 +5,2 @@\n-x\n+x\n c6\n c7\n c8\n\\ No newline at end of file")
            << 1
            << QStringLiteral("@@ -5,2 +5,2 @@\n-x\n+x\n c6")
            << QList<int>{0, 5, 5, 6};

        // An untracked file's body: added lines only, no hunk header.
        QTest::newRow("untracked file")
            << QStringLiteral("+a\n+b") << 2
            << QStringLiteral("+a\n+b") << QList<int>{1, 2};
    }

    void keepsContextNearChanges()
    {
        QFETCH(QString, body);
        QFETCH(int, contextLines);
        QFETCH(QString, text);
        QFETCH(QList<int>, numbers);

        const DiffBodyText out = diffBodyText(body, contextLines);
        QCOMPARE(out.text, text);
        QCOMPARE(out.numbers, numbers);
    }
};

QTEST_MAIN(TestDiffBody)
#include "tst_diffbody.moc"
