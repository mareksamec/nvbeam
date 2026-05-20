# Neobeam - Project Phases

## Phase 1: Skeleton + Connection ✓
- [x] CMake project scaffold with Qt6
- [x] Neovim process management (spawn `nvim --embed`, msgpack-RPC over stdin/stdout)
- [x] `nvim_ui_attach` with `ext_linegrid`
- [x] Grid renderer (QWidget painting character grid from `grid_line` events)
- [x] Build verified, nvim process connects successfully

## Phase 2: Input + Usability ← CURRENT
- [x] Keyboard input (Qt key events → Neovim input notation)
- [x] Mouse input (clicks, scroll, drag)
- [x] Cursor rendering (block/beam/underline per mode, blinking)
- [x] Resize handling (`nvim_ui_try_resize`)

## Phase 3: Visual Polish
- [ ] Font configuration (family, size, bold/italic, cell metrics)
- [ ] `ext_hlstate` semantic highlights
- [ ] Smooth scrolling
- [ ] TOML config loading

## Phase 4: Multi-grid + UI Chrome
- [ ] `ext_multigrid` (per-window grids, floating windows)
- [ ] `ext_cmdline` (native command line)
- [ ] `ext_popupmenu` (native completion popup)
- [ ] `ext_tabline` (native tab bar)
- [ ] `ext_messages` (native message area)

## Phase 5: Terminal + Advanced
- [ ] Embedded QTermWidget (splits, follows nvim cwd)
- [ ] Minimap (buffer content + highlight data)
- [ ] Image rendering (via `vim.ui.img` API when available)
