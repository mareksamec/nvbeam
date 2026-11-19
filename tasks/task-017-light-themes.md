# Task 017: light themes (Catppuccin Latte and vim-paper)

## Problem

nvbeam's UI chrome is dark only. The colours are hardcoded per widget, mostly
the Tokyo Night palette (`#1a1b26`, `#24283b`, `#c0caf5`, ...), with OneDark
and Catppuccin accents mixed in:

- `src/uiwidgets.cpp`: statusline, bufferline, message log, mode colours
  (about 34 literals)
- `src/switcher.cpp`, `src/branchswitcher.cpp`: popup style sheets
- `src/mainwindow.cpp`: panel toggle buttons
- `src/changereview.h`: `ReviewConfig` defaults (the panel itself already
  follows the terminal colour scheme)

On a light nvim colourscheme the chrome stays dark around a light buffer. The
request is two light themes: Catppuccin Latte and vim-paper.

## Change

1. **Palette in one place.** Add a `Theme` struct with named roles: background,
   surface, foreground, dim, accent, border, selection, error, warning, mode
   colours, and the git added/untracked colours. Put it in `nvbeam_core`, with
   the built-in themes as data (e.g. `src/theme.{h,cpp}`). Replace the
   literals in the files above with lookups. This step changes no colours: the
   current dark palette becomes the `dark` theme, so the step can be reviewed
   on its own.
2. **Config:** `[ui] theme = "dark"` (default), `"catppuccin-latte"` or
   `"vim-paper"`, validated by `nvbeam -k` with a test in `tests/tst_config.cpp`.
   Existing explicit colour keys (`accent_color`, `focus_border_color`,
   `[mouse] selection_color`) still override the theme.
3. **Terminal schemes:** add `CatppuccinLatte.colorscheme` and
   `VimPaper.colorscheme` to `resources/color-schemes/`. A theme picks its
   matching scheme when `[terminal] color_scheme` is not set explicitly. The
   review panel follows the terminal scheme, and its KF6 syntax theme already
   switches to light on a light background (`DiffHighlighter`).
4. **Palettes from upstream:** take the exact colours from the projects
   themselves at implementation time, not from memory: the Catppuccin palette
   (github.com/catppuccin/catppuccin, Latte flavour) and vim-paper
   (github.com/yorickpeterse/vim-paper). Note the source and version in a
   comment next to each theme.
5. The nvim buffer keeps the user's own nvim colourscheme. Document in
   `README.md` that the matching nvim colourscheme has to be set in nvim, and
   list the new `[ui] theme` key.

## Open question

Should `theme = "auto"` follow nvim instead? nvim reports its default
background through `default_colors_set` (already handled in
`src/gridwidget.cpp`), so nvbeam could switch between `dark` and a chosen light
theme when the nvim colourscheme changes. This is useful, but it needs a rule
for which light theme to pick, so it is left out of the first version.

## Verification

- After step 1, alone: no visible change against the current build, compared
  side by side on screenshots of the statusline, bufferline, switcher, branch
  switcher and message log.
- `ctest` passes with the new theme and config cases.
- With each light theme: every popup and panel is readable (text contrast on
  the light background, selection, focus border, error and warning lines,
  powerline dividers), and the terminals use the matching scheme.
- Needs a run of the app by the user.
