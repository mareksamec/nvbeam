# Contributing to nvbeam

Thanks for taking an interest. nvbeam is a Qt6/C++20 GUI that drives an embedded
`nvim --embed` over msgpack-RPC, so most changes touch one of three layers: the Qt
widgets in `src/`, the Lua helpers in `lua/` that run inside Neovim, or the
packaging under `packaging/` and `.github/workflows/`.

## Before you start

Open an issue first for anything bigger than a bug fix. The roadmap and the
reasoning behind existing design decisions live in `PROJECT.md`, and finished work
is written up in `tasks/completed/` - both are worth a look before proposing a
feature, because a surprising number of ideas have already been tried and
rejected there for a reason.

`AGENTS.md` describes the project scope and the constraints the code is written
under. It is aimed at coding agents, but it is the shortest summary of what this
project does and does not want to be.

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNVBEAM_TERMINAL=ON -DNVBEAM_SYNTAX=ON
cmake --build build -j$(nproc)
```

Dependencies per distribution are listed in `README.md`. Both optional features
(`NVBEAM_TERMINAL`, `NVBEAM_SYNTAX`) have to keep compiling when turned off - CI
builds one configuration with the terminal disabled for exactly that reason.

## Testing a change

There is no unit test suite; the app is verified by running it. Build it, run it
against a real file or directory, and say in the pull request what you exercised
and what you saw. Claims like "should work" are the one thing that will reliably
hold a review up.

For changes with a runtime effect that is hard to observe by hand, the app can be
run headless (`QT_QPA_PLATFORM=offscreen`) with the category-gated debug traces:

```sh
QT_LOGGING_RULES="nvbeam.*.debug=true" ./build/nvbeam path/to/file
```

Performance matters here more than in a typical GUI: the editor has to stay
responsive while an agent rewrites files underneath it. Anything that adds work to
a redraw, a poll or a resize should come with a measurement, not an estimate.

## Code style

- Follow the surrounding code. Qt idioms, `QString`/`QVector`, signals and slots,
  and member names prefixed `m_`.
- Comment why, not what. Every non-obvious method gets a short doc comment; the
  existing files are the reference for the expected density.
- No hardcoded values that a user might want to change - they belong in
  `Config` and in `nvbeam.toml.example`.
- Readable beats clever. This is a codebase other people have to maintain.
- Widgets emit signals; they do not call Neovim themselves. RPC goes through the
  owner (usually `MainWindow`), and anything that would be several round trips
  belongs in a Lua helper in `lua/` instead.

## Commits and pull requests

- One logical change per pull request. Renames, reformatting and drive-by cleanups
  mixed into a feature branch make the diff unreviewable; send them separately.
- Commit subject in the imperative mood ("Close buffers from the tabline"),
  followed by bullets stating what is now true.
- Rebase on `main` rather than merging it into your branch.
- Describe in the pull request what you changed, why, and how you verified it.

## Reporting bugs

Include the nvbeam version or commit, your Neovim version, Qt version, the
distribution and whether you run Wayland or X11, plus the steps that reproduce the
problem. Crashes are much easier to act on with a backtrace (`coredumpctl info
nvbeam` on systemd distributions).
