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

## Phase 8: Code Quality & Robustness

### Correctness
- [x] Add column bounds check in `handleGridLine` - row is guarded but col is not, malformed Neovim responses can write past the end of a row
- [x] Handle RPC error field - response handler extracts result (`msgList[3]`) but silently drops the error (`msgList[2]`); failed nvim API calls are invisible
- [x] Add RPC timeout and cancellation - callbacks stored by message ID are never expired; Neovim crash mid-request leaks closures
- [x] Drain partial msgpack buffer on disconnect - partial message in `m_buffer` is never flushed when Neovim exits unexpectedly
- [x] Guard `m_grids` lookups with `contains()` - `m_grids[id]` returns a default-constructed `Grid{rows=0,cols=0}` for unknown IDs instead of signalling absence

### Performance
- [ ] Cache styled fonts in `paintEvent` - bold/italic `QFont` variants are constructed fresh per cell; use a small `QHash<int, QFont>` keyed on style flags
- [ ] Cache z-order sorted grid list - grids are re-sorted on every `paintEvent`; invalidate only on `win_pos`/`win_float_pos` events
- [ ] Speed up `gridAt()` mouse hit-test - currently O(n) scan per mouse move; use the cached z-order list
- [ ] Flatten grid cell storage - replace `QVector<QVector<GridCell>>` with a flat `std::vector<GridCell>` indexed by `row * cols + col` for better cache behaviour
- [ ] Shrink `GridCell::text` - most cells hold a single codepoint; use `char32_t` or `QChar` and reserve `QString` only for multi-codepoint clusters

### Code Structure
- [ ] Break up `MainWindow` constructor (~370 lines) into `setupLayout()`, `setupConnections()`, etc.
- [ ] Split `GridWidget` (810 lines) - rendering, keyboard input, mouse selection, drag/drop, and Neovim event dispatch are too many concerns for one class
- [ ] Replace `QVariant` in Neovim message path with concrete structs - `.toInt()`/`.toList()`/`.toMap()` without type guards moves errors from compile-time to runtime
- [ ] Add RAII guard around `blockSignals(true/false)` in `uiwidgets.cpp` - current pattern is not exception-safe
- [ ] Replace C-style casts in minimap hot paths with `static_cast<int>(...)`

### Missing Features
- [ ] Statusline text should be selectable - native statusline widget does not support text selection
- [ ] IME support - `QInputMethodEvent` is not handled in `GridWidget`; CJK input drops composed characters
- [ ] Config hot-reload - changes to `neobeam.toml` require a restart; use `QFileSystemWatcher` with debounced reload
- [ ] Event-driven terminal CWD tracking - current 500 ms poll in `mainwindow.cpp` should be replaced with a Neovim autocmd
- [ ] Unit test suite - no tests for the msgpack codec, grid state machine, or key-mapping logic; these are the areas most prone to off-by-one errors

### Housekeeping
- [ ] Fix placeholder git clone URL in `README.md` (`youruser`)
- [ ] Fill in maintainer info in `packaging/aur/neobeam-git/PKGBUILD`
- [ ] Remove or consolidate vendored `third_party/toml11/` - the normal build uses FetchContent; the copy is only used by the AUR PKGBUILD override, which is confusing
- [ ] Fix typo in `AGENTS.md`: "functinons"

## Fixes/Adjustments

### General
- [x] Add switch -c to start neobeam with a custom config file path
- [x] Create example config file in the project root
### Term
- [ ] Enable sixel support for QTermWidget (blocked: upstream has no sixel support)
- [x] Add Shift+Insert and Ctrl+Shift+V bindings to paste clipboard to terminal
- [x] Add option to enable PuTTY-style right-click paste in the terminal to the config
- [x] Add options to enable text anti-aliasing and hinting for terminal font
