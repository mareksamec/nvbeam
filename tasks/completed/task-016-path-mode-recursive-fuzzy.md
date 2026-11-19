# Task 016: fzf-style recursive search in path mode

## Problem

Path mode (Ctrl+D in the switcher) only lists the one directory named by the
text before the last `/`, and matches the fragment after it against those
names. A file two folders down cannot be found without Tab-walking every level.
The request is fzf-like search: type a fragment and get fuzzy matches from the
whole tree below the current directory, sub-folders included.

## Change

1. **Walk the tree** below the listed directory and score each entry's path
   relative to it with `fuzzyScore()` (`src/fuzzy.h`), the same scorer project
   mode uses. Matches across segments (`srcsw` → `src/switcher.cpp`) then come
   from the scorer itself.
2. **Keep the GUI responsive:** the file list is built off the GUI thread
   (`git` as a separate process, the folder walk through `QtConcurrent`) and
   cached per root directory while the switcher is open, so each keystroke
   only scores the cached list. Until the list for a root is ready, the
   one-level listing stays, and it is replaced once the list arrives. A walk
   whose root is no longer wanted (the switcher closed) is cancelled.
3. **File list and limits:**
   - Run `git -C <root> ls-files -z --cached --others --exclude-standard`
     asynchronously (`QProcess`). Exit code 0 means the root is inside a git
     repository: the output is the file list, relative to the root, with
     `.gitignore` applied. This also works when the root is a subfolder of a
     repository. A failure (exit code 128, "not a git repository", or no git
     installed) falls back to walking the folders.
   - `ls-files` lists files only. Folders come from the files' parent paths,
     so empty and fully ignored folders do not appear.
   - The folder walk skips `.git`, does not descend into hidden folders or
     symlinked folders, and does not descend into a folder that has a `.git`
     of its own (a nested repository: the folder itself is listed, its
     contents are not). Running `ls-files` per nested repository is a possible
     follow-up.
   - Hidden entries: a path with any part starting with `.` is shown only when
     the fragment starts with `.`, for the git list and the walk alike.
   - Config keys in `[switcher]`: `path_max_depth` (default 6) limits the
     folder walk, `path_max_entries` (default 50000) caps the paths collected
     by either source. The git list is not depth limited.
4. **Interaction with completion:** Tab and Enter keep their current meaning.
   Tab completes the highlighted row into the input, which now can be several
   levels deep, and Enter opens it. Show each row's relative path, not only its
   name, so two files with the same name can be told apart.

Do this after task 015. It builds on the same path parsing, and the restart
rule decides which directory is the root.

## Decisions

- Recursive results start once the fragment has 2 or more characters. An
  empty or one-character fragment keeps the plain one-level listing, so
  browsing works as before.
- Folders and files are both matched. Opening a folder is half of what path
  mode is for.

## Verification

- Unit test the parts that do not need a widget: the relative-path scoring and
  ordering, and the ignore rules, in `nvbeam_core` with a test.
- In the app, in a large repository (several thousand files): typing stays
  smooth, results appear within a fraction of a second, and ignored directories
  do not show up. Outside a repository, the depth and entry limits hold in the
  home directory.
- Needs a run of the app by the user.
