
## Side TERM - AI agent features

### Claude
[ ] **Support diff view**
- IF Claude CLI supports it (does it have some api/control interface?) Enable if claude is running in the side term (how to do the detection properly? E.g. I use alias claude and claude-personal for personal account)
- (available in Claude Code cloud and some editors like Zed)
- Diff view can be expanded next to the side term
- Add button and a keyboard shortcut for it

### Codex (ChatGPT)
- Same for Codex CLI if possible.

---

## Evaluation (2026-09-16)

**Claude: feasible.** The Claude Code IDE integration is a WebSocket/JSON-RPC
contract that any editor can host (lock file in `~/.claude/ide/`, `openDiff`
tool, `FILE_SAVED` / `DIFF_REJECTED` answers). Detection by process name or
alias is not needed: the CLI auto-connects from `CLAUDE_CODE_SSE_PORT` and
`ENABLE_IDE_INTEGRATION`, injected into the terminals neobeam spawns, and the
WebSocket connection itself is the "Claude is running" signal.

Planned in:

- `task-002-claude-ide-bridge.md` (server, lock file, environment injection)
- `task-003-claude-diff-view.md` (diff rendered by Neovim, button and shortcut)

**Codex: not feasible as written.** Since the app-server split, the interactive
`codex` TUI is itself a client of a separate agent process and exposes nothing
to a third party watching the terminal. File changes stream as
`item/fileChange/*` only to whoever started the session. Getting them into
neobeam would mean launching `codex app-server` and owning the whole
conversation UI, which is a different and much larger feature, sharing no code
with the Claude bridge. Parked until OpenAI ships an editor-side integration
contract.
