#include "pathindex.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

/// Create the files (and the folders they need) below `root`. A path ending in
/// '/' is created as an empty folder.
void createTree(const QString &root, const QStringList &paths)
{
    for (const QString &path : paths) {
        const QString full = root + QLatin1Char('/') + path;
        if (path.endsWith(QLatin1Char('/'))) {
            QDir().mkpath(full);
            continue;
        }
        QDir().mkpath(QFileInfo(full).absolutePath());
        QFile file(full);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }
}

/// `git ls-files -z` output for these paths.
QByteArray nulSeparated(const QStringList &paths)
{
    QByteArray out;
    for (const QString &path : paths)
        out += path.toUtf8() + '\0';
    return out;
}

QStringList sorted(QStringList list)
{
    list.sort();
    return list;
}

} // namespace

/// The file list behind path mode's recursive search: what git's output and a
/// folder walk produce, and how a fragment ranks it.
class TestPathIndex : public QObject
{
    Q_OBJECT

private slots:
    void gitOutputListsFilesAndTheirFolders()
    {
        const PathIndex index = pathIndexFromGitLsFiles(
            nulSeparated({"README.md", "src/app/main.cpp", "src/app/util.cpp", "src/x.h"}), 100);

        // Each folder once, before its contents.
        QCOMPARE(index.paths, (QStringList{"README.md", "src/", "src/app/", "src/app/main.cpp",
                                           "src/app/util.cpp", "src/x.h"}));
        QVERIFY(!index.truncated);
    }

    void gitOutputKeepsUnusualNames()
    {
        const PathIndex index = pathIndexFromGitLsFiles(
            nulSeparated({"with space.txt", QStringLiteral("café.md")}), 100);
        QCOMPARE(index.paths, (QStringList{"with space.txt", QStringLiteral("café.md")}));
    }

    void gitOutputStopsAtTheCap()
    {
        const PathIndex index = pathIndexFromGitLsFiles(nulSeparated({"a/1", "a/2", "a/3"}), 3);
        QCOMPARE(index.paths, (QStringList{"a/", "a/1", "a/2"}));
        QVERIFY(index.truncated);
    }

    void walkSkipsWhatItShould()
    {
        QTemporaryDir dir;
        createTree(dir.path(), {"src/main.cpp", ".git/HEAD", ".config/tool/rc",
                                "nested/.git/HEAD", "nested/inner.txt", "empty/"});
        const PathIndex index = walkPathIndex(dir.path(), {6, 1000});

        // .git is left out; a hidden folder and a nested repository are listed
        // but not walked into; an empty folder is still a folder.
        QCOMPARE(sorted(index.paths), (QStringList{".config/", "empty/", "nested/",
                                                   "src/", "src/main.cpp"}));
        QVERIFY(!index.truncated);
    }

    void walkStopsAtTheDepth()
    {
        QTemporaryDir dir;
        createTree(dir.path(), {"a/b/c/deep.txt", "top.txt"});
        const PathIndex index = walkPathIndex(dir.path(), {2, 1000});
        QCOMPARE(sorted(index.paths), (QStringList{"a/", "a/b/", "top.txt"}));
    }

    void walkKeepsTheShallowEntriesAtTheCap()
    {
        QTemporaryDir dir;
        createTree(dir.path(), {"a/b/c/1", "a/b/c/2", "x", "y"});
        const PathIndex index = walkPathIndex(dir.path(), {6, 3});

        // Breadth first: everything at the top comes before anything deeper.
        QCOMPARE(sorted(index.paths), (QStringList{"a/", "x", "y"}));
        QVERIFY(index.truncated);
    }

    void walkStopsWhenCancelled()
    {
        QTemporaryDir dir;
        createTree(dir.path(), {"a/1", "b/2"});
        const std::atomic<bool> cancel(true);
        QVERIFY(walkPathIndex(dir.path(), {6, 1000}, &cancel).paths.isEmpty());
    }

    void walkDoesNotFollowSymlinkedFolders()
    {
        QTemporaryDir dir;
        createTree(dir.path(), {"real/file.txt"});
        QVERIFY(QFile::link(dir.path() + "/real", dir.path() + "/link"));
        const PathIndex index = walkPathIndex(dir.path(), {6, 1000});
        QCOMPARE(sorted(index.paths), (QStringList{"link/", "real/", "real/file.txt"}));
    }

    void ranksAcrossFoldersAndByName()
    {
        const QStringList paths{"src/", "src/switcher.cpp", "src/switcher.h", "docs/sw.md",
                                "tests/tst_switch.cpp", "build/"};

        // Spanning folders: "srcsw" is src/ + sw. The two switcher files tie,
        // and the shorter path comes first.
        QCOMPARE(rankPaths(paths, "srcsw", 10),
                 (QStringList{"src/switcher.h", "src/switcher.cpp"}));

        // A match in the name beats one spread over the path, and an equal
        // score keeps the shorter path first.
        const QStringList ranked = rankPaths(paths, "switcher", 10);
        QCOMPARE(ranked.mid(0, 2), (QStringList{"src/switcher.h", "src/switcher.cpp"}));

        // Folders take part, matched without their trailing separator.
        QCOMPARE(rankPaths(paths, "build", 10), QStringList{"build/"});

        QCOMPARE(rankPaths(paths, "sw", 2).size(), 2);
        QVERIFY(rankPaths(paths, "zzz", 10).isEmpty());
    }

    void hidesDotPathsUnlessAskedFor()
    {
        const QStringList paths{".github/", ".github/ci.yml", "src/.env", "src/config.cpp"};

        QCOMPARE(rankPaths(paths, "ci", 10), QStringList{"src/config.cpp"});
        QVERIFY(rankPaths(paths, ".gi", 10).contains(QStringLiteral(".github/")));
        QVERIFY(rankPaths(paths, ".en", 10).contains(QStringLiteral("src/.env")));

        QVERIFY(isHiddenPath(QStringLiteral(".github/ci.yml")));
        QVERIFY(isHiddenPath(QStringLiteral("src/.env")));
        QVERIFY(!isHiddenPath(QStringLiteral("src/a.b")));
    }
};

QTEST_MAIN(TestPathIndex)
#include "tst_pathindex.moc"
