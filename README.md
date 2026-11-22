# Nvbeam

A fast, native Neovim GUI client built with Qt6 and C++.

Nvbeam connects to an embedded Neovim instance via msgpack-RPC and renders the editor using native Qt widgets. It provides a VSCode-like experience with a native terminal panel, minimap, tabline, command line, and completion popup — all driven by Neovim's UI protocol extensions.

I wanted nvim gui with native terminal + some simple way to run cli coding agents so I decide to vibecode Nvbeam to fit my needs 100%.


> [!NOTE]
> This is still Alpha version - check [known bugs](#known-bugs) in the bottom!
> 
> **Current use case:**
> - Neovim + some file manager like Snacks, Neotree, etc.
> - Nvbeam projects to easily switch between your projects with Ctrl + Shift + P.
> - Or open a directory by hitting Ctrl + Shift + P and then Ctrl + D.
> - Configure coding agent commands per project to start in the side terminal e.g. `claude --continue`
> - Easily switch Git branches with Ctrl + Shift + B.

## Features

- **Full Neovim integration** — ext_linegrid, ext_multigrid, ext_hlstate, ext_cmdline, ext_popupmenu, ext_tabline, ext_messages
- **Native statusline** — mode indicator, file, git branch, diagnostics, position
- **Git branch switcher** – VSCode-style branch picker under the statusline: click the git segment (or Ctrl+Shift+B) to switch branches or create a new one
- **Embedded terminal** — QTermWidget-based, with splits and nvim cwd following (Ctrl+\` to toggle), resizable via a draggable divider
- **Change review** — panel next to the side terminal listing each changed file, expandable to its colored, selectable diff (Ctrl+Shift+R), refreshing itself while an agent writes
- **Minimap** — zoomed-out buffer view with syntax highlighting, viewport indicator, click-to-scroll
- **Configurable fonts** — family, size, hinting, antialiasing
- **Drag & drop** — drop files onto the buffer area to open them in the window under the cursor; modifier keys choose split/tab placement. Dropping onto the terminal pastes the path
- **TOML configuration** — sensible defaults, all options commented out in the generated config
- **Session restore** — remembers window size, terminal splits/proportions, panel height, and expanded state
- **Wayland native** — via qt6-wayland
- **Image rendering** — *(Planned)* placeholder for upcoming `vim.ui.img` API

## Install

### Arch Linux (AUR)

```sh
yay -S nvbeam-bin   # prebuilt, from the latest release
yay -S nvbeam-git   # built from the latest main
```

Any AUR helper works the same way. Without one:

```sh
git clone https://aur.archlinux.org/nvbeam-bin.git
cd nvbeam-bin
makepkg -si
```

### Other distributions

Each [GitHub release](https://github.com/mareksamec/nvbeam/releases) has a `.deb`, an `.rpm` and an Arch package. Otherwise build from source as described below.

## Requirements

- Neovim 0.10+
- Qt 6.x (Core, Gui, Widgets)
- CMake 3.20+
- C++20 compiler
- qtermwidget6 (optional, for embedded terminal)
- KF6SyntaxHighlighting (optional, for code coloring in the review panel's diffs)

## Build

```sh
git clone https://github.com/mareksamec/nvbeam.git
cd nvbeam
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

To enable the embedded terminal (requires qtermwidget6 installed):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNVBEAM_TERMINAL=ON
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
./build/nvbeam [options] [-- nvim-args...]
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
| Ctrl+Shift+P | Toggle the project switcher (Ctrl+D inside it switches between project search and path mode, Ctrl+P pins a folder as a project, Ctrl+N creates a new folder) |
| Ctrl+Shift+B | Toggle the git branch switcher, from anywhere in the window including inside a terminal (also opens by clicking the statusline's git segment) |
| Ctrl+Shift+L | Open the link in the focused terminal's selection |
| Ctrl+Shift+R | Toggle the change review panel (git diff of the worktree, opens right of the side terminal) |
| Ctrl+Shift+D | Switch the review panel between the worktree and the branch comparison |

### Project switcher

Starting nvbeam without arguments, or pressing **Ctrl+Shift+P**, opens a KRunner-style switcher: type to fuzzy-search pinned projects (`[[projects]]` in the config) and recently opened folders and files. Selecting an entry points nvim's cwd and every terminal at the project directory and opens it in nvim. When nvim exits (`:q`), nvbeam shows the switcher again instead of closing (`[switcher] keep_running = false` turns that off). **Ctrl+D** switches to path mode, where Tab completes the typed path against the directory tree, for opening anything that is neither pinned nor recent.

**Ctrl+N** (or the folder button at the right edge of the input) creates a new folder. In path mode it creates the typed path, parents included; in project search it first switches to path mode so the name can be typed. A typed path that does not exist also shows a `+ Create folder` row at the end of the list. The new folder is then listed in the input, so Enter opens it and Ctrl+P pins it.

**Ctrl+P** pins the highlighted folder (or the typed path) as a project and asks for its `side_command`, prefilled with whatever the side terminal is running; clear it for a plain shell. Ctrl+P on a pinned project (◇) removes it again. Pins are written to `~/.config/nvbeam/projects.toml` in the same `[[projects]]` format, so moving one into `nvbeam.toml` is a copy-paste; projects declared there (◆) cannot be removed from the switcher.

### Git branch switcher

Clicking the statusline's git segment, or pressing **Ctrl+Shift+B** from anywhere (terminals included), opens a VSCode-style branch picker. It lists remote-only branches first, then local ones, most recently used first, with the checked-out branch marked `◆`. Typing fuzzy-filters the list, Enter checks the branch out (a remote-only entry gets a local tracking branch) and Esc closes it. Typing a name no branch has adds a **Create branch "…"** row at the end, which runs `git checkout -b <name>`.

When git refuses (uncommitted changes in the way, a branch checked out in another worktree), the popup stays open and shows git's message. On success the statusline updates, open buffers are rechecked against the new working tree and the bottom terminal's shell prompt is refreshed. The popup font is set with `[ui] branch_switcher_font` and `branch_switcher_font_size`, and the switcher needs `[ui] git_status` to be on.

### Change review

The **◧** button on the side terminal's toolbar, or **Ctrl+Shift+R**, opens a review panel to the right of that terminal. It lists every changed file, untracked ones included, with its `+added -removed` counts; clicking a row expands its diff, and changed images are shown as images, old and new side by side. **Ctrl+Shift+D** (or the **⎇** button) switches between **worktree** mode (the worktree against `HEAD`) and **branch** mode (the merge base with the default branch against the worktree, so everything the branch adds). The default branch comes from `[review] base_branch`, then `origin/HEAD`, then a local `main` or `master`.

The panel exists to show what an agent in the side terminal writes, even in auto-accept mode: while open, it re-reads the worktree every `[review] refresh_ms` (2000 ms by default) and keeps open rows and selections across refreshes. Diff text is selectable and Ctrl+C copies it as a plain diff. In a build with KF6SyntaxHighlighting the code is colored by its language (`[review] syntax`), and the panel takes its colors from the terminal's `color_scheme`. The remaining `[review]` options are listed in the config below.

### Drag & drop

Drop files from your file manager onto the buffer area to open them in the Neovim window under the cursor. Hold a modifier while dropping to choose placement:

| Modifier | Action |
|----------|--------|
| (none) | Open in the target window (`:edit`) |
| Shift | Open in a horizontal split (`:split`) |
| Ctrl | Open in a vertical split (`:vsplit`) |
| Alt | Open in a new tab (`:tabedit`) |

Dropping onto a terminal pastes the file path (single-quoted) into the shell instead of opening it.

### Terminal links

In both terminals, URLs and e-mail addresses written with a scheme are underlined on hover and open in your browser on click; right-clicking one offers **Open Link** / **Copy Link**.

For a URL without a scheme (`github.com/user/repo`) or one a TUI wrapped across lines, select it and press **Ctrl+Shift+L**: nvbeam joins the fragments and completes the scheme. Links sent only as OSC 8 hyperlinks (the URL is never printed) cannot be opened, because qtermwidget does not implement that escape sequence.

## Configuration

Nvbeam reads its config from `~/.config/nvbeam/nvbeam.toml`, or from the path given with `-c`/`--config`. On first run, a default config file is generated with all options commented out:

```toml
# Nvbeam configuration

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
# color_scheme = "Neoblue"  # QTermWidget color scheme (default: the [ui] theme's; Neoblue, OneDark,
#                           # Dracula, CatppuccinLatte, VimPaper ship with nvbeam)
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
# width = 400  # startup width of one side terminal split, in pixels (per side command)
# font = ""  # side terminal font (empty = use terminal font)
# font_size = 0  # side terminal font size (0 = use terminal font size)

# [review]
# width = 600  # startup width of the change review panel, in pixels
# refresh_ms = 2000  # how often the open panel re-reads the worktree (floor 250, 0 = no poll)
# timeout_ms = 10000 # a git run that takes longer is killed (0 = wait forever)
# base = ""  # revision to diff against (empty = the worktree against HEAD)
# mode = "unstaged"  # what the panel opens on: "unstaged" (the worktree against HEAD)
#                    # or "branch" (the merge base with the default branch against
#                    # the worktree); Ctrl+Shift+D switches at runtime
# base_branch = ""  # branch mode's comparison branch (empty = origin/HEAD, then main, then master)
# font = ""  # review font (empty = use the side terminal font)
# font_size = 0  # review font size (0 = use the side terminal font size)
# max_lines = 20000  # cap on the diff lines kept per file
# file_max_height = 600  # tallest an expanded file grows before it scrolls itself, in pixels
# show_context = false  # expand a file to the full hunks instead of the changed lines only
# context_lines = 2  # unchanged lines shown around each change (0-3, 0 = changed lines only)
# syntax = true  # color the code in a diff by the file's language (needs KF6SyntaxHighlighting)
# image_preview = true  # show a changed image as an image instead of git's "Binary files differ"
# image_max_bytes = 10485760  # larger images are not decoded (10 MiB)
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
# theme = "dark"  # chrome palette: dark, catppuccin-latte, vim-paper
#                  # (also picks the terminal color_scheme when that is not set)
# accent_color = "#50fa7b"  # overrides the theme's accent
# statusline_divider = "powerline"  # powerline, pipe, none
# focus_border_color = "#61afef"  # 1px border around the focused grid/terminal pane
# branch_switcher_font = "monospace"  # git branch switcher popup font family
# branch_switcher_font_size = 10  # its list rows; the input line is one point larger
# window_title = ""  # custom title template (empty = default "Nvbeam - ~/path",
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
# path_max_depth = 6   # folder levels path mode searches outside a git repository
# path_max_entries = 50000  # cap on the paths path mode searches

# Pinned projects, shown on top of the project switcher list.
# Repeat the [[projects]] block once per project.
# [[projects]]
# path = "~/work/myproject"
# name = "myproject"      # optional display name (default: folder name)
# side_command = "claude" # optional command for the side terminal in this project
# side_command_2 = "nanocoder" # optional second side terminal split
# side_command_3 = ""          # optional third side terminal split
#
# [[projects]]
# path = "~/work/otherproject"  # only path is required; side terminal runs a plain shell
```

A ready-to-copy `nvbeam.toml.example` is also included in the project root.

Run `nvbeam -k` (optionally with `-c <path>`) to validate the config without starting the GUI. Unknown sections or options, wrong value types and invalid values (colors, enums) are reported as warnings; a value that fails validation falls back to its default instead of aborting the load.

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
- `color_scheme` — Color scheme name (default: the `[ui] theme`'s, `"Neoblue"` for `dark`, nvbeam's own dark blue scheme; `"OneDark"` — based on onedark.nvim with a slightly darker background than the editor — `"Dracula"`, and the light `"CatppuccinLatte"` and `"VimPaper"` also ship with nvbeam). Any scheme installed with qtermwidget works too (`Linux`, `Nord`, `Solarized`, ...), as does a full path to a `.colorscheme` file
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
- `width` — Startup width of one side terminal split, in pixels. The panel gets this much per split, so a project with `side_command_2`/`side_command_3` opens twice or three times as wide (capped so the editor area keeps at least 200 px). Dragging the handle redefines the per-split width, and `[session] restore_window` brings it back on the next launch (default: `400`)
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
- `theme`: Palette of nvbeam's own chrome (statusline, bufferline, command line, messages, popups, switchers): `dark`, `catppuccin-latte` or `vim-paper`. It also picks the matching terminal `color_scheme` (`Neoblue`, `CatppuccinLatte`, `VimPaper`) when `[terminal] color_scheme` is not set, and the review panel follows that scheme. The nvim buffer keeps your nvim colorscheme, so set the matching one in nvim yourself (for example `catppuccin-latte` from catppuccin/nvim, or `paper` from vim-paper). `accent_color`, `focus_border_color` and `[mouse] selection_color` override the theme's colors (default: `"dark"`)
- `accent_color` — Accent color for UI elements (default: the theme's, `"#50fa7b"` for `dark`)
- `statusline_divider` — Statusline segment divider style: `powerline`, `pipe`, `none` (default: `powerline`)
- `focus_border_color` — Color of the 1px border around the focused grid/terminal pane (default: the theme's, `"#61afef"` for `dark`)
- `branch_switcher_font` — Font family of the git branch switcher popup. A monospace family is what lets its commit-date column line up; a proportional one works too, the dates just stop aligning exactly (default: `"monospace"`)
- `branch_switcher_font_size` — Point size of the branch switcher's list rows; the input line above them renders one point larger. Accepted range 4-72, clamped with a warning outside it (default: `10`)
- `window_title` — Custom window title template. Empty uses the default title: `Nvbeam - ~/path`, plain `Nvbeam` for the home directory, or `<project name> - ~/path` after picking a pinned project in the switcher. Placeholders: `%p` project name (empty when no project is active), `%d` cwd (`~`-shortened), `%D` cwd (absolute), `%f` current file name, `%F` current file path (`~`-shortened), `%%` literal `%` (default: `""`)

**`[mouse]`**
- `terminal_selection` — Terminal-style select/copy in nvim buffers when vim `mouse=` is empty (default: `true`)
- `auto_copy` — Auto-copy the selection to the clipboard on release (default: `true`)
- `selection_color` — Selection highlight color (default: the `[ui] theme`'s, `"#44475a"` for `dark`)

**`[session]`**
- `restore_window` — Restore the window size, the editor/terminal panel proportions and the side terminal's per-split width. Best-effort: the window manager or Wayland compositor may override the size, and window position is ignored on Wayland and tiling WMs (default: `true`)
- `restore_terminal` — Restore the terminal split count, split proportions, and expanded state. When enabled, the remembered expanded state takes precedence over `[terminal] start_expanded` (default: `true`). The side terminal is only restored when the launch has a `side_command` (a pinned project opened as a folder); opening a single file starts with it collapsed, without forgetting the remembered state. Starting with no arguments has no project yet, so the side terminal opens when a project with a `side_command` is picked in the switcher

**`[switcher]`**
- `keep_running` — When nvim exits, keep nvbeam running and show the project switcher instead of closing the window (default: `true`)
- `max_recent` — How many recent files and folders (each) are remembered for the switcher; `0` disables recents tracking (default: `20`)
- `path_max_depth`: How many folder levels path mode's recursive search walks outside a git repository. Inside one, `git ls-files` lists the files instead, with `.gitignore` applied (default: `6`, range 1-20)
- `path_max_entries`: Cap on the files and folders path mode's recursive search collects (default: `50000`, range 100-1000000)

**`[[projects]]`** (repeatable)
- `path` — Project folder, `~` expands to your home directory (required)
- `name` — Display name in the switcher; empty uses the folder name (default: `""`)
- `side_command` — Command run in the side terminal while this project is open, e.g. `"claude"` or `"nanocoder"`; empty runs a plain shell (default: `""`)
- `side_command_2`, `side_command_3` — Further commands, each in its own side terminal split, so several agents run next to each other (default: `""`)

The side terminal commands are per-project: opening a project (via the switcher or `nvbeam <folder>`) selects the matching entry and opens one split per configured command, side by side. On a project switch the side terminal moves to the project directory and runs the new project's commands; if a command is involved (old or new), its splits are replaced with a fresh shell per command, while a plain shell with no command on either side is kept and just changes directory. Picking a project that defines a command also opens the side terminal if it was collapsed — projects without one leave the panel as it is.

Splits you open yourself with **+** (Ctrl+Shift+E) run a plain shell, never the project's command, so splitting a panel that is already running an agent does not start a second copy of it.

Projects pinned from the switcher with Ctrl+P are written to `~/.config/nvbeam/projects.toml` in this same format, so an entry can be moved here by copying the block. A path present in both files is shown once, and this file wins, which is how a pin gets a `side_command` it was not created with.

Session state is stored separately from your config, in `~/.config/nvbeam/session.ini` (managed automatically — you don't edit it). Recently opened files/folders for the switcher live there too.

## Contributing

Bug reports and pull requests are welcome. `CONTRIBUTING.md` covers the build,
how changes are verified (unit tests for the non-UI core, running the app for
everything else), and the conventions the code follows. `PROJECT.md` holds the roadmap and the reasoning
behind existing design decisions; `AGENTS.md` states the project's scope.


## Known bugs
- Minimap sometimes does not show the whole file.
- Side term scaling issues when resizing the window (e.g. during tiling)

## Roadmap
- Fix bugs (obviously 😇)
- Add full X11/XLibre support
- Find/Create better alternative to QTermWidget with Sixel support, better font hinting, etc.

## License

MIT
