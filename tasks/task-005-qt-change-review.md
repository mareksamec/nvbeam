# Task 005: change review panel (native Qt)

## Goal

A panel that shows the changes an agent has already written to the worktree and
keeps itself current while the agent keeps writing. It lives in the neobeam
window as a native Qt widget, opening to the right of the side terminal, and its
text is selectable, copyable and colored (green added, red removed).

This replaces the Neovim-buffer approach explored in task 004: the diff belongs
next to the agent that produced it, not in the editor the user is working in.

## Why a panel and not the IDE diff

The IDE bridge's `openDiff` is the CLI's permission-request surface. It is only
issued when the agent asks for approval, so in auto-accept and bypass modes it
never arrives and the bridge-driven diff view stays empty. A panel that reads
the worktree with `git diff` does not care which mode the agent runs in: the
changes are on disk either way.

## What it shows

- `git diff <base>` for tracked files, where `base` is empty by default, which
  means the worktree against `HEAD`.
- The names of untracked files (`git ls-files --others --exclude-standard`)
  appended as a short section, since `git diff` does not report them.
- A header line with the file count and the added/removed line totals.

Non-goals for this task: no accept/reject actions (the agent's own permission
flow owns that), no per-file tree or side-by-side view, no staging, no editing.

## Qt elements

| Need | Element | Why this one |
|---|---|---|
| The panel itself | `QWidget` with `QVBoxLayout`, added as a third child of the existing `m_hsplitter` | The splitter already holds [editor area, side terminal]; a third child opens to the right of the side terminal and gets a drag handle for free |
| Header (title, stats, buttons) | `QToolBar` with a `QLabel` and two `QAction`s | Same construction the terminal panel's toolbar already uses, so the chrome matches |
| Diff text | `QPlainTextEdit`, read-only, `NoWrap` | Selectable and copyable with no extra work (`Ctrl+C`, context menu, keyboard selection), and its block-based document handles a multi-thousand-line diff far better than `QTextEdit`/`QTextBrowser` with rich text |
| Coloring | `QSyntaxHighlighter` subclass attached to the edit's document | Colors on top of plain text, so the text stays plain for copying: a copied hunk pastes as the diff, not as markup. Re-highlights only the blocks that changed |
| Periodic refresh | `QTimer` | Started when the panel is shown, stopped when hidden |
| Running git | `QProcess`, asynchronous | Never block the UI thread; same pattern as `GitStatus` |

`QPlainTextEdit` over a `QTreeWidget`+diff pair: the tree is the cloud client's
layout, but it is a second widget to keep in sync on every refresh and it is not
what makes the view useful. One scrolling unified diff is the whole feature.

## Staying current

An agent writing a file touches neither `.git` nor a Neovim buffer, so none of
the existing push triggers (nvim `BufWritePost`, the `.git` watcher, a cwd
change) fire. A recursive filesystem watcher is not an option either: inotify
does not recurse, so it would mean one watch per directory in the tree plus a
rebuild on every `mkdir`.

The panel therefore polls, bounded four ways:

1. Only while the panel is visible. The timer is started in `showEvent` and
   stopped in `hideEvent`.
2. Only while the window is active. A tick with the window in the background is
   skipped; `QEvent::ApplicationActivate` refreshes immediately instead, so
   coming back to neobeam shows current text without waiting out the interval.
3. One run at a time. A tick that lands while git is still running sets a rerun
   flag instead of starting a second process.
4. The view is only rewritten when the diff text actually differs from what is
   on screen. On an unchanged tick nothing moves, so scroll position and
   selection survive.

`GitStatus::changed` is also connected to a refresh, so a commit or a branch
switch updates the panel at once rather than on the next tick.

When the text does change, the vertical scrollbar value and the cursor position
are saved and restored around the rewrite, so the view does not jump to the top
while the user is reading. An active selection is lost on a real change; that is
the cost of replacing the document.

## Configuration

A new `[review]` section, since none of these belong to an existing one:

| Key | Default | Meaning |
|---|---|---|
| `width` | 600 | Startup width of the panel in pixels |
| `refresh_ms` | 2000 | Poll interval; floored at 250, `0` disables the poll (manual refresh button only) |
| `base` | `""` | Revision to diff against; empty means the worktree against `HEAD` |
| `font` | `""` | Font family; empty means the terminal font |
| `font_size` | 0 | Point size; `0` means the terminal font size |
| `max_lines` | 20000 | Cap on rendered diff lines, so a huge change cannot stall the UI |
| `added_color` | `#98c379` | `+` lines |
| `removed_color` | `#e06c75` | `-` lines |
| `hunk_color` | `#61afef` | `@@` hunk headers |
| `file_color` | `#c678dd` | `diff --git`, `---`, `+++`, `index` header lines |

## Wiring

- `src/changereview.{h,cpp}`: `ReviewConfig`, `DiffHighlighter`,
  `ChangeReviewPanel`. Added to `CMakeLists.txt`.
- `MainWindow` owns `m_review`, adds it to `m_hsplitter` after the side
  terminal, and gives it `m_cfg.reviewWidth` pixels the first time it is shown.
- Opened by the `◧` button in the side terminal's toolbar (side-panel mode only)
  and by `Ctrl+Shift+G`; the same button/shortcut closes it, as does the panel's
  own `✕`.
- `setWorkingDirectory()` follows nvim's cwd, alongside the existing
  `GitStatus::setWorkingDirectory()` call.
- `enterMaximized`/`leaveMaximized` treat it like the side terminal: its
  visibility is captured and restored, and it is hidden while another pane has
  the window.

## Acceptance

1. `Ctrl+Shift+G` or `◧` opens the panel to the right of the side terminal; the
   same key closes it.
2. A worktree with changes shows a colored unified diff; a clean worktree says
   so; a non-repository cwd says so.
3. Selecting diff text and pressing `Ctrl+C` yields the plain diff text.
4. While an agent writes files in the side terminal, the panel updates within
   `refresh_ms` without the user touching anything.
5. Scroll position survives a refresh, and an idle repository produces no
   visible movement at all.
6. `refresh_ms = 0` leaves the panel static until the refresh button is pressed.
7. The panel works with no IDE bridge and no Neovim plugin involved: it is
   `git` plus Qt only.

## Risks

- A repository with a very large diff makes each poll expensive. `max_lines`
  caps the rendering, not git's own work; a user in that situation sets
  `refresh_ms = 0`.
- The panel shows the worktree, so it also shows edits the user made by hand.
  That is intended: it is a review of what is on disk, not of what one agent
  did.
