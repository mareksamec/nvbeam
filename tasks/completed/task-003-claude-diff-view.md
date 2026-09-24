# Task 003: Claude diff view in the Neovim grid

Part 2 of 2 for `001-brief-side-term-ai.md` / Claude diff view. Depends on task
002 (the bridge that delivers `openDiff`).

## Goal

When Claude proposes a file change, neobeam shows it as a real side-by-side
diff in the Neovim grid, next to the side terminal Claude is running in, and
the user accepts it with `:w` or rejects it by closing the tab. The answer
travels back to the CLI as `FILE_SAVED` or `DIFF_REJECTED`.

## Why Neovim renders the diff

Neovim already owns diffing: `diffthis`, `]c` / `[c` navigation, `do` / `dp`,
folds, syntax highlighting, and the user's own diff configuration. A Qt diff
widget would duplicate all of it, would not match the editor's colours, and
would have no relationship to the buffer the user is editing. The bridge
therefore converts the `openDiff` call into a Neovim command, and the entire
visual side of this feature is one Lua file.

"Expanded next to the side term" is satisfied by construction: the diff opens
in the editor area, which sits next to the docked side terminal in
`m_hsplitter`.

## New file: `lua/agentdiff.lua`

Follows the existing convention in `lua/`: a chunk executed once per nvim
instance via `nvim_exec_lua`, defining functions on the global `neobeam` table,
taking the RPC channel id as its argument so it can push results back with
`vim.rpcnotify` (same pattern as `lua/gitstatus.lua` and `lua/events.lua`).

```lua
-- neobeam.agent_diff_open(old_path, new_path, contents, tab_name)
```

Behaviour:

1. `vim.cmd('tabnew')`, edit `old_path` in the left window, `vim.cmd('diffthis')`.
   When `old_path` does not exist (a file Claude is creating), use an empty
   scratch buffer on the left so the diff reads as "everything is new".
2. Split right, create the proposal buffer: `contents` split on newlines into a
   buffer with `buftype=acwrite`, `bufhidden=wipe`, `swapfile=false`, named
   `tab_name` so `:ls` and the tabline show what it is. Copy the left buffer's
   `filetype` so highlighting matches. `vim.cmd('diffthis')`.
3. Record the tab handle and `tab_name` in a module-local table keyed by
   `tab_name`, so `close_tab` and cleanup can find it.
4. `BufWriteCmd` on the proposal buffer: write its lines over `old_path` with
   `vim.fn.writefile`, notify `{tab_name = ..., result = 'saved'}`, mark the
   entry resolved, close the tab, then `:checktime` so any window already
   showing that file reloads.
5. `BufWipeout` (or `TabClosed`) on an entry that is not resolved: notify
   `{tab_name = ..., result = 'rejected'}`.
6. `neobeam.agent_diff_close(tab_name)` and `neobeam.agent_diff_close_all()`
   for the `close_tab` and `closeAllDiffTabs` tools. Closing this way resolves
   the entry first, so it does not also fire a spurious rejection.

Accepting writes the file from nvim, which is what the reference
implementations do: `FILE_SAVED` tells the CLI the content is already on disk.

The autocmds are buffer-local and the augroup is cleared on re-registration, so
a `keep_running` nvim restart re-registers cleanly like the other helpers.

## Changes to existing files

### `src/mainwindow.{h,cpp}`

Register the new helper in the existing bootstrap loop (`mainwindow.cpp` around
line 564) and pass the channel id alongside `gitstatus.lua` and `events.lua`.

New private method, kept small because it is pure wiring:

```cpp
/// Route a Claude openDiff request into nvim's own diff (lua/agentdiff.lua)
/// and remember which request the answer belongs to.
void openAgentDiff(const QString &requestId, const QString &oldPath,
                   const QString &newPath, const QString &newContents,
                   const QString &tabName);
```

- Connect `ClaudeIdeServer::openDiffRequested` to it. It calls
  `nvim_exec_lua` with `return neobeam.agent_diff_open(...)` and stores
  `tabName -> requestId` in a `QHash<QString, QString> m_pendingDiffs`.
- Handle the `neobeam_diff_result` notification in the existing
  `NvimProcess::notification` switch: look the tab name up, call
  `m_claudeIde->resolveDiff(id, result == "saved")`, erase the entry.
- Connect `closeTabRequested` / `closeAllDiffTabsRequested` to the matching
  Lua calls.
- On `connectedChanged(false)`, close any open diff tabs and drop the pending
  map: the client that asked for them is gone.
- Raise and focus the grid when a diff opens, and leave the maximized-pane
  state alone (`m_maximized`); if the editor is hidden because the side
  terminal is maximized, call `leaveMaximized()` first, otherwise the diff
  would open invisibly.

Keep this in `MainWindow` rather than in a new controller: it is roughly forty
lines of signal wiring plus one hash, and it matches how `GitStatus` and
`BranchSwitcherWidget` are wired today.

### Button and shortcut

- Side terminal toolbar (`TerminalPanel` builds it around line 218): one more
  action, shown only when the panel is in side mode, that jumps to the pending
  diff tab. Disabled while no diff is open, so it doubles as the "Claude is
  connected and waiting on you" indicator the brief asks for.
- `Ctrl+Shift+D` for the same thing, registered next to the other shortcuts in
  the `MainWindow` constructor. Verified free: the existing set is
  ``Ctrl+` ``, `Ctrl+Shift+` `Down/E/L/M/P/Period/V/W/X` and `Shift+Insert`.
- With no diff open, both are no-ops rather than errors.

### README

Document the feature in the keybinding table and in prose next to the side
terminal section: what `[claude] enabled` does, that `claude` auto-connects
from any neobeam terminal regardless of alias, that `:w` accepts and closing
the tab rejects, and that `Ctrl+Shift+D` jumps to the pending diff.

### PROJECT.md

Add the item to the current improvement segment with the same level of detail
as the neighbouring entries.

## Acceptance

1. Ask Claude in the side terminal to edit a file in the project. A diff tab
   opens in the editor area showing the current file against the proposal, with
   correct syntax highlighting.
2. `:w` in the proposal buffer writes the change to the real file, closes the
   tab and Claude proceeds as if the edit was accepted.
3. Closing the tab without writing makes Claude report the edit as rejected.
4. A multi-file edit sequence opens one diff after another, each answered
   independently.
5. Killing `claude` while a diff is open closes the tab and leaves no stale
   entry in the pending map (open another diff afterwards and confirm it still
   works).
6. `Ctrl+Shift+D` and the toolbar button jump to the open diff, and do nothing
   when there is none.
7. The whole feature is absent, with no build errors and no behaviour change,
   when built with `-DNEOBEAM_CLAUDE_IDE=OFF`.

Per `AGENTS.md`, the user runs the binary and reports back.

## Risks

- **Write semantics.** Accepting writes the file from nvim. If a future CLI
  version expects the editor not to write, the change would land twice. Phase 0
  of task 002 should capture what the CLI does after `FILE_SAVED`; adjust here
  if it re-applies the patch itself.
- **Unsaved changes in the target buffer.** If the user has unwritten edits to
  `old_path`, accepting Claude's version overwrites the file underneath them.
  Guard: refuse to open the diff and answer `DIFF_REJECTED` with the reason
  when the target buffer is modified, rather than silently losing work.
- **Large files.** `new_file_contents` arrives whole over the socket. Fine for
  source files; a multi-megabyte file would pause the UI. Cap the payload and
  reject beyond it.

## Non-goals

- A Qt-rendered diff widget.
- Hunk-level accept/reject. Neovim's `do` / `dp` already cover it inside the
  diff, and the protocol only answers accept or reject for the file as a whole.
- Codex.
