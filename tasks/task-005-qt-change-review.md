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

## Layout

A scrolling column of file rows, the way the Claude Code review pane is laid
out: one row per changed file showing the name, its directory and the
`+added -removed` counts, collapsed by default. Opening a row reveals that
file's diff; the unchanged context lines are hidden until the row's own context
button asks for them, so an open file shows what actually changed.

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
| Panel header (stats, buttons) | `QToolBar` with a `QLabel` and four `QAction`s (expand all, collapse all, refresh, close) | Same construction the terminal panel's toolbar already uses, so the chrome matches |
| File column | `QScrollArea` over a `QWidget` with a `QVBoxLayout` | One scrolling column whose rows are real widgets, so each row can hold a text editor |
| File row | `FileCard`: `QToolButton` (arrow + name, the expander) plus a `QLabel` for the counts and a checkable `QToolButton` for context | A `QTreeWidget` would give the arrows for free but its items are not text-selectable, which is the one thing the diff has to be |
| Diff text | `DiffBody`, a `QPlainTextEdit` per row, read-only, wrapped, sized to its content, with a line-number gutter | Selectable and copyable with no extra work (`Ctrl+C`, context menu, keyboard selection); its block-based document handles a long diff far better than rich text |
| Coloring | `QSyntaxHighlighter` subclass attached to each row's document | Colors on top of plain text, so the text stays plain for copying: a copied hunk pastes as the diff, not as markup. Re-highlights only the blocks that changed |
| Periodic refresh | `QTimer` | Started when the panel is shown, stopped when hidden |
| Running git | `QProcess`, asynchronous | Never block the UI thread; same pattern as `GitStatus` |

## Staying current

An agent writing a file touches neither `.git` nor a Neovim buffer, so none of
the existing push triggers (nvim `BufWritePost`, the `.git` watcher, a cwd
change) fire. A recursive filesystem watcher is not an option either: inotify
does not recurse, so it would mean one watch per directory in the tree plus a
rebuild on every `mkdir`.

The panel therefore polls, bounded three ways:

1. Only while the panel is visible. The timer is started in `showEvent` and
   stopped in `hideEvent`. There is deliberately no second condition on the tick
   (an earlier version also required the window to be active, and the refresh
   then never happened in practice).
2. One run at a time. A tick that lands while git is still running sets a rerun
   flag instead of starting a second process. Each run answers exactly once,
   from its exit, its failure to start or `timeout_ms` elapsing, whichever comes
   first, and the in-flight flag is cleared with that answer. A run outliving
   even twice its timeout is disowned by the watchdog in `collect()` so the poll
   cannot be stranded on it.
3. Rows are reconciled, not rebuilt: a file whose diff is unchanged keeps its
   row, its expanded state and any selection inside it. Only new, changed and
   removed files touch the widget tree.

`GitStatus::changed` is also connected to a refresh, so a commit or a branch
switch updates the panel at once rather than on the next tick.

Which file rows are open is remembered by path, so a file that briefly stops
being changed and comes back reappears open.

## Configuration

A new `[review]` section, since none of these belong to an existing one:

| Key | Default | Meaning |
|---|---|---|
| `width` | 600 | Startup width of the panel in pixels |
| `refresh_ms` | 2000 | Poll interval; floored at 250, `0` disables the poll (manual refresh button only) |
| `timeout_ms` | 10000 | A git run taking longer is killed and the refresh reported as failed; `0` waits forever |
| `base` | `""` | Revision to diff against; empty means the worktree against `HEAD` |
| `font` | `""` | Font family; empty means the terminal font |
| `font_size` | 0 | Point size; `0` means the terminal font size |
| `max_lines` | 20000 | Cap on the diff lines kept per file, so a huge change cannot stall the UI |
| `file_max_height` | 600 | Tallest an expanded file grows before it scrolls itself, in pixels |
| `syntax` | true | Color the code in a diff line by the file's language; ignored in a build without KF6SyntaxHighlighting |
| `show_context` | false | Expand rows to the full hunks instead of the changed lines only |

## Line numbers

`DiffBody` reserves a gutter on the left of its viewport (`setViewportMargins`)
and paints the numbers there per visible block, the way Qt's code editor
example does. They are painted rather than written into the document so that a
copied selection stays a usable diff. `FileCard::bodyText()` produces them
alongside the text: the hunk headers say where each run starts, an added or
context line takes the new file's number, a removed line the old file's, and
lines hidden by the context filter are still counted so the visible numbers
stay true. A changed line's stripe continues behind its number, and the number
itself takes the added or removed color, so the gutter reads as part of the
line rather than as a column beside it.

## Code coloring

`DiffHighlighter` also derives from `KSyntaxHighlighting::AbstractHighlighter`
when the optional KF6SyntaxHighlighting dependency is present
(`NEOBEAM_SYNTAX`, on by default, silently off when the package is missing).
Per line it hands the language the text without its `+`/`-` marker and shifts
the returned token offsets back over that column, so the document still holds
the diff character for character and a copied selection still pastes as one.
The added/removed color then tints the line's background instead of coloring
its text, the marker column keeps that color, and the lexer state is carried
between lines in a `QTextBlockUserData` and reset at every hunk header, since a
hunk starts at an arbitrary line of the file. The language is chosen from the
file path (`Repository::definitionForFileName`) and the theme from the panel
background's lightness.

## Colors

The panel has no palette of its own. `applyTerminalScheme()` in
`src/mainwindow.cpp` reads the scheme named by `[terminal] color_scheme`
through `src/colorscheme.{h,cpp}` (the loader `TerminalPanel` also uses now)
and maps it: background and foreground as they are, green for `+`, red for
`-`, blue for `@@`, magenta for the file name, bright black for dimmed text,
black for the selection, and background blends for hover, for the band each
file row's name sits on, for the hairline that closes a file block off, and for
the panel background itself, which sits a shade lighter than the terminal's so
the diff reads on a greyer ground. Those three blends run towards a neutral
grey of the foreground's brightness rather than the foreground, so a tinted
scheme foreground gives the chrome no colour cast.

The header band is painted by the row widget alone. Its children (the name
button, the context toggle, the counts) are transparent, and the hover tint is
a `:hover` rule on the row, so pointing at a file lights the band from edge to
edge instead of leaving the counts on a different grey.

An added or removed line's stripe is the block's own background
(`FileCard::paintChangedLines`), not a character format, which would stop where
the text does, and not a full-width extra selection either, which only reaches
the first visual row of a wrapped line. The body's document margin is zero, so
the stripe starts where the gutter ends with nothing unpainted between them. Its two colors (`kAddedStripe`,
`kRemovedStripe` in `src/changereview.cpp`) are fixed rather than derived from
the scheme's green and red: the stripe has to stay dark enough to read code on,
which is a different job from the marker color. The
`[terminal] foreground`/`background` overrides win over the scheme, the same
precedence the terminal itself applies. A color the scheme does not declare
keeps the `ReviewConfig` default.

## Wiring

- `src/changereview.{h,cpp}`: `ReviewConfig`, `FileDiff`, `DiffHighlighter`,
  `FileCard`, `ChangeReviewPanel`. Added to `CMakeLists.txt`.
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
2. A worktree with changes lists one row per file with its counts; expanding a
   row shows its colored diff; a clean worktree says so; a non-repository cwd
   says so.
3. Selecting diff text in an open row and pressing `Ctrl+C` yields the plain
   diff text.
4. While an agent writes files in the side terminal, the panel updates within
   `refresh_ms` without the user touching anything.
5. An open row stays open across refreshes, and an idle repository produces no
   visible movement at all.
6. `refresh_ms = 0` leaves the panel static until the refresh button is pressed.
7. A `git` that never exits leaves the panel polling: the run is killed at
   `timeout_ms`, the panel says it gave up, and the next tick tries again.
8. The panel works with no IDE bridge and no Neovim plugin involved: it is
   `git` plus Qt only.

## Risks

- A repository with a very large diff makes each poll expensive. `max_lines`
  caps the rendering, not git's own work; a user in that situation sets
  `refresh_ms = 0`.
- The panel shows the worktree, so it also shows edits the user made by hand.
  That is intended: it is a review of what is on disk, not of what one agent
  did.
