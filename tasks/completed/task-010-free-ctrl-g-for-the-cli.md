# Task 010: leave Ctrl+G / Ctrl+Shift+G to the side terminal

## Problem

The Claude CLI uses Ctrl+G to open the current query in an external editor.
Neobeam binds `Ctrl+Shift+G` to the change review panel
(`Shortcuts::toggleChangeReview()`), and window-level QShortcuts win over the
focused terminal widget, so the key never reaches the CLI running in the side
terminal.

Plain `Ctrl+G` is not bound by neobeam today, but it has to stay that way, and
it has to be confirmed that the embedded terminal really passes it through
(QTermWidget sends it as BEL, 0x07, which is what the CLI reads).

## Change

- Move the change review toggle off G. Proposed binding: `Ctrl+Shift+R`, which
  nothing in `Shortcuts` uses. The review button's tooltip follows
  automatically, since it is built from the same `Shortcuts` entry.
- Keep `Ctrl+G` unbound at window level.
- Check the remaining bindings for other keys the CLI owns, and note any that
  are also worth freeing.

## Audit of the remaining bindings

Checked against the Claude Code interactive-mode reference
(https://code.claude.com/docs/en/interactive-mode), which claims `Ctrl+C`,
`Ctrl+D`, `Ctrl+E`, `Ctrl+B`, `Ctrl+G`, `Ctrl+K`, `Ctrl+L`, `Ctrl+N`, `Ctrl+O`,
`Ctrl+P`, `Ctrl+R`, `Ctrl+S`, `Ctrl+T`, `Ctrl+U`, `Ctrl+V`, `Ctrl+W`, `Ctrl+X`,
`Ctrl+Y`, `Ctrl+Z`, `Ctrl+_` / `Ctrl+Shift+-`, `Ctrl+Enter`, `Shift+Tab` and
the Alt keys.

Nothing else has to be freed:

- Neobeam binds no plain `Ctrl+<letter>`. Its window bindings are
  `Ctrl+\``, `Ctrl+Tab` / `Ctrl+Shift+Tab`, `Ctrl+-` / `Ctrl+=`, `Ctrl+wheel`
  and otherwise `Ctrl+Shift+<key>`, none of which the CLI claims.
- `Ctrl+-` is matched on `modifiers() == Qt::ControlModifier`, so the CLI's
  `Ctrl+Shift+-` (undo input edit) carries the Shift flag and passes straight
  through to the terminal.
- `Ctrl+Shift+V` (neobeam paste) and the CLI's `Ctrl+V` (paste image) are
  distinct key events to Qt, so both keep working.

## Verification

With the side terminal focused and the CLI running: Ctrl+G opens the query in
the editor, and Ctrl+Shift+R toggles the review panel from anywhere in the
window.
