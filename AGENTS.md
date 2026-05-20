# Project scope
- Neovim GUI client
- Build on Qt / modern C++
- Build app with multi-platform support in mind.
- Embedded native terminal ([QTermWidget](https://github.com/lxqt/qtermwidget))
    - VScode like terminal window
    - Terminals supports splits
    - Terminal switchces to current vim working dir by default
    - Support for Sixel image protocols (embedded terminal)
- Config in toml
    - Defaults config in QStandardPaths locations are commented out
    - Select image rendering preferences
    - Configure fonts and terminal behaviour
    - Support smooth fond rendering, hinting configuration and anti-aliasing.
    - Configure terminal follow nvim working dir true/false
- Image rendering in nvim buffers when official vim.ui.img API becomes available.
- Native vs-code like minimap with syntax highlighting, LSP references support etc.
    - Consume highlight data from Neovim (syntax groups, diagnostics, extmarks)
    - Use QT library as a fallback.
- Wayland support via native qt6-wayland

# Neovim related
## Versions and compatibility
- Min Neovim version 0.10+

## Neovim UI extensions list
- ext_linegrid
- ext_multigrid (for proper window management)
- ext_hlstate (for semantic highlight info)
- ext_cmdline, ext_popupmenu, ext_tabline, ext_messages (for native UI chrome)

## Input handling
- Use inspiration how other similar projects handle the following:
    - IME support
    - Dead keys
    - compose keys


# Coding practices
- Write clean and efficient code
- Follow Qt and modern C++ best practices
- Comment the code - main functinons and methods
- Do not hard code values, expose them in parameters and variables
- Prefer readable code instead of super l33t tricks and hacks that are hard to read and maintain for less experienced programmers.
- CMake build system

# Performance targets
- Low latency
- Fast and smooth animations

# Feature roadmap
- BSD build compatibility (OpenBSD, FreeBSD, NetBSD) - Unix first project
- Custom image previewer config
- X server support
- Ueberzug preview support
- Windows support
- Fast large file handling
- Ligatures
- Kitty image protocol
