# Neobeam

A fast, native Neovim GUI client built with Qt6 and C++.

Neobeam connects to an embedded Neovim instance via msgpack-RPC and renders the editor using native Qt widgets. It provides a VSCode-like experience with a native terminal panel, minimap, tabline, command line, and completion popup — all driven by Neovim's UI protocol extensions.

## Features

- **Full Neovim integration** — ext_linegrid, ext_multigrid, ext_hlstate, ext_cmdline, ext_popupmenu, ext_tabline, ext_messages
- **Native statusline** — mode indicator, file, git branch, diagnostics, position
- **Embedded terminal** — QTermWidget-based, with splits and nvim cwd following (Ctrl+\` to toggle)
- **Minimap** — zoomed-out buffer view with syntax highlighting, viewport indicator, click-to-scroll
- **Configurable fonts** — family, size, hinting, antialiasing
- **TOML configuration** — sensible defaults, all options commented out in the generated config
- **Wayland native** — via qt6-wayland
- **Image rendering** — *(Planned)* placeholder for upcoming `vim.ui.img` API

## Requirements

- Neovim 0.10+
- Qt 6.x (Core, Gui, Widgets)
- CMake 3.20+
- C++20 compiler
- qtermwidget6 (optional, for embedded terminal)

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
```

### Dependencies (Fedora)

```sh
sudo dnf install cmake gcc-c++ qt6-qtbase-devel mesa-libGL-devel
# Optional: for embedded terminal
sudo dnf install qtermwidget-qt6-devel
```

### Dependencies (Arch)

```sh
sudo pacman -S cmake qt6-base
# Optional: for embedded terminal
sudo pacman -S qtermwidget
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
| `-h`, `--help` | Show help |
| `-v`, `--version` | Show version |

Any unrecognized arguments are passed directly to the embedded Neovim process.

### Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+\` | Toggle terminal panel |
| Shift+Insert | Paste clipboard into terminal |
| Ctrl+Shift+V | Paste clipboard into terminal |

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
# color_scheme = "Linux"  # QTermWidget color scheme
# shell = ""  # shell program (empty = system default)
# shell_args = []  # arguments passed to shell
# exec_command = ""  # command to run after shell starts (e.g. "tmux")
# right_click_paste = false  # PuTTY-style right-click paste
# antialiasing = true  # font antialiasing in terminal
# hinting = "default"  # font hinting: none, slight, medium, full, default

# [ui]
# ext_popupmenu = true
# ext_tabline = true
# ext_messages = true
# bufferline = true  # show open buffers instead of tabpages
# statusline = true  # native statusline (mode, file, position, diagnostics)
# accent_color = "#50fa7b"
# statusline_divider = "powerline"  # powerline, pipe, none

# [mouse]
# terminal_selection = true   # terminal-style select/copy when vim mouse= is empty
# auto_copy = true            # auto-copy selection to clipboard on release
# selection_color = "#44475a" # selection highlight color
```

A ready-to-copy `neobeam.toml.example` is also included in the project root.

### Configuration options

**`[font]`**
- `family` — Font family name (default: `"Monospace"`)
- `size` — Font size in points (default: `10`)
- `hinting` — Hinting preference: `none`, `slight`, `medium`, `full`, `default` (default: `default`)
- `antialiasing` — Enable font antialiasing (default: `true`)

**`[terminal]`**
- `follow_cwd` — Terminal automatically changes to nvim's working directory (default: `true`)
- `start_expanded` — Open with the terminal panel visible (default: `false`)
- `height` — Terminal panel startup height in pixels (default: `300`)
- `font` — Terminal font family; empty uses the global font (default: `""`)
- `font_size` — Terminal font size in points; `0` uses the global font size (default: `0`)
- `color_scheme` — QTermWidget color scheme name (default: `"Linux"`)
- `shell` — Shell program to launch; empty uses the system default (default: `""`)
- `shell_args` — Arguments passed to the shell (default: `[]`)
- `exec_command` — Command run after the shell starts, e.g. `"tmux"` (default: `""`)
- `right_click_paste` — PuTTY-style right-click paste in the terminal (default: `false`)
- `antialiasing` — Enable font antialiasing in the terminal (default: `true`)
- `hinting` — Terminal font hinting: `none`, `slight`, `medium`, `full`, `default` (default: `default`)

**`[ui]`**
- `ext_popupmenu` — Native completion popup (default: `true`)
- `ext_tabline` — Native tab bar (default: `true`)
- `ext_messages` — Native message area (default: `true`)
- `bufferline` — Show open buffers in tabline instead of tabpages (default: `true`)
- `statusline` — Native statusline widget (default: `true`)
- `accent_color` — Accent color for UI elements (default: `"#50fa7b"`)
- `statusline_divider` — Statusline segment divider style: `powerline`, `pipe`, `none` (default: `powerline`)

**`[mouse]`**
- `terminal_selection` — Terminal-style select/copy in nvim buffers when vim `mouse=` is empty (default: `true`)
- `auto_copy` — Auto-copy the selection to the clipboard on release (default: `true`)
- `selection_color` — Selection highlight color (default: `"#44475a"`)

## License

MIT
