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
2. **Keep the GUI responsive:** the walk runs off the GUI thread
   (`QtConcurrent` or a worker `QThread`), and results are streamed to the list
   in batches. A new keystroke cancels the walk in progress. Re-walking on every
   keystroke is wasteful, so cache the file list per root directory for the
   switcher's open and score the cache on each keystroke.
3. **Limits:** a maximum depth and entry count (new constants, or config keys
   if they turn out to need tuning), and skip `.git`, plus hidden entries
   unless the fragment starts with `.`. Inside a git repository, prefer
   `git ls-files --cached --others --exclude-standard`. It is fast, and it
   respects `.gitignore`, which keeps `node_modules`, build directories and
   similar out of the results.
4. **Interaction with completion:** Tab and Enter keep their current meaning.
   Tab completes the highlighted row into the input, which now can be several
   levels deep, and Enter opens it. Show each row's relative path, not only its
   name, so two files with the same name can be told apart.

Do this after task 015. It builds on the same path parsing, and the restart
rule decides which directory is the root.

## Open questions

- Recursive results always, or only once the fragment is long enough (for
  example 2 or more characters), keeping the plain one-level listing for an
  empty fragment? The second keeps today's behaviour for browsing.
- Should folders be matched too, or only files? Opening a folder is half of
  what path mode is for, so probably both.

## Verification

- Unit test the parts that do not need a widget: the relative-path scoring and
  ordering, and the ignore rules, in `nvbeam_core` with a test.
- In the app, in a large repository (several thousand files): typing stays
  smooth, results appear within a fraction of a second, and ignored directories
  do not show up. Outside a repository, the depth and entry limits hold in the
  home directory.
- Needs a run of the app by the user.
