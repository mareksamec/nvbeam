## What changed

<!-- One paragraph: what this does and why. Link the issue if there is one. -->

## How it was verified

<!--
Say what you actually ran and saw. For the UI, the grid, the terminal and the
Neovim connection there are no unit tests, so a build plus a description of what
you exercised in the running app is the evidence a review relies on. For
anything performance-sensitive, give numbers.
-->

## Checklist

- [ ] One logical change; no unrelated renames or reformatting
- [ ] Builds with the optional features both on and off (`NVBEAM_TERMINAL`, `NVBEAM_SYNTAX`)
- [ ] New user-visible settings are in `Config` and `nvbeam.toml.example`, not hardcoded
- [ ] `ctest` passes, and a change to `nvbeam_core` comes with a test
- [ ] Ran the app and confirmed the change (say what you saw above)
