# Task 018: create a new folder from the project switcher

## Problem

The switcher opens pinned projects, recents and any existing path (path mode,
Ctrl+D), but it cannot start a new project folder. The user has to leave
nvbeam or open a terminal to `mkdir` first.

## Change

1. **Core helper:** `createFolder(path)` in `src/pathinput.cpp` creates the
   folder with its missing parents (`QDir::mkpath`, so `a/b/c` works in one
   step) and reports `Created`, `AlreadyExists`, `NotAFolder` (a file has that
   name) or `Failed` (no permission, or a file in the middle of the path).
2. **Three triggers, one action** (`SwitcherWidget::newFolder()`):
   - a folder icon button at the right edge of the input
     (`QLineEdit::addAction`, trailing position). It takes no focus, so a
     click does not dismiss the switcher. It is hidden while the line holds
     the pin side command prompt.
   - Ctrl+N.
   - a `+ Create folder <path>` row, appended last in path mode whenever the
     typed path does not exist. Being last, Enter on a fuzzy fragment still
     opens the best match; the row is highlighted only when nothing else
     matches.
3. **Behaviour:** in project mode the action switches to path mode,
   prefilled with nvim's cwd, so the name can be typed after it. In path mode
   it creates exactly the typed path, the same way Ctrl+P pins the typed
   path.
4. **After creating:** the input becomes `<new folder>/` and the footer says
   "Created … · Enter opens · Ctrl+P pins". Opening and pinning reuse the
   existing flows (an exactly typed path wins on Enter; Ctrl+P asks for the
   side command). The cached recursive-search file lists are dropped so the
   new folder shows up.
5. **Icon:** painted with QPainter in the theme's `subtext` colour at 1x, 2x
   and 3x. No icon file, no Qt6::Svg runtime dependency, no reliance on an
   icon theme (missing on BSD and Windows).

## Decisions

- Folders only. nvim's `:e` already creates files.
- No `git init` or other follow-up action; Ctrl+P and Enter cover the next
  step.
- No config key: nothing here has a value worth tuning.

## Verification

- Unit tests in `tests/tst_pathinput.cpp`: new folder, nested folder, existing
  folder, existing file, file in the middle of the path, read-only parent.
- Manual: click the icon in project mode (switches to path mode, stays open);
  type a new path, see the `+ Create folder` row, Ctrl+N creates it and lists
  it; Enter opens it in nvim; Ctrl+P on it asks for the side command; Ctrl+N
  on an existing folder says it exists; the icon reads well in a dark theme
  and in Catppuccin Latte.
