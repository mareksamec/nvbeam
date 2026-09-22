# Task 006: two diff modes in the change review panel

## Goal

The review panel answers one question today ("what is in the worktree that is
not in HEAD"). Reviewing a branch before it goes out needs the other one
("what does this branch add on top of main"). Give the panel both, as two
modes over the same rows, the same coloring and the same poll.

- **Unstaged** — the current behaviour: the worktree against `HEAD`, plus
  untracked files. What an agent has just written.
- **Branch** — the whole branch: everything that separates it from the default
  branch, uncommitted work included.

## What "branch" compares

The base is the merge base, not the tip of the default branch:

```sh
git merge-base <default-branch> HEAD   # -> <merge-base>
git diff <merge-base>                  # merge base against the worktree
```

Two reasons for the merge base rather than `git diff main`:

1. Commits that landed in `main` after this branch started would otherwise
   appear as lines this branch deletes. The merge base asks "what did this
   branch do", which is the review question.
2. Diffing against the worktree rather than `HEAD` (that is, `git diff <merge-base>`
   rather than `git diff <merge-base>...HEAD`) keeps uncommitted work visible.
   Otherwise a file the user just edited disappears from the panel the moment
   the mode is switched, which reads as a bug.

Untracked files are listed in both modes, since both end at the worktree. The
existing rule in `collect()` that skips `git ls-files --others` whenever a base
is set therefore has to become "skip it only when comparing two revisions",
which is what an explicit `[review] base` does.

## Resolving the default branch

In order, stopping at the first that answers:

1. `[review] base_branch` from the config, when set. No guessing for a
   repository whose trunk is called something else.
2. `git symbolic-ref --quiet --short refs/remotes/origin/HEAD` → `origin/main`.
   This is what the remote itself calls its default branch.
3. `git rev-parse --verify --quiet main`, then `master`, then `origin/main`,
   `origin/master`.

Resolution is asynchronous like every other git call here, cached per working
directory, and invalidated when `GitStatus::changed` fires (a branch switch or
a fetch can change the answer) or when the cwd changes.

Failure cases each get their own message in the panel body rather than an empty
list:

| Situation | Message |
|---|---|
| No default branch found | "No main or master branch to compare against." |
| `merge-base` finds no common ancestor (unrelated histories, shallow clone) | "No common ancestor with <branch>." |
| HEAD is the default branch | "On <branch>; nothing to compare." plus an offer to read it as the unstaged mode |
| Detached HEAD | Works as normal: the merge base of a detached HEAD is well defined |

## Switching modes

A checkable `QToolButton` in the panel toolbar, left of `⟳`, labelled with the
active mode (`worktree` / `branch`), plus a shortcut registered in
`src/shortcuts.h` so the tooltip can advertise it through `Shortcuts::hint()`,
the way the terminal panel's buttons already do. `Ctrl+Shift+D` is free today;
the header comment in `shortcuts.h` records why a binding lives there rather
than in `MainWindow::eventFilter`.

Switching modes refreshes immediately rather than waiting for the next tick,
and keeps which rows are open: the panel already remembers that by path.

The startup mode comes from `[review] mode`. A mode picked at runtime is not
written back to `neobeam.toml` (that file is the user's, see `src/session.h`);
whether it belongs in `session.ini` alongside the restored terminal state is a
judgement call for the implementation, and staying out of the session file is
an acceptable answer.

An explicit `[review] base` still wins over both modes: it pins the comparison
to one revision. The toggle is then disabled, with a tooltip saying which key
is holding it.

## Configuration

| Key | Default | Meaning |
|---|---|---|
| `mode` | `"unstaged"` | Startup mode: `"unstaged"` (worktree vs HEAD) or `"branch"` (merge base with the default branch vs worktree) |
| `base_branch` | `""` | Branch mode's comparison branch; empty means auto-detect (`origin/HEAD`, then `main`, then `master`) |

Both go through the existing `readString` path in `src/config.cpp`, are added
to the `[review]` key list that rejects typos, and ship commented out in
`neobeam.toml.example`. An unknown `mode` value is a config warning that falls
back to `unstaged` rather than an error: the panel still has to open.

## Wiring

- `ReviewConfig` gains `mode` and `baseBranch`; `ChangeReviewPanel` gains the
  active mode, the resolved base branch and the cached merge base.
- `collect()` chooses its arguments from the mode. Branch mode needs the merge
  base first, so the call becomes two chained `runGit()` calls; the "one run at
  a time" guard has to cover the pair, not each half, or a slow `merge-base`
  lets a second pass start.
- The header stats and the empty-state message say what is being compared
  ("No changes against HEAD." / "No changes against <branch>."), so the mode is
  readable without looking at the toggle.
- README: a row in the review section for the toggle and its shortcut, and the
  two new config keys.

## Acceptance

1. The toggle and its shortcut switch modes, the tooltip shows the shortcut,
   and the panel refreshes at once.
2. On a feature branch with commits and uncommitted edits, branch mode lists
   both; unstaged mode lists only the uncommitted ones.
3. Commits added to `main` after the branch started never appear as removals in
   branch mode.
4. Untracked files appear in both modes.
5. A repository whose trunk is `master` resolves without configuration; one
   with neither `main` nor `master` says so instead of showing an empty list.
6. `[review] base` set pins the comparison and disables the toggle.
7. Switching branches in the embedded terminal re-resolves the base on the
   `GitStatus::changed` refresh, with no restart.
8. Rows stay open across a mode switch.

## Risks

- `merge-base` on a shallow clone can fail; the message has to name that rather
  than reporting "no changes".
- Branch mode on a long-lived branch produces a much larger diff than the
  worktree mode. `max_lines` already caps the rendering per file, but the
  per-poll cost is higher, which is an argument for `refresh_ms = 0` in that
  situation.
