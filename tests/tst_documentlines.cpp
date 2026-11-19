#include "documentlines.h"

#include <QSignalSpy>
#include <QTest>
#include <QTextBlock>
#include <QTextDocument>

namespace {

/// The document's blocks as lines, the inverse of what updateDocumentLines()
/// is asked to produce.
QStringList blocksOf(const QTextDocument &document)
{
    QStringList lines;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next())
        lines << block.text();
    return lines;
}

/// Marks every block so a test can tell which ones were rewritten.
void markAllBlocks(QTextDocument &document)
{
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        QTextCursor cursor(block);
        QTextBlockFormat format = block.blockFormat();
        format.setProperty(QTextFormat::UserProperty, true);
        cursor.setBlockFormat(format);
    }
}

bool isMarked(const QTextBlock &block)
{
    return block.blockFormat().boolProperty(QTextFormat::UserProperty);
}

/// Clears the mark; used as the LineFormat so rewritten blocks lose it.
QTextBlockFormat unmark(const QString &, const QTextBlockFormat &current)
{
    QTextBlockFormat format = current;
    format.clearProperty(QTextFormat::UserProperty);
    return format;
}

} // namespace

class TestDocumentLines : public QObject
{
    Q_OBJECT

private slots:
    void producesTheLines_data()
    {
        QTest::addColumn<QStringList>("before");
        QTest::addColumn<QStringList>("after");
        QTest::addColumn<int>("rewritten");

        const QStringList abc{"a", "b", "c"};
        QTest::newRow("identical") << abc << abc << 0;
        QTest::newRow("append") << abc << QStringList{"a", "b", "c", "d", "e"} << 2;
        QTest::newRow("prepend") << abc << QStringList{"x", "a", "b", "c"} << 1;
        QTest::newRow("insert in the middle") << abc << QStringList{"a", "x", "y", "b", "c"} << 2;
        QTest::newRow("replace in the middle") << abc << QStringList{"a", "x", "c"} << 1;
        QTest::newRow("delete in the middle") << abc << QStringList{"a", "c"} << 0;
        QTest::newRow("delete the tail") << abc << QStringList{"a"} << 0;
        QTest::newRow("delete the head") << abc << QStringList{"c"} << 0;
        QTest::newRow("replace the tail") << abc << QStringList{"a", "b", "z"} << 1;
        QTest::newRow("nothing in common, shorter") << abc << QStringList{"x", "y"} << 2;
        QTest::newRow("nothing in common, longer") << abc << QStringList{"w", "x", "y", "z"} << 4;
        QTest::newRow("to empty") << abc << QStringList{QString()} << 1;
        QTest::newRow("from empty") << QStringList{QString()} << abc << 3;
        QTest::newRow("empty list means empty text") << abc << QStringList{} << 1;
        QTest::newRow("repeated lines") << QStringList{"a", "a", "a"}
                                        << QStringList{"a", "a", "a", "a"} << 1;
        QTest::newRow("empty lines kept") << QStringList{"a", "", "b"}
                                          << QStringList{"a", "", "", "b"} << 1;
        // A hunk header whose count moved as lines were appended below it:
        // only the header and the new line are written, not the lines between.
        QTest::newRow("header changed, lines appended")
            << QStringList{"@@ -1,2 +1,3 @@", "+x", "+y"}
            << QStringList{"@@ -1,2 +1,4 @@", "+x", "+y", "+z"} << 2;
        QTest::newRow("two separate in-place changes")
            << QStringList{"a", "b", "c", "d", "e"}
            << QStringList{"a", "B", "c", "D", "e"} << 2;
    }

    void producesTheLines()
    {
        QFETCH(QStringList, before);
        QFETCH(QStringList, after);
        QFETCH(int, rewritten);

        QTextDocument document;
        document.setPlainText(before.join(QLatin1Char('\n')));
        QCOMPARE(updateDocumentLines(&document, after), rewritten);

        const QStringList expected = after.isEmpty() ? QStringList{QString()} : after;
        QCOMPARE(blocksOf(document), expected);
        QCOMPARE(document.toPlainText(), expected.join(QLatin1Char('\n')));
    }

    void leavesSharedBlocksAlone()
    {
        QTextDocument document;
        document.setPlainText(QStringLiteral("a\nb\nc\nd\ne"));
        markAllBlocks(document);

        updateDocumentLines(&document, {"a", "b", "X", "Y", "d", "e"}, unmark);

        // a, b kept; X, Y rewritten; d is the one block past the range, which
        // is reformatted too; e untouched.
        const QList<bool> marked{true, true, false, false, false, true};
        QTextBlock block = document.begin();
        for (int n = 0; n < marked.size(); ++n, block = block.next())
            QCOMPARE(isMarked(block), marked.at(n));
    }

    void formatsEveryRewrittenBlock()
    {
        QTextDocument document;
        document.setPlainText(QStringLiteral("a"));
        updateDocumentLines(&document, {"+x", " y", "-z"},
                            [](const QString &line, const QTextBlockFormat &current) {
            QTextBlockFormat format = current;
            format.setProperty(QTextFormat::UserProperty, line.startsWith(QLatin1Char('+')));
            return format;
        });
        QTextBlock block = document.begin();
        QVERIFY(isMarked(block));
        QVERIFY(!isMarked(block.next()));
        QVERIFY(!isMarked(block.next().next()));
    }

    void reportsOneChangeForTheRewrittenRange()
    {
        QTextDocument document;
        // A document only reports contentsChange once it has a layout, which
        // an editor's document always has.
        document.documentLayout();
        document.setPlainText(QStringLiteral("a\nb\nc"));
        QSignalSpy spy(&document, &QTextDocument::contentsChange);

        updateDocumentLines(&document, {"a", "b", "c", "d"},
                            [](const QString &, const QTextBlockFormat &current) {
            return current;
        });

        // One signal, starting at the end of the shared "c": the shared lines
        // before it are not part of the change a highlighter is told about.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 5);
    }

    void reportsEachInPlaceRunSeparately()
    {
        QTextDocument document;
        document.documentLayout();
        document.setPlainText(QStringLiteral("H1\nx\ny"));
        QSignalSpy spy(&document, &QTextDocument::contentsChange);

        updateDocumentLines(&document, {"H2", "x", "y", "z"},
                            [](const QString &, const QTextBlockFormat &current) {
            return current;
        });

        // The header on its own, then the appended line: "x" (from position 3)
        // and "y" between them are in neither change. The first change may
        // include the header's block separator, which its format sits on.
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QVERIFY(spy.at(0).at(2).toInt() <= 3);
        QCOMPARE(spy.at(1).at(0).toInt(), 6);
    }
};

QTEST_MAIN(TestDocumentLines)
#include "tst_documentlines.moc"
