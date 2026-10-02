#include "colorscheme.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

/// Reading a QTermWidget ".colorscheme" file, which is what lets the native
/// panels draw in the terminal's colors.
class TestColorScheme : public QObject
{
    Q_OBJECT

private slots:
    void blendIsLinearBetweenTheTwoColors();
    void findAcceptsAnExplicitPath();
    void findRejectsAMissingExplicitPath();
    void findReturnsNothingForAnUnknownName();
    void loadReadsBackgroundForegroundAndAnsi();
    void loadRejectsMalformedGroups();
    void loadOfAnUnknownSchemeIsInvalid();

private:
    /// Write `body` into a .colorscheme file in a per-test temporary directory
    /// and return its absolute path.
    QString writeScheme(const QByteArray &body);

    QTemporaryDir m_dir;
};

QString TestColorScheme::writeScheme(const QByteArray &body)
{
    const QString path = m_dir.filePath(QStringLiteral("Test.colorscheme"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    file.write(body);
    file.close();
    return path;
}

void TestColorScheme::blendIsLinearBetweenTheTwoColors()
{
    const QColor black(0, 0, 0);
    const QColor white(255, 255, 255);

    QCOMPARE(blendColors(black, white, 0.0), black);
    QCOMPARE(blendColors(black, white, 1.0), white);
    QCOMPARE(blendColors(black, white, 0.5), QColor(127, 127, 127));
    // Each channel is blended on its own.
    QCOMPARE(blendColors(QColor(0, 100, 200), QColor(100, 100, 0), 0.5), QColor(50, 100, 100));
}

void TestColorScheme::findAcceptsAnExplicitPath()
{
    const QString path = writeScheme("[Background]\nColor=0,0,0\n");
    QVERIFY(!path.isEmpty());
    // A name containing a slash is taken as a path and used as-is.
    QCOMPARE(findColorSchemeFile(path), path);
}

void TestColorScheme::findRejectsAMissingExplicitPath()
{
    QVERIFY(findColorSchemeFile(m_dir.filePath(QStringLiteral("nope.colorscheme"))).isEmpty());
}

void TestColorScheme::findReturnsNothingForAnUnknownName()
{
    QVERIFY(findColorSchemeFile(QStringLiteral("NoSuchSchemeHere")).isEmpty());
}

void TestColorScheme::loadReadsBackgroundForegroundAndAnsi()
{
    const QString path = writeScheme(
        "[Background]\nColor=40,42,54\n"
        "[Foreground]\nColor=248,248,242\n"
        "[Color1]\nColor=255,85,85\n"
        "[Color1Intense]\nColor=255,110,103\n"
        "[Color2]\nColor=80,250,123\n");

    const TerminalScheme scheme = loadTerminalScheme(path);

    QVERIFY(scheme.valid);
    QCOMPARE(scheme.background, QColor(40, 42, 54));
    QCOMPARE(scheme.foreground, QColor(248, 248, 242));
    QCOMPARE(scheme.ansi[AnsiRed], QColor(255, 85, 85));
    QCOMPARE(scheme.ansiIntense[AnsiRed], QColor(255, 110, 103));
    QCOMPARE(scheme.ansi[AnsiGreen], QColor(80, 250, 123));
    // Groups the file does not declare stay invalid rather than defaulting.
    QVERIFY(!scheme.ansi[AnsiBlue].isValid());
    QVERIFY(!scheme.ansiIntense[AnsiGreen].isValid());
}

void TestColorScheme::loadRejectsMalformedGroups()
{
    const QString path = writeScheme(
        "[Background]\nColor=40,42\n"            // too few components
        "[Foreground]\nColor=300,0,0\n"          // out of range
        "[Color0]\nColor=black,0,0\n");          // not a number

    const TerminalScheme scheme = loadTerminalScheme(path);

    QVERIFY(scheme.valid);   // the file was found, so the scheme is "loaded"
    QVERIFY(!scheme.background.isValid());
    QVERIFY(!scheme.foreground.isValid());
    QVERIFY(!scheme.ansi[AnsiBlack].isValid());
}

void TestColorScheme::loadOfAnUnknownSchemeIsInvalid()
{
    QVERIFY(!loadTerminalScheme(QStringLiteral("NoSuchSchemeHere")).valid);
}

QTEST_GUILESS_MAIN(TestColorScheme)
#include "tst_colorscheme.moc"
