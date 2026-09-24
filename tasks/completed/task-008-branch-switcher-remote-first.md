# Task 008: show remote-only branches at the top of the branch switcher

## Problem

A branch someone created and pushed from another machine (or from the web UI)
shows up in the switcher far below the local branches, so the branch that is
most likely to be wanted next is off the visible part of the list.

`GitStatus::parseBranches()` is the cause. `git for-each-ref
--sort=-committerdate` already returns every ref in commit-date order, but the
parser splits the output into two lists - locals and remotes - and appends the
remotes after all locals. A remote-only branch therefore always ranks below
every local branch regardless of its date.

## Change

Order the offered rows as:

1. remote-only branches (no local counterpart), in git's commit-date order,
2. local branches, in git's commit-date order.

The deduplication rule is unchanged: a remote branch whose short target name
already exists locally is dropped, because checking it out would check out that
same local branch.

## Non-goals

- No reflog-based ranking of local branches (the unmerged
  `fix/branch-switcher-sort` branch explored that; it is a separate question).
- No change to the switcher widget, its filtering, or the checkout path.
