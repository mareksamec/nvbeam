# Task 015: path mode keeps a stale `~/` in front of what the user types

## Problem

Ctrl+Shift+P followed by Ctrl+D opens the switcher's path mode prefilled with
nvim's cwd and a trailing separator, for example `~/`. The prefill is not
selected, so whatever the user types is appended to it. Typing `~/Projects` or
`/etc` gives `~/~/Projects` or `~//etc`. `expandHome()` only expands a leading
`~`, so these paths resolve to nothing and the list goes empty.

## Change

1. **Select the prefill** in `SwitcherWidget::setMode()` (`src/switcher.cpp`)
   when entering path mode. Typing then replaces it, while Tab and the arrow
   keys still work from it, because its contents are listed as before.
2. **Restart the path at `~/` or `/`** in the middle of the text, the way the
   Emacs minibuffer does. When the text contains `/~/` or `//`, everything
   before the last such restart is ignored. Then `~/~/Projects` reads as
   `~/Projects`, and `~/code//etc` reads as `/etc`. This covers users who
   click to deselect, or type after moving the cursor.
3. **Make it testable:** `collapseHome()`, `expandHome()` and `resolvePath()`
   are file-local in `switcher.cpp`, which is not in `nvbeam_core`. Move them,
   together with the new restart rule, into a small core unit (e.g.
   `src/pathinput.{h,cpp}`), and add `tests/tst_pathinput.cpp`.

## Verification

- `ctest` passes, with new cases for `~/~/x`, `~//etc`, `/a//b`, a relative
  path, an empty text, and a plain `~`.
- In the app: Ctrl+D, then type `~/Proj` straight away. The input reads
  `~/Proj` and lists matches from the home directory. Tab and Enter still walk
  down the tree from the prefilled cwd.
- Needs a run of the app by the user.
