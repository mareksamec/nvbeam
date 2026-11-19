# Nvbeam - Project Phases

## Phase 1: Skeleton + Connection ✓
- [x] CMake project scaffold with Qt6
- [x] Neovim process management (`nvim --embed`, msgpack-RPC over stdin/stdout)
- [x] `nvim_ui_attach` with `ext_linegrid`
- [x] Grid renderer painting the character grid from `grid_line` events

## Phase 2: Input + Usability ✓
- [x] Keyboard input (Qt key events → Neovim input notation)
- [x] Mouse input (clicks, scroll, drag)
- [x] Cursor rendering (block/beam/underline per mode, blinking)
- [x] Resize handling (`nvim_ui_try_resize`)

## Phase 3: Visual Polish ✓
- [x] Font configuration (family, size, bold/italic, hinting, antialiasing)
- [x] `ext_hlstate` semantic highlights
- [x] Smooth scrolling
- [x] TOML config loading (`~/.config/nvbeam/nvbeam.toml`)

## Phase 4: Multi-grid + UI Chrome ✓
- [x] `ext_multigrid` (per-window grids, floating windows, z-ordering)
- [x] Native `ext_cmdline`, `ext_popupmenu`, `ext_tabline` and `ext_messages`, configurable in `[ui]`
- [x] Native statusline (mode, file, position, diagnostics, configurable segments)

## Phase 5: Terminal + Advanced
- [x] Embedded QTermWidget (splits, follows nvim cwd)
- [x] Minimap (buffer content + highlight data)
- [x] Image rendering (via `vim.ui.img` API when available)

## Phase 6: Native Image Preview
- [ ] Custom RPC notification handler (`nvbeam_image_show`, `nvbeam_image_hide`)
- [ ] Signal pipeline: NvimProcess → MainWindow → ImageRenderer
- [ ] Bundled Lua plugin (`nvbeam_preview.lua`) with adapter pattern for file browsers:
  - [ ] netrw / Lexplore
  - [ ] oil.nvim
  - [ ] neo-tree.nvim
  - [ ] snacks.nvim explorer
  - [ ] telescope.nvim (preview pane)
- [ ] Configurable preview position (right half, floating, fixed column)
- [ ] Supported formats: PNG, JPEG, GIF, BMP, WebP, SVG
- [ ] Graceful fallback (no-op when file is not an image)
- [ ] Config option `[preview]` section: enable/disable, max size, position preference

## Phase 7: Improvements
- [x] Statusline dividers and selectable statusline text
- [x] Terminal-like mouse selection in nvim buffers when `mouse=` is empty, with auto-copy (`[mouse]` config)
- [x] Terminal splits: Ctrl+Shift+E splits, Ctrl+Shift+W closes, a button sends nvim's cwd to all terminals
- [x] Opening a folder ignores session restore and points nvim and the terminals at it
- [x] Ctrl+Tab cycles between the terminals and the nvim area
- [x] Side terminal next to the minimap for CLI agents, with per-project command and its own font
- [x] Ctrl+-/Ctrl+= and Ctrl+wheel change the terminal font size
- [x] Keep nvbeam running when nvim exits (`[switcher] keep_running`)
- [x] Project switcher: KRunner-style fuzzy search over pinned projects and recents, per-project side command
- [x] Window title `<project> - ~/path`, configurable via `[ui] window_title` placeholders
- [x] Config validation (`nvbeam -k`), unknown or invalid options reported as warnings
- [x] Terminal copy-on-select to PRIMARY + CLIPBOARD (`[terminal] copy_on_select`)
- [x] Fixed buffer separator artifacts and clipped error messages (`ext_messages` log below the buffer)
- [x] Git status in the statusline, push-updated via nvim autocmds and a `.git` watcher

## Phase 8: Text selection mode
- [x] Text selection mode (Ctrl+Shift+. or ⌶ button, Shift+drag one-shot): terminal-style selection without nvim visual mode, copied to CLIPBOARD + PRIMARY
- [x] Selection is clamped to its starting grid and follows the text on scroll
- [x] Fixed multi-line selections copying empty lines (grid 1 excluded from hit testing)
- [x] Shift+double-click selects and copies a word

## Pipeline adjustments
 - [x] Allow to publish release build from branch
 - [ ] Do not run build pipelines in PR/branch when only .md files are updated.
 - [x] OpenBSD build pipeline (manual `.github/workflows/openbsd.yml`) and port skeleton in `packaging/openbsd/`
 - [ ] Create comprehensive project documentation with examples on GitHub - suggest best approach.

## Fixes/Adjustments

### General
- [x] `-c` switch for a custom config file path
- [x] Example config file in the project root
### Term
- [ ] Enable sixel support for QTermWidget (blocked: upstream has no sixel support)
- [x] Shift+Insert and Ctrl+Shift+V paste the clipboard into the terminal
- [x] Optional PuTTY-style right-click paste
- [x] Terminal font antialiasing and hinting options
### Improvements 2026-08-03
- [x] Shift+Tab reaches the terminal instead of moving Qt focus (`MainWindow::focusNextPrevChild()`)
- [x] Terminal links: right-click Open/Copy Link, Ctrl+Shift+L opens the selected link, `[terminal] word_characters`
- [x] Side terminal starts collapsed when the launch has no `side_command`
- [x] `[side_terminal] width` is per split and remembered across sessions
- [x] Ctrl+Shift+X maximizes the editor, restoring the previous panel state on the second press
- [x] Bottom terminal prompt refresh on branch change (`[terminal] refresh_prompt_on_git_change`, off by default)

### Git segment 2026-08-21
- [x] Git segment follows checkouts made outside the app (directory watches, refresh on window activate)
- [x] Branch name comes from the built-in provider; `[ui] gitsigns_branch` restores gitsigns precedence

### Improvement segment 2026-08-31
- [x] Git segment tooltip lists changed and untracked files (capped at 20) and ahead/behind counts
- [x] Git branch switcher (click the git segment or Ctrl+Shift+B): fuzzy list, checkout, create branch
- [x] Ctrl+-/Ctrl+= and Ctrl+wheel change the nvim buffer font size (session only)
- [x] Ctrl+Shift+X also maximizes the bottom or side terminal
- [x] Project switcher path mode (Ctrl+D) with Tab completion against the directory tree

## Side TERM - AI agent features

### Claude
[ ] **Support diff view**
- [ ] IF Claude CLI supports it (does it have some api/control interface?) Enable if claude is running in the side term (how to do the detection properly? E.g. I use alias claude and claude-personal for personal account)
- [ ] (available in Claude Code cloud and some editors like Zed)
- [x] Diff view can be expanded next to the side term
- [x] Add button and a keyboard shortcut for it
- [x] Change review panel (Ctrl+Shift+R): live per-file diffs of the worktree or branch, syntax colors, image diffs

### Codex (ChatGPT)
- Same for Codex CLI if possible.


## Project rename
- [x] Renamed neobeam -> nvbeam across build, app identity, packaging, Lua/RPC names and docs; no migration of old user state


## New feature pack

**Diff view adjustment**
- Add some context to diff viewer to see 1-2 lines before and after added/removed content 
- Fix bug, where nvbeam would freeze when diff viewer is expanded a file is being edited.
- Diff viewer sometimes causes High CPU spikes when many new lines are added to a file or when a big new file is created.

**File switcher**
- After pressing Ctrl+P and then Ctrl+D the file switcher always prepends ~/ when the user is typing. Sometimes two ~/~/ end up being in front of the path
- Real fzf search tree in Ctrl+D mode - show more fuzzy options and sub-folders

**Light theme**
- Create a light themes: Catpuccin Late and vim-paper

## Upstream issues

### qtermwidget
- `QTermWidget::copyAvailable(bool)` is never emitted (verified in 2.4.0 sources). The only
  emitter is the `TerminalDisplay::selectionChanged()` slot, but the connection from
  `ScreenWindow::selectionChanged` to it is missing in `TerminalDisplay::setScreenWindow()`,
  so there is no usable selection-change notification in the public API.
  - Workaround in nvbeam: copy-on-select (`[terminal] copy_on_select`) installs an event
    filter on the internal `Konsole::TerminalDisplay` widget and copies the selection on
    left-button release (`TerminalPanel::eventFilter`).
  - If fixed upstream, the event filter can be replaced with a plain connect to
    `QTermWidget::copyAvailable`.
- No sixel image protocol support (blocks the "Enable sixel support" item above). QtermWidget issue here: [552](https://github.com/lxqt/qtermwidget/issues/552)
- No OSC 8 hyperlink support (verified in 2.4.0: `Vt102Emulation` parses no `ESC ] 8 ; ; uri ST`
  sequence, so the URI is discarded and only the display text reaches the screen buffer). TUI
  applications that print a label with a hidden URL are therefore unclickable in nvbeam, and no
  workaround is possible from the outside - the filter chain that produces link hotspots is not
  reachable through the public API either (`TerminalDisplay::filterChain()` needs a header
  qtermwidget does not install).
  - Workaround for the cases that *are* recoverable (scheme-less URLs, links an application wrapped
    across lines): select the link and press Ctrl+Shift+L (`TerminalPanel::openSelectedLink`).
