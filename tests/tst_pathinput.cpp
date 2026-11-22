#include "pathinput.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

/// The switcher's path mode input: "~" handling, restarting a path midway and
/// resolving it against nvim's cwd. HOME is redirected by the test runner, so
/// expectations are built from QDir::homePath() rather than a fixed path.
class TestPathInput : public QObject
{
    Q_OBJECT

private slots:
    void restartsAtANewRoot_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("restarted");

        QTest::newRow("home typed after the prefill") << "~/~/Projects" << "~/Projects";
        QTest::newRow("root typed after the prefill") << "~//etc" << "/etc";
        QTest::newRow("root inside an absolute path") << "/a//b" << "/b";
        QTest::newRow("deeper prefill") << "~/code//etc" << "/etc";
        QTest::newRow("last restart wins") << "/a//b/~/c" << "~/c";
        QTest::newRow("home as the last thing typed") << "/srv/~" << "~";
        QTest::newRow("no restart") << "~/Projects/nvbeam" << "~/Projects/nvbeam";
        QTest::newRow("trailing separator kept") << "~/Projects/" << "~/Projects/";
        QTest::newRow("tilde inside a name") << "~/a~b/c" << "~/a~b/c";
        QTest::newRow("relative") << "src/app" << "src/app";
        QTest::newRow("plain tilde") << "~" << "~";
        QTest::newRow("empty") << "" << "";
    }

    void restartsAtANewRoot()
    {
        QFETCH(QString, text);
        QFETCH(QString, restarted);
        QCOMPARE(restartPath(text), restarted);
    }

    void resolvesAgainstTheBase_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("resolved");

        const QString home = QDir::homePath();
        QTest::newRow("home typed after the prefill") << "~/~/Projects" << home + "/Projects";
        QTest::newRow("root typed after the prefill") << "~//etc" << "/etc";
        QTest::newRow("root inside an absolute path") << "/a//b" << "/b";
        QTest::newRow("relative is anchored at the base") << "src" << "/base/dir/src";
        QTest::newRow("empty is the base") << "" << "/base/dir";
        QTest::newRow("blank is the base") << "  " << "/base/dir";
        QTest::newRow("plain tilde") << "~" << home;
        QTest::newRow("trailing separator kept") << "~/Projects/" << home + "/Projects/";
    }

    void resolvesAgainstTheBase()
    {
        QFETCH(QString, text);
        QFETCH(QString, resolved);
        QCOMPARE(resolvePath(text, QStringLiteral("/base/dir")), resolved);
    }

    void collapsesAndExpandsHome()
    {
        const QString home = QDir::homePath();
        QCOMPARE(collapseHome(home), QStringLiteral("~"));
        QCOMPARE(collapseHome(home + "/x/y"), QStringLiteral("~/x/y"));
        // A sibling whose name only starts with the home directory's.
        QCOMPARE(collapseHome(home + "ish/x"), home + "ish/x");
        QCOMPARE(collapseHome(QStringLiteral("/etc")), QStringLiteral("/etc"));

        QCOMPARE(expandHome(QStringLiteral("~")), home);
        QCOMPARE(expandHome(QStringLiteral("~/x")), home + "/x");
        QCOMPARE(expandHome(QStringLiteral("~x")), QStringLiteral("~x"));
        QCOMPARE(expandHome(QStringLiteral("rel")), QStringLiteral("rel"));
    }

    /// The switcher's new-folder action: nested creation, and the cases that
    /// must leave the filesystem alone or report failure.
    void createsFolders()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir root(tmp.path());

        const QString single = root.filePath(QStringLiteral("new"));
        QCOMPARE(createFolder(single), CreateFolderResult::Created);
        QVERIFY(QFileInfo(single).isDir());
        QCOMPARE(createFolder(single), CreateFolderResult::AlreadyExists);

        const QString nested = root.filePath(QStringLiteral("a/b/c"));
        QCOMPARE(createFolder(nested), CreateFolderResult::Created);
        QVERIFY(QFileInfo(nested).isDir());

        const QString file = root.filePath(QStringLiteral("file.txt"));
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QCOMPARE(createFolder(file), CreateFolderResult::NotAFolder);
        // A file in the middle of the path blocks mkpath.
        QCOMPARE(createFolder(file + QStringLiteral("/below")), CreateFolderResult::Failed);
    }

    void reportsAFolderItCannotCreate()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString locked = QDir(tmp.path()).filePath(QStringLiteral("locked"));
        QVERIFY(QDir().mkdir(locked));
        QVERIFY(QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner));
        // root ignores permission bits, so the check means nothing there.
        if (QFileInfo(locked).isWritable()) {
            QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
            QSKIP("Directory permissions are not enforced for this user");
        }
        const CreateFolderResult result = createFolder(locked + QStringLiteral("/new"));
        QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        QCOMPARE(result, CreateFolderResult::Failed);
    }
};

QTEST_MAIN(TestPathInput)
#include "tst_pathinput.moc"
