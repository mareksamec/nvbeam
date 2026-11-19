# Task 014: show a few context lines around changes in the review panel

## Problem

An expanded file in the review panel shows only added and removed lines. The
unchanged context is all or nothing: hidden by default, and the row's ⋯ button
(or `[review] show_context`) brings back the full hunks git produced. A single
changed line is hard to read with no neighbours, and the full hunks are often
more than needed.

## Change

1. **New config key** `[review] context_lines` (integer, default `2`, range
   0-3). Its value is the number of unchanged lines kept before and after each
   run of added or removed lines. `0` keeps today's behaviour.
   - Parse and validate it in `src/config.cpp` next to `show_context`, with a
     test in `tests/tst_config.cpp`. It belongs to `nvbeam_core`, so a test is
     required.
   - Add it to the generated config template, to `nvbeam.toml.example`, and to
     the `[review]` block in `README.md`.
2. **Filtering** in `FileCard::bodyText()` (`src/changereview.cpp`): a context
   line is kept when it is within `context_lines` of an added or removed line in
   the same hunk. This needs a second pass over each hunk: the first pass finds
   the change lines, the second keeps the context lines near them. Line numbers
   are already counted for hidden lines, so they stay correct.
3. **⋯ and `show_context`** keep their meaning: they show the full hunks. With
   `context_lines` in place, the ⋯ tooltip should say "Show all context lines".
4. **Upper bound:** `git diff` produces 3 context lines by default, so filtering
   can only go down to fewer. That is why the range stops at 3. If more is ever
   wanted, `collectDiff()` would pass `-U<n>`, which changes the hunks and the
   cost of every refresh, so it is left out here.

Do this after task 013. That task may change how the body is rebuilt, and the
filter must work with an incremental update too.

## Open question

Inside a hunk, two changes can be further apart than `2 × context_lines`. The
lines between them are then hidden with no marker. Should a dim separator line
mark the gap? It would make a copied selection no longer a valid diff, which
the panel currently preserves.

## Verification

- `ctest` passes, including the new `context_lines` cases (default, 0, out of
  range, wrong type).
- A file with changes in several places shows each change with 2 lines above
  and below. The gutter numbers match the file, and ⋯ still expands to the full
  hunks.
- Needs a run of the app by the user.
