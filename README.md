# Neobeam

A fast, native Neovim GUI client built with Qt6 and C++.

Neobeam connects to an embedded Neovim instance via msgpack-RPC and renders the editor using native Qt widgets. It provides a VSCode-like experience with a native terminal panel, minimap, tabline, command line, and completion popup — all driven by Neovim's UI protocol extensions.

## Features

- **Full Neovim integration** — ext_linegrid, ext_multigrid, ext_hlstate, ext_cmdline, ext_popupmenu, ext_tabline, ext_messages
- **Native statusline** — mode indicator, file, git branch, diagnostics, position
- **Git branch switcher** – VSCode-style branch picker under the statusline: click the git segment (or Ctrl+Shift+B) to switch branches or create a new one
- **Embedded terminal** — QTermWidget-based, with splits and nvim cwd following (Ctrl+\` to toggle), resizable via a draggable divider
- **Change review** — panel next to the side terminal listing each changed file, expandable to its colored, selectable diff (Ctrl+Shift+G), refreshing itself while an agent writes
- **Minimap** — zoomed-out buffer view with syntax highlighting, viewport indicator, click-to-scroll
- **Configurable fonts** — family, size, hinting, antialiasing
- **Drag & drop** — drop files onto the buffer area to open them in the window under the cursor; modifier keys choose split/tab placement. Dropping onto the terminal pastes the path
- **TOML configuration** — sensible defaults, all options commented out in the generated config
- **Session restore** — remembers window size, terminal splits/proportions, panel height, and expanded state
- **Wayland native** — via qt6-wayland
- **Image rendering** — *(Planned)* placeholder for upcoming `vim.ui.img` API

## Requirements

- Neovim 0.10+
- Qt 6.x (Core, Gui, Widgets)
- CMake 3.20+
- C++20 compiler
- qtermwidget6 (optional, for embedded terminal)
- KF6SyntaxHighlighting (optional, for code coloring in the review panel's diffs)

## Build

```sh
git clone https://github.com/youruser/neobeam.git
cd neobeam
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

To enable the embedded terminal (requires qtermwidget6 installed):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNEOBEAM_TERMINAL=ON
cmake --build build -j$(nproc)
```

### Dependencies (Debian/Ubuntu)

```sh
sudo apt install cmake g++ qt6-base-dev libgl1-mesa-dev
# Optional: for embedded terminal
sudo apt install qtermwidget6-dev
# Optional: for code coloring in review diffs
sudo apt install libkf6syntaxhighlighting-dev
```

### Dependencies (Fedora)

```sh
sudo dnf install cmake gcc-c++ qt6-qtbase-devel mesa-libGL-devel
# Optional: for embedded terminal
sudo dnf install qtermwidget-qt6-devel
# Optional: for code coloring in review diffs
sudo dnf install kf6-syntax-highlighting-devel
```

### Dependencies (Arch)

```sh
sudo pacman -S cmake qt6-base
# Optional: for embedded terminal
sudo pacman -S qtermwidget
# Optional: for code coloring in review diffs
sudo pacman -S syntax-highlighting
```

## Usage

```sh
./build/neobeam [options] [-- nvim-args...]
```

### Options

| Flag | Description |
|------|-------------|
| `-f`, `--font <family>` | Font family |
| `--font-size <size>` | Font size in points |
| `--native-ui` | Enable all native UI elements (ext_popupmenu, ext_tabline, ext_messages) |
| `-c`, `--config <path>` | Path to a custom config file |
| `-k`, `--check-config` | Validate the config file and exit (exit code: 0 = clean, 1 = warnings, 2 = errors) |
| `-h`, `--help` | Show help |
| `-v`, `--version` | Show version |

Any unrecognized arguments are passed directly to the embedded Neovim process.

### Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+\` | Toggle terminal panel |
| Ctrl+Shift+\` | Toggle the side terminal (same as the ◨ button) |
| Ctrl+Tab | Switch between terminal and nvim buffer |
| Shift+Insert | Paste clipboard (into terminal when focused, into Neovim otherwise) |
| Ctrl+Shift+V | Paste clipboard (into terminal when focused, into Neovim otherwise) |
| Ctrl+= | Increase the font size of the focused pane (terminal or nvim buffer) |
| Ctrl+- | Decrease the font size of the focused pane (terminal or nvim buffer) |
| Ctrl+Mouse wheel | Increase/decrease the font size of the pane under the pointer |
| Ctrl+Shift+X | Maximize the focused pane (nvim buffer, bottom terminal or side terminal); press again to restore |
| Ctrl+Shift+P | Toggle the project switcher (Ctrl+D inside it switches between project search and path mode) |
| Ctrl+Shift+B | Toggle the git branch switcher, from anywhere in the window including inside a terminal (also opens by clicking the statusline's git segment) |
| Ctrl+Shift+L | Open the link in the focused terminal's selection |
| Ctrl+Shift+G | Toggle the change review panel (git diff of the worktree, opens right of the side terminal) |

### Project switcher

Starting neobeam without arguments opens a KRunner-style switcher: type to fuzzy-search pinned projects (`[[projects]]` in the config) and recently opened folders and files. Selecting an entry points nvim's cwd and every terminal (side terminal included) at the project directory, and opens the file or folder in nvim. When nvim exits (`:q`), neobeam stays running and shows the switcher again instead of closing (disable with `[switcher] keep_running = false`). Esc or clicking elsewhere dismisses it.

Ctrl+D switches the same input line to path mode, for opening anything that is neither pinned nor recent. It starts at nvim's cwd and completes what you type against the real directory tree: type `~/Proj`, press Tab and the input becomes `~/Projects/`; each further Tab cycles to the next matching entry. Enter confirms the completion (a folder's contents are then listed, so Tab/Enter walks down the tree) and the next Enter opens what the path names. Right completes and confirms in one keystroke, at the end of the line only. Relative paths resolve against nvim's cwd, and dotfiles appear once the typed name starts with `.`. Ctrl+D returns to project mode.

### Git branch switcher

The git segment is led by a branch glyph, painted with the segment's own foreground colour rather than loaded from an icon file, so it follows the statusline palette instead of needing a recoloured asset per theme.

Clicking the git segment of the statusline, or pressing **Ctrl+Shift+B** (which works wherever focus happens to be, the embedded terminals included, and closes the popup again), opens a branch picker anchored to that segment, the way VSCode's branch indicator does. It lists the repository's local branches, most recently committed to first, followed by the remote-only ones; the checked-out branch is marked with `◆`. Typing fuzzy-filters the list, Up/Down move, Enter checks the highlighted branch out, Esc or clicking elsewhere closes it.

Checking out a remote-only entry (`origin/some-branch`) checks out the plain name, so git creates a local branch tracking the remote one instead of detaching HEAD.

Typing a name no branch has adds a **Create branch "…"** row at the end of the list; Enter there runs `git checkout -b <name>` from the current HEAD and switches to it. The row is last on purpose: switching to an existing branch is the common case, and Enter should never create a branch by surprise.

When git refuses (uncommitted changes that the checkout would overwrite, a branch already checked out in another worktree, a name git rejects), the popup stays open and shows git's own message. On success it closes, the statusline's git segment updates, open buffers are rechecked against the new working tree (`checktime`), and the bottom terminal's shell prompt is refreshed so a branch-aware prompt does not go stale.

The popup is monospaced (`[ui] branch_switcher_font` and `branch_switcher_font_size`), with the commit dates padded into a column that lines up, and highlighted with `[ui] focus_border_color` (OneDark blue by default) rather than the green `accent_color` the project switcher uses, so the two pickers are told apart at a glance. The name column takes whatever width is left once the widest date is reserved, so a date is never pushed off the edge; a branch name too long for it is elided in the middle and shown in full in the row's tooltip.

The switcher needs the statusline's git segment, so it follows `[ui] git_status`; with that off, neither the click target nor the shortcut exists.

### Change review

The **◧** button on the side terminal's toolbar, or **Ctrl+Shift+G**, opens a
review panel to the right of that terminal. It lists one row per changed file,
each showing the file name, its directory and `+added -removed` counts, and a
tag for a new, deleted, renamed or untracked file. A row is collapsed by
default; its name sits on a band running the full width of the panel, which is
what keeps one expanded file apart from the next. Clicking the row (or **⌄** /
**⌃** in the header, which open and close every row) reveals that file's diff, and inside a row the **⋯** button brings back the
unchanged context lines, which are hidden so an open file shows only what
actually changed. Untracked files are listed too, with their content read from
disk as added lines when the row is opened.

Each file's diff lives in its own read-only text area, so the text is selectable
and **Ctrl+C** copies it as the plain diff. A gutter beside it carries the line
numbers: the new file's number for an added or unchanged line, the old file's
for a removed one, counted from the hunk headers so hidden context lines do not
throw them off. A changed line's stripe runs behind its number, which is itself
green or red. The numbers are painted, not part of the text, so a copied
selection is still a usable diff. In a build with
KF6SyntaxHighlighting the code inside a diff is colored by the file's own
language, and an added or removed line carries its color as a stripe across the
panel's full width instead of as text color, with the leading `+`/`-` keeping
the diff color.
Set `[review] syntax = false` for plain diff coloring. The panel draws itself in the
terminal's colors: the scheme named by `[terminal] color_scheme` (and the
`foreground`/`background` overrides there) supplies its text and, a shade
lighter than the terminal itself, its background,
green for added lines, red for removed ones, blue for hunk headers and magenta
for the file name, so the panel and the terminal next to it match.

This is a review of what is on disk, which is the point: an agent running in
the side terminal in auto-accept mode never asks for approval, so nothing else
shows what it wrote. While the panel is open it re-reads the worktree every
`[review] refresh_ms` milliseconds (2000 by default, `0` disables the poll and
leaves the **⟳** button), so changes appear as the agent writes them. A row
whose diff did not change is left untouched, so an open file stays open and a
selection inside it survives the refresh. A `git` that stops answering (a
repository on a dead network mount) is killed after `[review] timeout_ms` and
the panel says so, rather than freezing on the last diff it managed to read.

Set `[review] base` to diff against a revision (`"main"`, `"HEAD~3"`) instead of
the working tree against `HEAD`, `[review] show_context = true` to have rows
open on the full hunks, and `[review] file_max_height` to cap how tall one open
file grows before it scrolls on its own.

### Drag & drop

Drop files from your file manager onto Neobeam to open them.

**Onto the buffer area**, the file opens in the Neovim window under the cursor. Hold a modifier while dropping to choose placement:

| Modifier | Action |
|----------|--------|
| (none) | Open in the target window (`:edit`) |
| Shift | Open in a horizontal split (`:split`) |
| Ctrl | Open in a vertical split (`:vsplit`) |
| Alt | Open in a new tab (`:tabedit`) |

Dropping multiple files opens each one with the chosen command. Paths are passed to Neovim with proper escaping, so spaces and special characters are handled correctly.

**Onto the terminal**, the file path is pasted into the active shell (single-quoted) instead of being opened.

### Terminal links

Both terminals (bottom panel and side terminal) handle links the same way. URLs and e-mail addresses written with a scheme are underlined on hover and open in your browser on click; right-clicking one offers **Open Link** / **Copy Link**.

Some links cannot be recognized that way — a URL written without a scheme (`www.example.com`, `github.com/user/repo`), or one a TUI application wrapped across lines inside its own frame. For those, select the link text and press **Ctrl+Shift+L**: neobeam takes the link from the selection, joins fragments split across lines, and completes a missing scheme. A double-click selects a whole URL in one go (see `[terminal] word_characters`).

Links that an application only sends as an OSC 8 hyperlink (clickable text whose URL is never printed) cannot be opened at all — qtermwidget does not implement that escape sequence, so the URL never reaches neobeam.

## Configuration

Neobeam reads its config from `~/.config/neobeam/neobeam.toml`, or from the path given with `-c`/`--config`. On first run, a default config file is generated with all options commented out:

```toml
# Neobeam configuration

# [font]
# family = "Monospace"
# size = 10
# hinting = "default"  # none, slight, medium, full, default
# antialiasing = true

# [terminal]
# follow_cwd = true  # terminal follows nvim working directory
# start_expanded = false  # open with the terminal panel visible
# height = 300  # startup height in pixels
# font = ""  # terminal font (empty = use global font)
# font_size = 0  # terminal font size (0 = use global)
# color_scheme = "Neoblue"  # QTermWidget color scheme (Neoblue, OneDark, Dracula ship with neobeam)
# foreground = ""  # override scheme text color, e.g. "#f8f8f2" (empty = scheme default)
# background = ""  # override scheme background color (empty = scheme default)
# shell = ""  # shell program (empty = system default)
# shell_args = []  # arguments passed to shell
# exec_command = ""  # command to run after shell starts (e.g. "tmux")
# right_click_paste = false  # PuTTY-style right-click paste
# copy_on_select = true  # copy selection to PRIMARY + CLIPBOARD as it is made
# word_characters = ":@-./_~?&=%#+"  # non-alphanumerics kept when a double-click selects a word
# antialiasing = true  # font antialiasing in terminal
# hinting = "default"  # font hinting: none, slight, medium, full, default
# scrollback = 1000  # scrollback buffer size in lines
# refresh_prompt_on_git_change = false  # WARNING: sends Enter to the bottom terminal's
#   command line after a branch switch, to refresh a branch-aware shell prompt. Only
#   sent to splits with nothing typed into them; off by default since it injects a
#   keystroke into a live shell.

# [side_terminal]
# width = 400  # startup width in pixels
# font = ""  # side terminal font (empty = use terminal font)
# font_size = 0  # side terminal font size (0 = use terminal font size)

# [review]
# width = 600  # startup width of the change review panel, in pixels
# refresh_ms = 2000  # how often the open panel re-reads the worktree (floor 250, 0 = no poll)
# timeout_ms = 10000 # a git run that takes longer is killed (0 = wait forever)
# base = ""  # revision to diff against (empty = the worktree against HEAD)
# font = ""  # review font (empty = use the side terminal font)
# font_size = 0  # review font size (0 = use the side terminal font size)
# max_lines = 20000  # cap on the diff lines kept per file
# file_max_height = 600  # tallest an expanded file grows before it scrolls itself, in pixels
# show_context = false  # expand a file to the full hunks instead of the changed lines only
# syntax = true  # color the code in a diff by the file's language (needs KF6SyntaxHighlighting)
# The panel has no colors of its own: it follows [terminal] color_scheme and
# the foreground/background overrides there.

# [ui]
# ext_popupmenu = true
# ext_tabline = true
# ext_messages = true
# bufferline = true  # show open buffers instead of tabpages
# statusline = true  # native statusline (mode, file, position, diagnostics)
# git_status = true  # git branch + status counts in the statusline (push-updated, no polling)
# gitsigns_branch = false  # take the branch name from gitsigns instead of the built-in git provider
# accent_color = "#50fa7b"
# statusline_divider = "powerline"  # powerline, pipe, none
# focus_border_color = "#61afef"  # 1px border around the focused grid/terminal pane
# branch_switcher_font = "monospace"  # git branch switcher popup font family
# branch_switcher_font_size = 10  # its list rows; the input line is one point larger
# window_title = ""  # custom title template (empty = default "Neobeam - ~/path",
#                    # or "<project> - ~/path" after a switcher project pick).
#                    # Placeholders: %p project name, %d cwd (~-shortened), %D cwd,
#                    # %f file name, %F file path (~-shortened), %% literal %

# [mouse]
# terminal_selection = true   # terminal-style select/copy when vim mouse= is empty
# auto_copy = true            # auto-copy selection to clipboard on release
# selection_color = "#44475a" # selection highlight color

# [session]
# restore_window = true    # window size + editor/terminal proportions (WM/Wayland may override)
# restore_terminal = true  # terminal split count, proportions, expanded state

# [switcher]
# keep_running = true  # show the project switcher instead of quitting when nvim exits
# max_recent = 20      # recent files/folders kept in the switcher list (each)

# Pinned projects, shown on top of the project switcher list.
# Repeat the [[projects]] block once per project.
# [[projects]]
# path = "~/work/myproject"
# name = "myproject"      # optional display name (default: folder name)
# side_command = "claude" # optional command for the side terminal in this project
#
# [[projects]]
# path = "~/work/otherproject"  # only path is required; side terminal runs a plain shell
```

A ready-to-copy `neobeam.toml.example` is also included in the project root.

Run `neobeam -k` (optionally with `-c <path>`) to validate the config without starting the GUI. Unknown sections or options, wrong value types and invalid values (colors, enums) are reported as warnings; a value that fails validation falls back to its default instead of aborting the load.

### Configuration options

**`[font]`**
- `family` — Font family name (default: `"Monospace"`)
- `size` — Font size in points (default: `10`). Ctrl+=/Ctrl+- and Ctrl+wheel adjust the buffer
  font for the running session only; the next launch starts from this value again
- `hinting` — Hinting preference: `none`, `slight`, `medium`, `full`, `default` (default: `default`)
- `antialiasing` — Enable font antialiasing (default: `true`)

**`[terminal]`**
- `follow_cwd` — Terminal automatically changes to nvim's working directory (default: `true`)
- `start_expanded` — Open with the terminal panel visible (default: `false`)
- `height` — Terminal panel startup height in pixels (default: `300`)
- `font` — Terminal font family; empty uses the global font (default: `""`)
- `font_size` — Terminal font size in points; `0` uses the global font size (default: `0`)
- `color_scheme` — Color scheme name (default: `"Neoblue"`, neobeam's own dark blue scheme; `"OneDark"` — based on onedark.nvim with a slightly darker background than the editor — and `"Dracula"` also ship with neobeam). Any scheme installed with qtermwidget works too (`Linux`, `Nord`, `Solarized`, ...), as does a full path to a `.colorscheme` file
- `foreground` — Override the scheme's text color, e.g. `"#f8f8f2"`; empty keeps the scheme's color (default: `""`)
- `background` — Override the scheme's background color; empty keeps the scheme's color (default: `""`)
- `shell` — Shell program to launch; empty uses the system default (default: `""`)
- `shell_args` — Arguments passed to the shell (default: `[]`)
- `exec_command` — Command run after the shell starts, e.g. `"tmux"` (default: `""`)
- `right_click_paste` — PuTTY-style right-click paste in the terminal. Right-clicking a link opens the Open/Copy Link menu instead (default: `false`)
- `copy_on_select` — Copy the terminal selection to PRIMARY and CLIPBOARD as it is made (default: `true`)
- `word_characters` — Non-alphanumeric characters that count as part of a word when double-clicking in the terminal. The default extends Konsole's `":@-./_~"` with the query-string characters so a double-click selects a whole URL (default: `":@-./_~?&=%#+"`)
- `antialiasing` — Enable font antialiasing in the terminal (default: `true`)
- `hinting` — Terminal font hinting: `none`, `slight`, `medium`, `full`, `default` (default: `default`)
- `scrollback` — Terminal scrollback buffer size in lines (default: `1000`)
- `refresh_prompt_on_git_change` — **Warning: sends Enter to the bottom terminal's command line** after a branch switch (checkout/switch made in nvim or typed into the terminal itself), so a branch-aware shell prompt (e.g. zsh `vcs_info`) picks up the new branch name. Only sent to splits with nothing currently typed into them — a split mid-command is left alone — but it is still a keystroke injected into a live shell, so this is off by default. Not applied to the side terminal (default: `false`)

**`[side_terminal]`**
- `width` — Side terminal startup width in pixels (default: `400`)
- `font` — Side terminal font family; empty uses the `[terminal]` font (default: `""`)
- `font_size` — Side terminal font size in points; `0` uses the `[terminal]` font size (default: `0`)

**`[ui]`**
- `ext_popupmenu` — Native completion popup (default: `true`)
- `ext_tabline` — Native tab bar (default: `true`)
- `ext_messages` — Native message area (default: `true`)
- `bufferline` — Show open buffers in tabline instead of tabpages (default: `true`)
- `statusline` — Native statusline widget (default: `true`)
- `git_status` — Git branch + status counts in the statusline, push-updated with no polling (default: `true`)
- `gitsigns_branch` — Take the branch name from gitsigns (`vim.b.gitsigns_head`) instead of the
  built-in provider, which runs `git` itself and needs no plugin. Only affects the name; the
  counts always come from the built-in provider. Enable this if you want the branch to follow
  gitsigns' own detection, and accept that it goes stale on gitsigns' schedule (default: `false`)
- `accent_color` — Accent color for UI elements (default: `"#50fa7b"`)
- `statusline_divider` — Statusline segment divider style: `powerline`, `pipe`, `none` (default: `powerline`)
- `focus_border_color` — Color of the 1px border around the focused grid/terminal pane (default: `"#61afef"`)
- `branch_switcher_font` — Font family of the git branch switcher popup. A monospace family is what lets its commit-date column line up; a proportional one works too, the dates just stop aligning exactly (default: `"monospace"`)
- `branch_switcher_font_size` — Point size of the branch switcher's list rows; the input line above them renders one point larger. Accepted range 4-72, clamped with a warning outside it (default: `10`)
- `window_title` — Custom window title template. Empty uses the default title: `Neobeam - ~/path`, plain `Neobeam` for the home directory, or `<project name> - ~/path` after picking a pinned project in the switcher. Placeholders: `%p` project name (empty when no project is active), `%d` cwd (`~`-shortened), `%D` cwd (absolute), `%f` current file name, `%F` current file path (`~`-shortened), `%%` literal `%` (default: `""`)

**`[mouse]`**
- `terminal_selection` — Terminal-style select/copy in nvim buffers when vim `mouse=` is empty (default: `true`)
- `auto_copy` — Auto-copy the selection to the clipboard on release (default: `true`)
- `selection_color` — Selection highlight color (default: `"#44475a"`)

**`[session]`**
- `restore_window` — Restore the window size and the editor/terminal panel proportions. Best-effort: the window manager or Wayland compositor may override the size, and window position is ignored on Wayland and tiling WMs (default: `true`)
- `restore_terminal` — Restore the terminal split count, split proportions, and expanded state. When enabled, the remembered expanded state takes precedence over `[terminal] start_expanded` (default: `true`). The side terminal is only restored when the launch has a `side_command` (a pinned project opened as a folder); opening a single file starts with it collapsed, without forgetting the remembered state. Starting with no arguments has no project yet, so the side terminal opens when a project with a `side_command` is picked in the switcher

**`[switcher]`**
- `keep_running` — When nvim exits, keep neobeam running and show the project switcher instead of closing the window (default: `true`)
- `max_recent` — How many recent files and folders (each) are remembered for the switcher; `0` disables recents tracking (default: `20`)

**`[[projects]]`** (repeatable)
- `path` — Project folder, `~` expands to your home directory (required)
- `name` — Display name in the switcher; empty uses the folder name (default: `""`)
- `side_command` — Command run in the side terminal while this project is open, e.g. `"claude"` or `"nanocoder"`; empty runs a plain shell (default: `""`)

The side terminal command is per-project: opening a project (via the switcher or `neobeam <folder>`) selects the matching `side_command`. On a project switch the side terminal moves to the project directory and runs that command; if a command is involved (old or new), its splits are replaced with a fresh shell, while a plain shell with no command on either side is kept and just changes directory. Picking a project that defines a `side_command` also opens the side terminal if it was collapsed — projects without one leave the panel as it is.

Session state is stored separately from your config, in `~/.config/neobeam/session.ini` (managed automatically — you don't edit it). Recently opened files/folders for the switcher live there too.

## License

MIT
