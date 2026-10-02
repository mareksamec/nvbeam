#include "pinnedprojects.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTest>

/// projects.toml: the switcher's own pin list, written and read back by the
/// app. QStandardPaths test mode redirects AppConfigLocation, so the real
/// config directory is never touched.
class TestPinnedProjects : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void loadOfAMissingFileIsEmpty();
    void addThenLoadRoundTrips();
    void addRejectsADuplicatePath();
    void addRejectsAnEmptyPath();
    void removeDropsTheEntry();
    void removeReportsAnUnknownPath();
    void nameFallsBackToTheFolderName();
    void aMalformedFileReadsAsEmpty();
};

void TestPinnedProjects::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestPinnedProjects::init()
{
    // Each test starts from no pin list at all.
    QFile::remove(PinnedProjects::filePath());
}

void TestPinnedProjects::loadOfAMissingFileIsEmpty()
{
    QVERIFY(PinnedProjects::load().isEmpty());
}

void TestPinnedProjects::addThenLoadRoundTrips()
{
    ProjectEntry project;
    project.path = QStringLiteral("/tmp/some project");   // a space, to exercise the quoting
    project.name = QStringLiteral("Some Project");
    project.sideCommands = {QStringLiteral("claude"), QStringLiteral("btop")};

    QVERIFY(PinnedProjects::add(project));

    const QList<ProjectEntry> loaded = PinnedProjects::load();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.at(0).path, project.path);
    QCOMPARE(loaded.at(0).name, project.name);
    QCOMPARE(loaded.at(0).sideCommands, project.sideCommands);
}

void TestPinnedProjects::addRejectsADuplicatePath()
{
    ProjectEntry project;
    project.path = QStringLiteral("/tmp/project");
    QVERIFY(PinnedProjects::add(project));
    QVERIFY(!PinnedProjects::add(project));
    QCOMPARE(PinnedProjects::load().size(), 1);
}

void TestPinnedProjects::addRejectsAnEmptyPath()
{
    QVERIFY(!PinnedProjects::add(ProjectEntry{}));
    QVERIFY(PinnedProjects::load().isEmpty());
}

void TestPinnedProjects::removeDropsTheEntry()
{
    ProjectEntry first;
    first.path = QStringLiteral("/tmp/first");
    ProjectEntry second;
    second.path = QStringLiteral("/tmp/second");
    QVERIFY(PinnedProjects::add(first));
    QVERIFY(PinnedProjects::add(second));

    QVERIFY(PinnedProjects::remove(first.path));

    const QList<ProjectEntry> loaded = PinnedProjects::load();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.at(0).path, second.path);
}

void TestPinnedProjects::removeReportsAnUnknownPath()
{
    QVERIFY(!PinnedProjects::remove(QStringLiteral("/tmp/never-pinned")));
}

void TestPinnedProjects::nameFallsBackToTheFolderName()
{
    ProjectEntry project;
    project.path = QStringLiteral("/tmp/nvbeam");   // no name set
    QVERIFY(PinnedProjects::add(project));

    QCOMPARE(PinnedProjects::load().at(0).name, QStringLiteral("nvbeam"));
}

void TestPinnedProjects::aMalformedFileReadsAsEmpty()
{
    // App state, not user input: a broken file must not stop the app.
    const QString path = PinnedProjects::filePath();
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("this is not = = toml\n[[projects\n");
    file.close();

    QVERIFY(PinnedProjects::load().isEmpty());
}

QTEST_GUILESS_MAIN(TestPinnedProjects)
#include "tst_pinnedprojects.moc"
