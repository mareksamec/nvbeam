# Task 009: several side terminal commands per project

## Problem

A project often needs more than one CLI agent running at once, but a project
entry carries a single `side_command`, so the side terminal starts exactly one
tool. Splitting the panel with the "+" button is no help either: a new split
runs the configured command again, which starts a second copy of the same agent
and can leave two tools fighting over the same worktree.

## Change

### Config

`[[projects]]` (and the switcher's `projects.toml`) accept `side_command`,
`side_command_2` and `side_command_3`. They are read in that order and the
non-empty ones become the list of commands the side terminal starts with, one
split per command. A project with a single `side_command` behaves exactly as
before.

The pin dialog still asks for one command; the extra two are written by hand.

### Side terminal layout

The side panel's splits are laid out horizontally, so the agents sit side by
side and are all readable at once. The panel's minimum width and the rest of
the side-panel chrome are unchanged.

### Splits the user opens

"+" (and its shortcut) opens a plain shell. Each split remembers the command it
was started with, so:

- the startup splits keep running their own command,
- a user-opened split runs nothing,
- the pin dialog's prefilled suggestion reports what the focused split runs.

`adoptProject()` takes the command list: switching to another project replaces
the splits with one per command of the new project, as it already does for the
single-command case.

## Non-goals

- No per-split command editing in the UI.
- No session restore of side terminal splits (the side panel still starts from
  the project's commands on every launch).
