# Task 013: change review panel freezes while an expanded file is being edited

## Problem

With a file's row expanded in the change review panel, nvbeam freezes when that
file is edited quickly in the background, typically by a coding agent in the
side terminal. This is listed under Known bugs in `README.md`.

The cause has not been confirmed yet. Do this task before task 014, which
changes the same code.

## Suspects

All of the following run on the GUI thread, once per poll (`[review]
refresh_ms`, default 2000 ms), whenever the expanded file's diff text changed:

1. **Full body rebuild** in `FileCard::applyBody()` (`src/changereview.cpp`):
   `setPlainText()` replaces the whole document, so the whole diff is laid out
   again, even when the agent appended a single line. The cost grows with the
   file's diff size, up to `[review] max_lines` (20000).
2. **Syntax highlighting of the whole document**: `DiffHighlighter` is a
   `QSyntaxHighlighter`, which re-highlights every block after
   `setPlainText()`. With KF6SyntaxHighlighting this is the most expensive pass.
3. **`paintChangedLines()`** calls `setBlockFormat()` on every block. Each call
   emits `contentsChange`, which can re-trigger the highlighter and the layout
   block by block.
4. **Untracked files are read in full on every poll** to count their lines
   (`collectDiff()`, `kMaxUntrackedBytes` cap). An agent that creates a large
   new file pays this cost every 2 s, expanded or not.
5. **`parseDiff()`** splits the complete `git diff` output on the GUI thread.

The poll keeps firing while a rebuild is still running, so a rebuild that
takes longer than `refresh_ms` never lets the event loop catch up. That fits
the reported symptom of a freeze rather than a stutter.

## Change

1. **Measure first.** Add a `nvbeam.review` logging category (`src/logging.cpp`)
   and time each step above with `QElapsedTimer`. Reproduce with a script that
   appends lines to a tracked file and to a new untracked file every 100 ms.
   Use files of a few thousand lines, with `syntax` on and off.
2. **Fix what the numbers point at.** Likely candidates:
   - Update the body incrementally: compare the old and new line lists and
     edit only the changed range through a `QTextCursor`, instead of calling
     `setPlainText()`. Scroll position and selection then survive for free.
   - Paint the changed-line stripes for the edited range only, inside the same
     edit block.
   - Count an untracked file's lines only when its size or mtime changed since
     the last poll.
   - Skip a poll while the previous refresh's rendering is still pending,
     rather than queueing another one.
3. Remove the bug from Known bugs in `README.md` once it is fixed.

## Verification

- The reproduction script runs for a minute with the row expanded. The window
  stays responsive (typing in nvim, scrolling the panel), and the
  `nvbeam.review` timings show each refresh finishing well inside
  `refresh_ms`.
- A selection inside the expanded diff survives refreshes while the file grows.
- Build with and without KF6SyntaxHighlighting.
- Needs a run of the app by the user; the panel has no unit tests.
