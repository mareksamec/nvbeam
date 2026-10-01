#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

/// nvbeam.toml. The contract this covers is the one the file header states:
/// a bad value never aborts the load, it is reported and the option keeps its
/// default. So nearly every case asserts both halves - the value that survived
/// and the issue that was raised.
class TestConfig : public QObject
{
    Q_OBJECT

private slots:
    void aMissingFileWritesACommentedDefault();
    void readsFontSection();
    void readsTerminalSection();
    void readsProjects();

    void wrongTypeKeepsTheDefault();
    void unknownSectionIsReported();
    void unknownOptionIsReported();
    void aRemovedOptionPointsAtItsReplacement();
    void invalidColorKeepsTheDefault();
    void unknownHintingKeepsTheDefault();
    void invalidReviewModeFallsBackToUnstaged();
    void outOfRangeNumbersAreClamped();
    void aSyntaxErrorIsAnErrorAndLeavesDefaults();

    void projectWithoutAPathIsSkipped();
    void projectNameFallsBackToTheFolderName();

private:
    /// Write `body` as the config file for one test and return its path.
    QString writeConfig(const QByteArray &body);
    /// True when any issue's message contains `fragment`.
    static bool hasIssue(const QList<ConfigIssue> &issues, const QString &fragment,
                         ConfigIssue::Severity severity = ConfigIssue::Warning);

    QTemporaryDir m_dir;
};

QString TestConfig::writeConfig(const QByteArray &body)
{
    const QString path = m_dir.filePath(QStringLiteral("nvbeam.toml"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    file.write(body);
    file.close();
    return path;
}

bool TestConfig::hasIssue(const QList<ConfigIssue> &issues, const QString &fragment,
                          ConfigIssue::Severity severity)
{
    for (const ConfigIssue &issue : issues) {
        if (issue.severity == severity && issue.message.contains(fragment))
            return true;
    }
    return false;
}

void TestConfig::aMissingFileWritesACommentedDefault()
{
    const QString path = m_dir.filePath(QStringLiteral("fresh/nvbeam.toml"));
    QList<ConfigIssue> issues;

    const Config cfg = Config::load(path, &issues);

    QVERIFY(issues.isEmpty());
    QVERIFY(QFile::exists(path));
    QCOMPARE(cfg.fontFamily, QStringLiteral("Monospace"));

    // Everything in the written file is commented out, so loading it again
    // must produce exactly the same defaults rather than parse errors.
    QList<ConfigIssue> reloadIssues;
    const Config reloaded = Config::load(path, &reloadIssues);
    QVERIFY(reloadIssues.isEmpty());
    QCOMPARE(reloaded.fontFamily, cfg.fontFamily);
    QCOMPARE(reloaded.fontSize, cfg.fontSize);
    QCOMPARE(reloaded.terminalColorScheme, cfg.terminalColorScheme);
}

void TestConfig::readsFontSection()
{
    QList<ConfigIssue> issues;
    const Config cfg = Config::load(writeConfig(
        "[font]\n"
        "family = \"Hack Nerd Font\"\n"
        "size = 13\n"
        "hinting = \"slight\"\n"
        "antialiasing = false\n"), &issues);

    QVERIFY(issues.isEmpty());
    QCOMPARE(cfg.fontFamily, QStringLiteral("Hack Nerd Font"));
    QCOMPARE(cfg.fontSize, 13);
    QCOMPARE(cfg.hinting, QFont::PreferVerticalHinting);
    QVERIFY(!cfg.antialiasing);
}

void TestConfig::readsTerminalSection()
{
    QList<ConfigIssue> issues;
    const Config cfg = Config::load(writeConfig(
        "[terminal]\n"
        "follow_cwd = false\n"
        "start_expanded = true\n"
        "height = 450\n"
        "color_scheme = \"Dracula\"\n"
        "foreground = \"#f8f8f2\"\n"
        "shell = \"/usr/bin/zsh\"\n"
        "shell_args = [\"-l\", \"-i\"]\n"), &issues);

    QVERIFY(issues.isEmpty());
    QVERIFY(!cfg.terminalFollowCwd);
    QVERIFY(cfg.terminalStartExpanded);
    QCOMPARE(cfg.terminalHeight, 450);
    QCOMPARE(cfg.terminalColorScheme, QStringLiteral("Dracula"));
    QCOMPARE(cfg.terminalForeground, QStringLiteral("#f8f8f2"));
    QCOMPARE(cfg.terminalShell, QStringLiteral("/usr/bin/zsh"));
    QCOMPARE(cfg.terminalShellArgs, QStringList({QStringLiteral("-l"), QStringLiteral("-i")}));
}

void TestConfig::readsProjects()
{
    QList<ConfigIssue> issues;
    const Config cfg = Config::load(writeConfig(
        "[[projects]]\n"
        "path = \"~/work/nvbeam\"\n"
        "name = \"nvbeam\"\n"
        "side_command = \"claude\"\n"
        "side_command_3 = \"btop\"\n"), &issues);

    QCOMPARE(cfg.projects.size(), 1);
    // "~" expands, and the path is cleaned.
    QCOMPARE(cfg.projects.at(0).path, QDir::homePath() + QStringLiteral("/work/nvbeam"));
    QCOMPARE(cfg.projects.at(0).name, QStringLiteral("nvbeam"));
    // A gap in the numbering is not an error; it just contributes one command.
    QCOMPARE(cfg.projects.at(0).sideCommands,
             QStringList({QStringLiteral("claude"), QStringLiteral("btop")}));
    // The directory does not exist here, which is a warning and nothing more.
    QVERIFY(hasIssue(issues, QStringLiteral("path does not exist")));
}

void TestConfig::wrongTypeKeepsTheDefault()
{
    const Config defaults;
    QList<ConfigIssue> issues;
    const Config cfg = Config::load(writeConfig(
        "[font]\n"
        "size = \"thirteen\"\n"
        "antialiasing = 1\n"
        "[terminal]\n"
        "shell_args = \"-l\"\n"), &issues);

    QCOMPARE(cfg.fontSize, defaults.fontSize);
    QCOMPARE(cfg.antialiasing, defaults.antialiasing);
    QCOMPARE(cfg.terminalShellArgs, defaults.terminalShellArgs);
    QVERIFY(hasIssue(issues, QStringLiteral("[font] size: expected integer")));
    QVERIFY(hasIssue(issues, QStringLiteral("[font] antialiasing: expected boolean")));
    QVERIFY(hasIssue(issues, QStringLiteral("[terminal] shell_args: expected array of strings")));
}

void TestConfig::unknownSectionIsReported()
{
    QList<ConfigIssue> issues;
    Config::load(writeConfig("[nonsense]\nkey = 1\n"), &issues);

    QVERIFY(hasIssue(issues, QStringLiteral("unknown section [nonsense]")));
}

void TestConfig::unknownOptionIsReported()
{
    QList<ConfigIssue> issues;
    Config::load(writeConfig("[font]\nfamly = \"Monospace\"\n"), &issues);

    QVERIFY(hasIssue(issues, QStringLiteral("[font] unknown option \"famly\"")));
}

void TestConfig::aRemovedOptionPointsAtItsReplacement()
{
    QList<ConfigIssue> issues;
    Config::load(writeConfig("[side_terminal]\ncommand = \"claude\"\n"), &issues);

    QVERIFY(hasIssue(issues, QStringLiteral("set side_command on a [[projects]] entry")));
    // And not also reported as a plain unknown option.
    QVERIFY(!hasIssue(issues, QStringLiteral("unknown option \"command\"")));
}

void TestConfig::invalidColorKeepsTheDefault()
{
    const Config defaults;
    QList<ConfigIssue> issues;
    const Config cfg = Config::load(writeConfig(
        "[ui]\naccent_color = \"not a color\"\n"), &issues);

    QCOMPARE(cfg.accentColor, defaults.accentColor);
    QVERIFY(hasIssue(issues, QStringLiteral("is not a valid color")));
}

void TestConfig::unknownHintingKeepsTheDefault()
{
    QList<ConfigIssue> issues;
    const Config cfg = Config::load(writeConfig("[font]\nhinting = \"maximum\"\n"), &issues);

    QCOMPARE(cfg.hinting, QFont::PreferDefaultHinting);
    QVERIFY(hasIssue(issues, QStringLiteral("unknown value \"maximum\"")));
}

void TestConfig::invalidReviewModeFallsBackToUnstaged()
{
    QList<ConfigIssue> issues;
    const Config cfg = Config::load(writeConfig("[review]\nmode = \"sideways\"\n"), &issues);

    QCOMPARE(cfg.reviewMode, QStringLiteral("unstaged"));
    QVERIFY(hasIssue(issues, QStringLiteral("[review] mode")));
}

void TestConfig::outOfRangeNumbersAreClamped()
{
    QList<ConfigIssue> issues;
    const Config cfg = Config::load(writeConfig(
        "[terminal]\nscrollback = 0\n"
        "[side_terminal]\nwidth = 10\n"
        "[review]\nwidth = 10\nrefresh_ms = -5\n"
        "[switcher]\nmax_recent = 5000\n"), &issues);

    QCOMPARE(cfg.terminalScrollback, 1);
    QCOMPARE(cfg.sideTerminalWidth, 150);
    QCOMPARE(cfg.reviewWidth, 150);
    QCOMPARE(cfg.reviewRefreshMs, 0);
    QCOMPARE(cfg.switcherMaxRecent, 200);
    QVERIFY(hasIssue(issues, QStringLiteral("[review] refresh_ms")));
}

void TestConfig::aSyntaxErrorIsAnErrorAndLeavesDefaults()
{
    const Config defaults;
    QList<ConfigIssue> issues;
    const Config cfg = Config::load(writeConfig("[font\nfamily = \n"), &issues);

    QVERIFY(hasIssue(issues, QStringLiteral("failed to parse"), ConfigIssue::Error));
    QCOMPARE(cfg.fontFamily, defaults.fontFamily);
}

void TestConfig::projectWithoutAPathIsSkipped()
{
    QList<ConfigIssue> issues;
    const Config cfg = Config::load(writeConfig("[[projects]]\nname = \"orphan\"\n"), &issues);

    QVERIFY(cfg.projects.isEmpty());
    QVERIFY(hasIssue(issues, QStringLiteral("missing required \"path\"")));
}

void TestConfig::projectNameFallsBackToTheFolderName()
{
    // An existing directory, so the "does not exist" warning stays out of the way.
    const QString path = m_dir.path();
    QList<ConfigIssue> issues;
    const Config cfg = Config::load(writeConfig(
        "[[projects]]\npath = \"" + path.toUtf8() + "\"\n"), &issues);

    QCOMPARE(cfg.projects.size(), 1);
    QCOMPARE(cfg.projects.at(0).name, QFileInfo(path).fileName());
    QVERIFY(!hasIssue(issues, QStringLiteral("path does not exist")));
}

QTEST_MAIN(TestConfig)
#include "tst_config.moc"
