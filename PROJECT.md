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
- [-] Config
    - [x] Add simple config validation e.g. `neobeam -k`
    - [x] Handle non-existent or wrong options in the .toml config.
- [ ] **Term + Project improvements**
    - [x] Terminal select + copy to PRIMARY + CLIPBOARD 
        - [x] Add separate [terminal] config option for this? -> yes, [terminal] copy_on_select (default true)
        - [x] How to handle possible conflict with tools like wl-copy? -> no conflict; standard last-writer-wins selection semantics, no coordination needed
    - [x] Neobeam window title
        - [x] Make Neobeam uppercase
        - [x] Append project path or cwd folder path (if no project selected) e.g. Neobeam - ~/projects/neobeam
    - [-] Nvim buffer artifacts
        - [x] Sometimes when snacks.nvim file manager or nvim-tree is opened it leaves the vertical line in the buffer. Happens mainly when Neobeam window is stretched wide.
        - [x] Nvim terminal buffers (when you open one in the neobeam nvim buffer) also leaves the horizontal buffer separator artifact in the neobeam nvim buffer when closed.
        - [ ] When there is a warning or error displayed in the nvim buffer statusline it is clipped. (e.g. lua plugin error, or some vim script error etc.) (fix applied - message grid now composited above window grids; awaiting confirmation)
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


## Fixes/Adjustments

### General
- [x] Add switch -c to start neobeam with a custom config file path
- [x] Create example config file in the project root
### Term
- [ ] Enable sixel support for QTermWidget (blocked: upstream has no sixel support)
- [x] Add Shift+Insert and Ctrl+Shift+V bindings to paste clipboard to terminal
- [x] Add option to enable PuTTY-style right-click paste in the terminal to the config
- [x] Add options to enable text anti-aliasing and hinting for terminal font

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
- No sixel image protocol support (blocks the "Enable sixel support" item above).
