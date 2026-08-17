# Neobeam - Project Phases

## Phase 1: Skeleton + Connection ✓
- [x] CMake project scaffold with Qt6
- [x] Neovim process management (spawn `nvim --embed`, msgpack-RPC over stdin/stdout)
- [x] `nvim_ui_attach` with `ext_linegrid`
- [x] Grid renderer (QWidget painting character grid from `grid_line` events)
- [x] Build verified, nvim process connects successfully

## Phase 2: Input + Usability ✓
- [x] Keyboard input (Qt key events → Neovim input notation)
- [x] Mouse input (clicks, scroll, drag)
- [x] Cursor rendering (block/beam/underline per mode, blinking)
- [x] Resize handling (`nvim_ui_try_resize`)

## Phase 3: Visual Polish ✓
- [x] Font configuration (family, size, bold/italic, hinting, antialiasing)
- [x] `ext_hlstate` semantic highlights
- [x] Smooth scrolling (deferred to Phase 4 — needs per-window grids)
- [x] TOML config loading (~/.config/neobeam/neobeam.toml)

## Phase 4: Multi-grid + UI Chrome ✓
- [x] `ext_multigrid` (per-window grids, floating windows, z-ordering)
- [x] `ext_cmdline` (native command line widget)
- [x] `ext_popupmenu` (native completion popup)
- [x] `ext_tabline` (native tab bar)
- [x] `ext_messages` (native message area)
- [x] All UI extensions configurable in `[ui]` section of config
- [x] Native statusline (Qt widget, mode/file/position/diagnostics, configurable segments)

## Phase 5: Terminal + Advanced
- [x] Embedded QTermWidget (splits, follows nvim cwd)
- [x] Minimap (buffer content + highlight data)
- [x] Image rendering (via `vim.ui.img` API when available)

## Phase 6: Native Image Preview
- [ ] Custom RPC notification handler (`neobeam_image_show`, `neobeam_image_hide`)
- [ ] Signal pipeline: NvimProcess → MainWindow → ImageRenderer
- [ ] Bundled Lua plugin (`neobeam_preview.lua`) with adapter pattern for file browsers:
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
- [x] Improve the statusline (dividers, etc)
- [x] Mouse - terminal like mouse behaviour in nvim buffers on mouse= setting in vim. Allow selection with cursor with auto copy of selected test to clipboard that won't trigger nvim VISUAL mode on click or mouse drag.
- [x] Consider adding mouse behaviour to .toml config
- [x] Make text in the UI statusline selectable.
- [x] Ctrl + Shift + E splits the terminal
- [x] Ctrl + Shift + W closes active split
- [x] Instead add (arrow down button) that sends current nvim cwd to all open terminals.
- [x] When a folder is open with neobeam, session restore should be ignored and the folder path should be open both in nvim buffer and terminal cwd.
- [x] Ctrl+Tab anywhere in the neobeam should cycle between terminals and nvim area.
- [x] Side terminal - apart from bottom terminal pane add button to open side temrinal. This will be in the space next to the minimap. It is intended for the to run things like claude or nanocoder in the cli. It should always open in the same folder as current nvim cwd:
    - [x] Add config option for default command to run with side terminal e.g. claude, nanocoder
    - [x] Side terminal is not affected by the "Send cwd to all terminals button"
- [x] Add independent font configuration for the side terminal.
- [x] For all terminals, add keyboard shortcuts Ctrl - to decrease the font size and Ctrl = to increase it. Holding Ctrl + mouse wheel up and down will also increase/decrease the term font.
- [x] Add option to keep neobeam running when nvim buffer is exited
- [x] Project switcher
    - [x] Add project switcher - when neobeam is started without argument a project switcher with fzf search through recently opened folders and files (Like nvim Telescope)
    - [x] Switcher can use qt gui elements with typing area and a drop down list, maybe something like k runner in the middle of open neobeam window
    - [x] When a file is selected, cwd of nvim and all terminals (including side term) changes to the directory where the selected file is.
    - [x] You can also projects in neobeam.toml that will be on top of the switcher list.
    - [x] You can configure side term command for each specific folder or leave it without any command.
    - [x] We will remove the default side term command from the config
    - [x] When a project is selected with Project switcher, Neobeam window title should change to <Project name> - <path> the word "Neobeam" Should not show. e.g.:  Myproject - ~/projects/myproject -> project name shows while nvim cwd stays in the project dir; cd elsewhere reverts to "Neobeam - ~/path". New neobeam.title debug trace category for headless checks.
    - [x] Configurable window title: [ui] window_title template, empty = default behaviour. Placeholders %p project name, %d cwd (~), %D cwd (absolute), %f file name, %F file path (~), %% literal. Current file pushed via existing neobeam_file BufEnter notification; unknown placeholders render literally and warn in `neobeam -k`.
- [x] Config
    - [x] Add simple config validation e.g. `neobeam -k`
    - [x] Handle non-existent or wrong options in the .toml config.
- [x] **Term + Project improvements**
    - [x] Terminal select + copy to PRIMARY + CLIPBOARD 
        - [x] Add separate [terminal] config option for this? -> yes, [terminal] copy_on_select (default true)
        - [x] How to handle possible conflict with tools like wl-copy? -> no conflict; standard last-writer-wins selection semantics, no coordination needed
    - [x] Neobeam window title
        - [x] Make Neobeam uppercase
        - [x] Append project path or cwd folder path (if no project selected) e.g. Neobeam - ~/projects/neobeam
    - [x] Nvim buffer artifacts
        - [x] Sometimes when snacks.nvim file manager or nvim-tree is opened it leaves the vertical line in the buffer. Happens mainly when Neobeam window is stretched wide.
        - [x] Nvim terminal buffers (when you open one in the neobeam nvim buffer) also leaves the horizontal buffer separator artifact in the neobeam nvim buffer when closed.
        - [x] When there is a warning or error displayed in the nvim buffer statusline it is clipped. (e.g. lua plugin error, or some vim script error etc.) -> new approach on fix/warning-error-area: ext_messages now actually passed at ui_attach (was configured but never sent, so the message grid was in use); MessageWidget reworked into a timestamped, expandable 2-line message log below the buffer ("Messages" toggle, scrollbar, selectable + auto-copy like statusline). Verified headless (msg_show kinds echomsg/lua_error arrive incl. full traceback, msg_set_pos gone); awaiting visual confirmation
    - [x] Neobeam UI statusline:
        - [x] Add git status (optimize performance avoid frequent scanning, or use push model) -> push model: nvim autocmd rpcnotify + .git HEAD/index file watcher + cwd changes, debounced async `git status`; counts in accent_color ([ui] git_status option)

## Phase 8: Text selection mode
- [x] Implement mode where nvim buffer in neobeam behaves as if nvim is set to mouse= (mouse off)
    - [x] The cursor is set to beam and you have a selection in the term. -> mouse pointer becomes I-beam while mode is on; nvim caret stays nvim-controlled
    - [x] Selected text is copied to Clipboard -> CLIPBOARD + PRIMARY, same targets as the terminal panels; explicit selections copy regardless of [mouse] auto_copy
    - [x] Mode can be toggled with a button or keyboard shortcut Ctrl + Shift + . -> both: checkable "⌶" button in the bottom-right strip + Ctrl+Shift+.
    - [x] Holding Shift and drag selecting will also trigger this mode if not active (terminal like selectin behaviour) -> one-shot for that drag only; press is deferred so a plain Shift+click still reaches nvim as <S-LeftMouse>
    - [x] Drag selection in this mode won't trigger nvim visual selection mode.
    - [x] Fix uncovered along the way: drag coords are now clamped to the grid the selection started in, so dragging across a split/float no longer produces a garbage selection.
    - [x] Selection highlight follows the text on scroll (shifted by grid_scroll deltas); dropped once the selected rows scroll out of the window instead of floating over new content.
    - [x] Fix: multi-line selection copied empty lines. gridAt() iterated m_grids (QHash, randomized order) and grid 1 - the full-window backdrop that holds no buffer text under ext_multigrid - could match before the window grid, so the overlay drew at the right pixels but extraction read blank cells. Grid 1 is now excluded from the hit loop and only used as fallback. -> awaiting visual confirmation
    - [x] Shift + double click (or double click while the mode is on) selects the word under the pointer and copies it (xterm-style char classes: word = alnum + underscore). Previously there was no double-click handler at all: the deferred Shift+press leaked to nvim as two quick <S-LeftMouse> clicks, nvim drew its own visual word selection, and nothing was copied. A dragless Shift+click is now held for the double-click interval before being forwarded (flushed early on any other press/keypress), so plain Shift+click still reaches nvim as <S-LeftMouse>. -> awaiting visual confirmation

## Phase 9: Performance

Baseline: stable 20-25% CPU while editing on i7-1185G7. Suggested fix order (quick wins first):

- [x] Minimap: debounce the content rebuild (treesitter scan) to ~300ms after the last changedtick change; keep only the cheap viewport-only query on the short per-flush interval. -> split into neobeam.minimap_view() (per flush) + neobeam.minimap_scan() (300ms debounce, restarted while ticks keep changing)
- [x] Minimap: skip the scan entirely when the minimap is hidden. -> both timers no-op while hidden; re-show kicks one view poll which schedules a scan if the tick moved
- [x] Minimap: persist the lua fg_cache across calls in a global table, invalidate on ColorScheme autocmd.
- [x] Lua plumbing: stop re-reading .lua files from disk and re-compiling full chunks per timer fire. -> both: loadLua() caches sources, and statusline/bufferline/minimap register functions in a `neobeam` table on attach; timers call one-line `return neobeam.*()` chunks
- [x] Replace idle polling with push: cwd poll (500ms) and MRU poll (2s) become `DirChanged`/`BufEnter` autocmds + rpcnotify (lua/events.lua), same pattern as gitstatus.lua. Initial cwd/file pushed once at registration.
- [ ] Paint path, step 1: cache font descent + four QFont variants (normal/bold/italic/bold-italic) as members; batch consecutive cells with the same hlId into runs (one fillRect + one drawText per run). Currently one drawText per cell, ~15k cells per full repaint.
- [ ] Paint path, step 2: per-grid QPixmap backing store updated incrementally. grid_line paints only changed rows, grid_scroll becomes a blit, paintEvent composites pixmaps + cursor + selection overlay. Cursor-only moves repaint two small rects via update(rect) instead of the whole window.
- [x] Decoder quick win: use `QString::fromUtf8(data.constData() + offset, len)` in decodeMsgpack to avoid the intermediate mid() copy per string.
- [x] Fix uncovered along the way: infinite RPC feedback loop at idle. nvim re-emits a win_viewport_margins + tabline_update + flush batch after every RPC request it serves, and each flush re-armed the statusline/bufferline/minimap pollers whose next request triggered the same batch again. flushed() now fires only for batches carrying real content events. This loop was likely a large share of the 20-25% baseline; it also starved the debounced minimap scan (dark minimap).
- [x] Debug/verify infra: QLoggingCategory trace points (neobeam.minimap, neobeam.redraw; silent by default, QT_LOGGING_RULES to enable) + project verify skill (.claude/skills/verify) covering offscreen runs and RPC-level Lua testing.
- [ ] Decoder, longer term (only if still needed): decode redraw events directly into grid cells without building QVariant trees. Do not start here.


## Pipeline adjustments
 - [ ] Allow to publish release build from branch
 - [ ] Do not run build pipelines in PR/branch when only .md files are updated.
 - [x] Add BSD build pipeline -> separate manual-only workflow (.github/workflows/openbsd.yml, vmactions/openbsd-vm, workflow_dispatch with release input); OpenBSD port skeleton in packaging/openbsd/
 - [ ] Create comprehensive project documentation with examples on GitHub - suggest best approach.

## Fixes/Adjustments

### General
- [x] Add switch -c to start neobeam with a custom config file path
- [x] Create example config file in the project root
### Term
- [ ] Enable sixel support for QTermWidget (blocked: upstream has no sixel support)
- [x] Add Shift+Insert and Ctrl+Shift+V bindings to paste clipboard to terminal
- [x] Add option to enable PuTTY-style right-click paste in the terminal to the config
- [x] Add options to enable text anti-aliasing and hinting for terminal font
### Improvements 2026-08-03
- [x] It seems something in neobeam terminals captures Shift+Tab for the terminals (e.g. Shift+Tab is used in Codex CLI)
    - It was Qt's own focus navigation: `Konsole::TerminalDisplay::focusNextPrevChild()` swallows
      plain Tab but deliberately leaves Shift+Tab (`Key_Backtab`) to the window, which moved focus
      out of the panel. `MainWindow::focusNextPrevChild()` now refuses navigation while a terminal
      has focus, so Qt falls through to the terminal's key handler and the shell gets `ESC[Z`.
- [x] Some links are still not clickable (when part of some TUI apps). Check if handling is same for the side terminal as main terminal.
    - Handling is identical for both panels: every split of both is created by
      `TerminalPanel::createTermWidget()`, which is the only place `urlActivated` is wired.
    - qtermwidget's `UrlFilter` only matches URLs with an explicit scheme, on a single line.
      Added on the neobeam side: right-click on a detected link offers Open/Copy Link (it used to
      just paste with `right_click_paste`), a wider matcher behind Ctrl+Shift+L that resolves the
      link in the terminal selection (scheme-less hosts, e-mail, fragments an application wrapped
      across lines), and `[terminal] word_characters` so a double-click grabs a whole URL.
    - Still impossible: OSC 8 hyperlinks (see Upstream issues below) - the URL never reaches the
      screen buffer, so nothing in neobeam can recover it.
- [x] If Neobeam is opened without side terminal command e.e.g "Open file with Neobeam" or from file picker, start with side terminal collapsed)
    - The saved "side terminal was visible" state is only restored when the launch resolves a
      `side_command`, and a suppressed panel does not overwrite that saved state on close, so the
      next launch inside the project still brings it back.
    - Starting with no arguments (switcher, also the `keep_running` restart after `:q`) resolves no
      project at startup, so `applyProjectSideCommand()` now opens the side terminal when the picked
      project defines a `side_command` - that path never showed the panel at all before.
- [x] Add "maximized" mode - Ctrl + Shift + x would hide all terminals, side terminal and also messages. Pressing it again will restore it to the previous state.
    - `MainWindow::toggleMaximizedMode()` captures which of the bottom terminal, side terminal and
      messages panel are actually visible before hiding them, so a panel the user had already closed
      stays closed on restore instead of popping back open.
    - Driven through the existing toggle buttons/`MessageWidget::hidePanel()`/`showPanel()` rather than
      raw `hide()`/`show()`, so button check-state and session persistence stay consistent with normal
      manual toggling.
    - Side terminal restore calls `show()` directly instead of going through its toggle button: that
      button's handler re-fetches nvim's cwd and focuses the terminal, which is right when a user opens
      it fresh but wrong when just restoring pre-maximize state. `showEvent`'s own `visibilityChanged`
      still keeps the button's checked state in sync (same pattern session restore already relies on).
      Keyboard focus is explicitly returned to the grid on both enter and exit either way.
- [ ] Advanced git stats. I'd like that when I hover my mouse over git section of neobeam statusline I'll see some more detailed git stats (hover frame with text). It would have list of changed files. Number of commits etc. Changed files would be in OneDark green. If the list of changed file is too long. show just ...and 20 more files, etc. Mind the performance for this one.
- [x] Terminal prompt injection / refresh on git status. I'd like the bottom terminal to listen for git changes and when there is a new branch to either refresh / inject the terminal prompt (zsh or bash, depending what's enable in the config). Simple fallback solution would be to just send "Enter" into the terminal (if no letters are typed there to avoid executing some random command).
    - Went with the fallback: `GitStatus` (already push-updated via nvim autocmds + a
      `.git/HEAD`/`.git/index` file watcher, so it already sees branch switches typed
      straight into the terminal) gained a `branchChanged(QString)` signal, emitted only
      when the branch itself differs from the previous status, not on every status-count
      tick. `TerminalPanel` connects it to `refreshPromptOnBranchChange()`, which sends a
      bare `\r` to every split whose current line has nothing unsubmitted typed into it.
      Bottom terminal only, not the side terminal (usually an agent/REPL, not a shell -
      Enter there risks submitting an empty prompt to it).
    - "Nothing typed" has no public API to check (QTermWidget exposes no way to read the
      shell's readline buffer or the current screen line), so it's tracked as a heuristic:
      an event filter on each split's internal `Konsole::TerminalDisplay` (same object
      already filtered for copy-on-select) watches `QKeyEvent`s and flips a per-split dirty
      bit - Enter/Ctrl+C/Ctrl+U clear it, everything else sets it. Conservative in one
      direction only: e.g. Backspacing back to an empty line still reads as dirty, so the
      failure mode is a skipped refresh, never a stray Enter into a line being typed.
    - Considered sending Ctrl+L (redraw) instead of Enter: rejected because neither bash's
      readline `clear-screen` nor zsh's ZLE `clear-screen` re-expand `PS1`/`RPS1` - both just
      redraw the already-buffered prompt text, so a branch segment (zsh `vcs_info`, etc.)
      would stay stale. Enter is what actually forces a fresh prompt expansion.

## Project rename

Planned rename neobeam → neoqute ("neobeam" is already used by multiple projects on GitHub).
Since the repo is still private and no packages are published, this is a clean find-and-replace +
file renames with no user migration or compatibility shims needed. Do it in one commit/branch —
the Lua RPC event names exist on both the C++ and Lua side and must change together.

- [ ] Verify `neoqute` is free on GitHub and AUR before starting (note: name echoes qutebrowser, unrelated project — accept or reconsider)
- [ ] Rename the GitHub repo
- [ ] Build system:
    - [ ] `CMakeLists.txt`: `project(neobeam ...)`, target name (changes installed binary to `bin/neoqute`), `NEOBEAM_*` variables and compile definitions (`NEOBEAM_HAS_TERMINAL`, `NEOBEAM_ICON_SIZES`, `NEOBEAM_TERMINAL` option — also referenced in both workflows)
    - [ ] Icon pipeline: rename `resources/icons/neobeam.svg` + placeholder SVG, generated `neobeam_<size>.png` names in `cmake/AppIcons.cmake` and `main.cpp`
- [ ] Qt application identity:
    - [ ] `main.cpp`: `setApplicationName` (determines config dir `~/.config/neoqute/`), `setDesktopFileName`
    - [ ] `config.cpp`: hardcoded `neobeam.toml` filename, generated config template text, default window title `"Neobeam - ~/path"`, `neobeam -k` hints
- [ ] Desktop file + packaging:
    - [ ] `packaging/neobeam.desktop`: filename, `Name`, `Exec`, `Icon`, `StartupWMClass` (must match `setApplicationName` for taskbar grouping)
    - [ ] `packaging/arch/PKGBUILD` + `packaging/aur/neobeam-git/`: pkgname, dir name, `url=`, regenerate `.SRCINFO`
    - [ ] `.github/workflows/release.yml`: tarball prefix and artifact names
- [ ] Lua/RPC layer (rename C++ and Lua sides together):
    - [ ] `_G.neobeam` table, augroup names, RPC event names (`neobeam_cwd`, `neobeam_file`, `neobeam_git_event`, `neobeam_image_show/hide`) — listeners in `mainwindow.cpp` match these strings exactly
    - [ ] `lua/neobeam-plugin.lua` filename (and the `neobeam_preview.lua` install name it documents)
    - [ ] QLoggingCategory names (`neobeam.minimap`, `neobeam.redraw`, `neobeam.title`) + verify skill references
- [ ] Config examples + docs: `neobeam.toml`, `neobeam.toml.example`, `README.md`, `PROJECT.md`, `AGENTS.md`
- [ ] `Neoblue.colorscheme` stays — separate name, no need to touch

## Upstream issues

### qtermwidget
- `QTermWidget::copyAvailable(bool)` is never emitted (verified in 2.4.0 sources). The only
  emitter is the `TerminalDisplay::selectionChanged()` slot, but the connection from
  `ScreenWindow::selectionChanged` to it is missing in `TerminalDisplay::setScreenWindow()`,
  so there is no usable selection-change notification in the public API.
  - Workaround in neobeam: copy-on-select (`[terminal] copy_on_select`) installs an event
    filter on the internal `Konsole::TerminalDisplay` widget and copies the selection on
    left-button release (`TerminalPanel::eventFilter`).
  - If fixed upstream, the event filter can be replaced with a plain connect to
    `QTermWidget::copyAvailable`.
- No sixel image protocol support (blocks the "Enable sixel support" item above). QtermWidget issue here: [552](https://github.com/lxqt/qtermwidget/issues/552)
- No OSC 8 hyperlink support (verified in 2.4.0: `Vt102Emulation` parses no `ESC ] 8 ; ; uri ST`
  sequence, so the URI is discarded and only the display text reaches the screen buffer). TUI
  applications that print a label with a hidden URL are therefore unclickable in neobeam, and no
  workaround is possible from the outside - the filter chain that produces link hotspots is not
  reachable through the public API either (`TerminalDisplay::filterChain()` needs a header
  qtermwidget does not install).
  - Workaround for the cases that *are* recoverable (scheme-less URLs, links an application wrapped
    across lines): select the link and press Ctrl+Shift+L (`TerminalPanel::openSelectedLink`).
