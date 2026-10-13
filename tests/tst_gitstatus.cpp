#include "gitstatus.h"

#include <QTest>

/// The three parsers GitStatus runs over git's own output. They are pure
/// functions, so the tests feed them recorded output in exactly the formats
/// gitstatus.cpp asks git for:
///   status:   git status --porcelain --branch
///   reflog:   git reflog show --date=unix --format=%gd%x09%gs
///   branches: git for-each-ref --format=
///             %(HEAD)\t%(refname)\t%(refname:short)\t%(committerdate:relative)\t%(committerdate:unix)
class TestGitStatus : public QObject
{
    Q_OBJECT

private slots:
    void statusReadsBranchAndCounts();
    void statusReadsAheadBehind();
    void statusHandlesDetachedAndEmptyRepo();
    void statusIgnoresTruncatedLines();

    void reflogCollectsBothSidesOfACheckout();
    void reflogKeepsTheNewestEntryPerBranch();
    void reflogIgnoresUnrelatedEntries();

    void branchesSplitRemotesAndLocals();
    void branchesDropRemotesThatHaveALocal();
    void branchesRankLocalsByActivity();
    void branchesSkipRemoteHeadSymref();

    void branchSwitchNeedsAPreviousValidBranch();
    void branchSwitchIgnoresTheSameBranch();
    void branchSwitchSeesADifferentBranch();
    void branchSwitchIgnoresAnInvalidSnapshot();
    void branchSwitchSurvivesAnInvalidSnapshotInBetween();
};

void TestGitStatus::statusReadsBranchAndCounts()
{
    const auto info = GitStatus::parseStatus(QStringLiteral(
        "## main...origin/main\n"
        "M  src/config.cpp\n"      // staged only
        " M src/gitstatus.cpp\n"   // worktree only
        "MM src/main.cpp\n"        // both
        "?? build/\n"));

    QVERIFY(info.valid);
    QCOMPARE(info.branch, QStringLiteral("main"));
    QCOMPARE(info.staged, 2);
    QCOMPARE(info.modified, 2);
    QCOMPARE(info.untracked, 1);
    QCOMPARE(info.changedFiles,
             QStringList({QStringLiteral("src/config.cpp"), QStringLiteral("src/gitstatus.cpp"),
                          QStringLiteral("src/main.cpp")}));
    QCOMPARE(info.untrackedFiles, QStringList({QStringLiteral("build/")}));
    QCOMPARE(info.ahead, 0);
    QCOMPARE(info.behind, 0);
}

void TestGitStatus::statusReadsAheadBehind()
{
    auto info = GitStatus::parseStatus(QStringLiteral("## main...origin/main [ahead 3]\n"));
    QCOMPARE(info.ahead, 3);
    QCOMPARE(info.behind, 0);

    info = GitStatus::parseStatus(QStringLiteral("## main...origin/main [behind 2]\n"));
    QCOMPARE(info.ahead, 0);
    QCOMPARE(info.behind, 2);

    info = GitStatus::parseStatus(QStringLiteral("## main...origin/main [ahead 1, behind 2]\n"));
    QCOMPARE(info.ahead, 1);
    QCOMPARE(info.behind, 2);
}

void TestGitStatus::statusHandlesDetachedAndEmptyRepo()
{
    QCOMPARE(GitStatus::parseStatus(QStringLiteral("## HEAD (no branch)\n")).branch,
             QStringLiteral("detached"));
    QCOMPARE(GitStatus::parseStatus(QStringLiteral("## No commits yet on main\n")).branch,
             QStringLiteral("main"));
    // A branch with no upstream has no "..." part.
    QCOMPARE(GitStatus::parseStatus(QStringLiteral("## feature/x\n")).branch,
             QStringLiteral("feature/x"));
}

void TestGitStatus::statusIgnoresTruncatedLines()
{
    // Nothing shorter than "XY <path>" can carry a path.
    const auto info = GitStatus::parseStatus(QStringLiteral("## main\nM\n M \n"));
    QCOMPARE(info.staged, 0);
    QCOMPARE(info.modified, 0);
    QVERIFY(info.changedFiles.isEmpty());
}

void TestGitStatus::reflogCollectsBothSidesOfACheckout()
{
    const auto activity = GitStatus::parseReflogActivity(QStringLiteral(
        "HEAD@{1700000000}\tcheckout: moving from main to feature/x\n"));

    QCOMPARE(activity.size(), 2);
    QCOMPARE(activity.value(QStringLiteral("main")), 1700000000LL);
    QCOMPARE(activity.value(QStringLiteral("feature/x")), 1700000000LL);
}

void TestGitStatus::reflogKeepsTheNewestEntryPerBranch()
{
    // git prints newest first, so the first sighting of a name is its latest.
    const auto activity = GitStatus::parseReflogActivity(QStringLiteral(
        "HEAD@{1700000500}\tcheckout: moving from feature/x to main\n"
        "HEAD@{1700000000}\tcheckout: moving from main to feature/x\n"));

    QCOMPARE(activity.value(QStringLiteral("main")), 1700000500LL);
    QCOMPARE(activity.value(QStringLiteral("feature/x")), 1700000500LL);
}

void TestGitStatus::reflogIgnoresUnrelatedEntries()
{
    const auto activity = GitStatus::parseReflogActivity(QStringLiteral(
        "HEAD@{1700000100}\tcommit: add tests\n"
        "HEAD@{1700000050}\treset: moving to HEAD~1\n"
        "no tab here\n"
        "HEAD@{1700000000}\tcheckout: moving from main to topic\n"));

    QCOMPARE(activity.size(), 2);
    QVERIFY(activity.contains(QStringLiteral("main")));
    QVERIFY(activity.contains(QStringLiteral("topic")));
}

void TestGitStatus::branchesSplitRemotesAndLocals()
{
    const auto branches = GitStatus::parseBranches(QStringLiteral(
        "*\trefs/heads/main\tmain\t2 hours ago\t1700000000\n"
        "\trefs/remotes/origin/feature\torigin/feature\t1 hour ago\t1700003600\n"), {});

    QCOMPARE(branches.size(), 2);
    // Remote-only branches lead: they have no local row to be found under.
    QCOMPARE(branches.at(0).name, QStringLiteral("origin/feature"));
    QVERIFY(branches.at(0).remote);
    // The target drops the remote prefix so checkout creates a tracking branch.
    QCOMPARE(branches.at(0).target, QStringLiteral("feature"));
    QCOMPARE(branches.at(0).detail, QStringLiteral("1 hour ago"));
    QVERIFY(!branches.at(0).current);

    QCOMPARE(branches.at(1).name, QStringLiteral("main"));
    QCOMPARE(branches.at(1).target, QStringLiteral("main"));
    QVERIFY(!branches.at(1).remote);
    QVERIFY(branches.at(1).current);
}

void TestGitStatus::branchesDropRemotesThatHaveALocal()
{
    const auto branches = GitStatus::parseBranches(QStringLiteral(
        "*\trefs/heads/main\tmain\t2 hours ago\t1700000000\n"
        "\trefs/remotes/origin/main\torigin/main\t2 hours ago\t1700000000\n"), {});

    QCOMPARE(branches.size(), 1);
    QCOMPARE(branches.at(0).name, QStringLiteral("main"));
}

void TestGitStatus::branchesRankLocalsByActivity()
{
    // "fresh" was branched off an old base, so its commit date is older than
    // main's; the checkout recorded in the reflog is what should lift it.
    const QString output = QStringLiteral(
        "\trefs/heads/main\tmain\t2 hours ago\t1700000000\n"
        "\trefs/heads/fresh\tfresh\t3 weeks ago\t1600000000\n");

    auto branches = GitStatus::parseBranches(output, {});
    QCOMPARE(branches.at(0).name, QStringLiteral("main"));

    branches = GitStatus::parseBranches(output, {{QStringLiteral("fresh"), 1700009999LL}});
    QCOMPARE(branches.at(0).name, QStringLiteral("fresh"));
    QCOMPARE(branches.at(1).name, QStringLiteral("main"));
}

void TestGitStatus::branchesSkipRemoteHeadSymref()
{
    // git shortens refs/remotes/origin/HEAD to the bare remote name.
    const auto branches = GitStatus::parseBranches(QStringLiteral(
        "\trefs/remotes/origin/HEAD\torigin\t2 hours ago\t1700000000\n"
        "\trefs/heads/main\tmain\t2 hours ago\t1700000000\n"), {});

    QCOMPARE(branches.size(), 1);
    QCOMPARE(branches.at(0).name, QStringLiteral("main"));
}

// --- isBranchSwitch ---
// The gate on GitStatus::branchChanged, which is what refreshes a branch-aware
// shell prompt in the terminal panel after a checkout.

/// A snapshot on `branch`, as parseStatus() would produce it.
static GitStatusInfo onBranch(const QString &branch)
{
    GitStatusInfo info;
    info.valid = true;
    info.branch = branch;
    return info;
}

void TestGitStatus::branchSwitchNeedsAPreviousValidBranch()
{
    // The first snapshot after startup is not a switch: there is nothing to
    // have switched away from.
    QVERIFY(!GitStatus::isBranchSwitch(QString(), onBranch(QStringLiteral("main"))));
}

void TestGitStatus::branchSwitchIgnoresTheSameBranch()
{
    // A status-count-only update (a file was saved) must not look like a
    // checkout, or every save would poke the terminal.
    QVERIFY(!GitStatus::isBranchSwitch(QStringLiteral("main"),
                                       onBranch(QStringLiteral("main"))));
}

void TestGitStatus::branchSwitchSeesADifferentBranch()
{
    QVERIFY(GitStatus::isBranchSwitch(QStringLiteral("main"),
                                      onBranch(QStringLiteral("feature"))));
}

void TestGitStatus::branchSwitchIgnoresAnInvalidSnapshot()
{
    // Leaving a repository (or a failed git run) hides the segment; it is not
    // a branch switch.
    QVERIFY(!GitStatus::isBranchSwitch(QStringLiteral("main"), GitStatusInfo{}));
}

void TestGitStatus::branchSwitchSurvivesAnInvalidSnapshotInBetween()
{
    // The regression this function exists for: a git run that failed while the
    // checkout was still in flight publishes an invalid snapshot. Comparing
    // against that snapshot would report no switch when the new branch
    // arrives, and the prompt refresh would be lost for good. The last *valid*
    // branch is what is compared instead, so it is still a switch.
    const QString lastValid = QStringLiteral("main");
    QVERIFY(!GitStatus::isBranchSwitch(lastValid, GitStatusInfo{}));
    QVERIFY(GitStatus::isBranchSwitch(lastValid, onBranch(QStringLiteral("feature"))));
}

QTEST_GUILESS_MAIN(TestGitStatus)
#include "tst_gitstatus.moc"
