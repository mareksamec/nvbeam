#include "session.h"

#include <QFile>
#include <QStandardPaths>
#include <QTest>

/// session.ini: window geometry, terminal layout and the recent lists. The
/// point of these tests is that what the app writes on shutdown is what it
/// reads back on the next start, so each case goes through two Session
/// objects. QStandardPaths test mode keeps it out of the real config dir.
class TestSession : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void geometryAndSplittersRoundTrip();
    void terminalStateIsAbsentUntilWritten();
    void terminalStateRoundTrips();
    void recentListIsMostRecentFirst();
    void recentListMovesARepeatToTheFront();
    void recentListIsTrimmedToTheLimit();
    void recentListIgnoresAnEmptyPath();
};

void TestSession::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestSession::init()
{
    QFile::remove(Session::filePath());
}

void TestSession::geometryAndSplittersRoundTrip()
{
    const QByteArray geometry("\x01\x02\x03 geometry blob");
    const QByteArray splitter("\x04\x05 splitter blob");
    {
        Session session;
        session.setWindowGeometry(geometry);
        session.setVerticalSplitterState(splitter);
    }   // the destructor syncs

    Session reopened;
    QCOMPARE(reopened.windowGeometry(), geometry);
    QCOMPARE(reopened.verticalSplitterState(), splitter);
}

void TestSession::terminalStateIsAbsentUntilWritten()
{
    Session session;
    QVERIFY(!session.hasTerminalState());
}

void TestSession::terminalStateRoundTrips()
{
    {
        Session session;
        session.setTerminalSplitCount(3);
        session.setTerminalExpanded(true);
        session.setSideTerminalVisible(true);
        session.setSideTerminalSplitWidth(420);
    }

    Session reopened;
    QVERIFY(reopened.hasTerminalState());
    QCOMPARE(reopened.terminalSplitCount(), 3);
    QVERIFY(reopened.terminalExpanded());
    QVERIFY(reopened.sideTerminalVisible());
    QCOMPARE(reopened.sideTerminalSplitWidth(), 420);
}

void TestSession::recentListIsMostRecentFirst()
{
    Session session;
    session.addRecentFile(QStringLiteral("/a"), 10);
    session.addRecentFile(QStringLiteral("/b"), 10);

    QCOMPARE(session.recentFiles(), QStringList({QStringLiteral("/b"), QStringLiteral("/a")}));
}

void TestSession::recentListMovesARepeatToTheFront()
{
    Session session;
    session.addRecentFolder(QStringLiteral("/a"), 10);
    session.addRecentFolder(QStringLiteral("/b"), 10);
    session.addRecentFolder(QStringLiteral("/a"), 10);

    // Re-opening an entry reorders the list, it does not duplicate it.
    QCOMPARE(session.recentFolders(), QStringList({QStringLiteral("/a"), QStringLiteral("/b")}));
}

void TestSession::recentListIsTrimmedToTheLimit()
{
    Session session;
    for (int i = 0; i < 5; ++i)
        session.addRecentFile(QStringLiteral("/f%1").arg(i), 3);

    QCOMPARE(session.recentFiles(),
             QStringList({QStringLiteral("/f4"), QStringLiteral("/f3"), QStringLiteral("/f2")}));
}

void TestSession::recentListIgnoresAnEmptyPath()
{
    Session session;
    session.addRecentFile(QString(), 10);
    QVERIFY(session.recentFiles().isEmpty());
}

QTEST_GUILESS_MAIN(TestSession)
#include "tst_session.moc"
