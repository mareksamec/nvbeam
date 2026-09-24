# Task 011: rank local branches by their last use, not only by commit date

## Problem

The branch switcher orders local branches by `committerdate`, which is the date
of the branch's tip commit. A branch created off an older base carries that
base's date until it has a commit of its own, so a branch made minutes ago can
sit in the middle of the list, below branches nobody has touched in weeks.

Task 008 fixed the neighbouring problem (remote-only branches were appended
after every local one); this is the ranking of the locals themselves.

## Change

Read the HEAD reflog before listing the branches and rank each local branch by
the later of:

- its tip commit's date (`%(committerdate:unix)`), and
- the last time it was checked out, from `checkout: moving from <old> to <new>`
  reflog entries. Both sides of the move count: leaving a branch says as much
  about it having been worked on as arriving at one.

The reflog read is bounded (`-n 300`), because only the most recent entry per
branch matters and a long-lived repository's whole reflog would cost far more
for nothing. A repository without a reflog - an unborn HEAD - simply ranks by
commit date, so that failure is not reported to the user.

Order of the result is unchanged from task 008: remote-only branches first (in
git's commit-date order, they have no reflog entries), then the locals by rank.

## Origin

The listing and parsing code comes from the unmerged `fix/branch-switcher-sort`
branch (commit 554b3da), cherry-picked and re-resolved onto the task 008
ordering.

## Verification

In a repository with several branches: create a branch off an old commit, do
not commit on it, switch away and back. It ranks at the top of the switcher
instead of at its base's date.
