# Task 002: Claude Code IDE bridge (transport layer)

Part 1 of 2 for `001-brief-side-term-ai.md` / Claude diff view. This task ends
when the Claude CLI running in a neobeam terminal reports that it is connected
to neobeam as its IDE. Rendering the diff is task 003.

## Goal

neobeam hosts the same WebSocket/JSON-RPC contract that the VS Code and
JetBrains Claude Code extensions host, so `claude` running inside a neobeam
terminal auto-connects and can call editor-side tools.

Reference implementations of the same contract:

- https://github.com/coder/claudecode.nvim (`PROTOCOL.md`)
- https://github.com/transept-ai/obsidian-claude-code-ide-pro
- https://github.com/manzaltu/claude-code-ide.el

The protocol is not published by Anthropic. Treat every shape below as
*expected* and confirm it against real traffic in phase 0 before building on
it.

## Why this solves the detection problem in the brief

The brief asks how to detect that `claude` is running in the side terminal
given aliases such as `claude` and `claude-personal`. No detection is needed.
Claude Code auto-connects when it finds `CLAUDE_CODE_SSE_PORT` and
`ENABLE_IDE_INTEGRATION` in its environment, whatever the binary is called and
however it is aliased or wrapped. Injecting those two variables into the
terminals neobeam spawns is the whole mechanism, and "a client is connected on
our WebSocket" is the authoritative running/not-running signal used to enable
the UI in task 003.

## Protocol summary (to be confirmed in phase 0)

Lock file at `~/.claude/ide/<port>.lock`:

```json
{
  "pid": 12345,
  "workspaceFolders": ["/home/marek/Projects/neobeam"],
  "ideName": "neobeam",
  "transport": "ws",
  "authToken": "a3f1c2d4e5f60718293a4b5c6d7e8f90"
}
```

`authToken` is 32 lowercase hex characters (128 bits) from a CSPRNG. The client
sends it back as the `x-claude-code-ide-authorization` header on the WebSocket
handshake; a mismatch must be rejected.

Messages are JSON-RPC 2.0 over the WebSocket. Editor-side tools the CLI calls:
`openDiff` (blocking), `close_tab`, `closeAllDiffTabs`, `getCurrentSelection`,
`getOpenEditors`, `getWorkspaceFolders`, `getDiagnostics`, `openFile`,
`saveDocument`, `checkDocumentDirty`.

Open question for phase 0: whether the CLI calls those names as JSON-RPC
methods directly, or wraps them in an MCP `tools/call` after an `initialize`
and `tools/list` exchange. Different reference implementations describe it both
ways. The answer decides the dispatch layer, so it must be observed, not
assumed.

## Phase 0: protocol spike (do this first)

Smallest possible listener that logs, and nothing else.

1. Stand up `QWebSocketServer` on `QHostAddress::LocalHost`, port 0.
2. Write the lock file with the real port, a random token and the current
   working directory.
3. Log every text frame received, verbatim, and the handshake headers seen on
   the accepted socket.
4. Run `claude` in a terminal with the two environment variables set, then run
   `/ide` in it.
5. Record in this file, under "Observed protocol", the exact frames: the
   handshake sequence, whether `initialize`/`tools/list` appear, and the exact
   `openDiff` payload produced by asking Claude to edit a file.

Only after that, build the real dispatch.

## Build system

Mirror the existing optional-terminal pattern in `CMakeLists.txt` so a machine
without `qt6-websockets` still builds:

```cmake
option(NEOBEAM_CLAUDE_IDE "Build the Claude Code IDE bridge (requires Qt6 WebSockets)" ON)

if(NEOBEAM_CLAUDE_IDE)
    find_package(Qt6 QUIET COMPONENTS WebSockets)
    if(NOT Qt6WebSockets_FOUND)
        message(STATUS "Qt6 WebSockets not found, Claude IDE bridge disabled")
        set(NEOBEAM_CLAUDE_IDE OFF)
    endif()
endif()
```

When on: add `src/claudeide.cpp` to the target, link `Qt6::WebSockets`, define
`NEOBEAM_HAS_CLAUDE_IDE`. Every call site in `mainwindow.cpp` is guarded by
that define, exactly as the terminal panel is today.

Qt6 WebSockets is already installed on the dev machine
(`/usr/lib/cmake/Qt6WebSockets`). Packaging files under `packaging/` need the
new dependency added.

## New files

### `src/claudeide.h` / `src/claudeide.cpp`

`class ClaudeIdeServer : public QObject`. Transport and protocol only. It knows
nothing about Neovim, diffs or widgets; it emits requests as signals and waits
for the owner to answer.

Public API:

```cpp
/// Start listening on a free localhost port and publish the lock file.
/// Returns false when the port cannot be bound (the bridge then stays inert).
bool start(const QString &workspaceDir);

/// Rewrite the lock file's workspaceFolders after a project switch.
void setWorkspaceDir(const QString &dir);

/// Environment entries to inject into terminals ("NAME=value" form), empty
/// while not listening.
QStringList terminalEnvironment() const;

/// Whether a Claude client is currently connected. This is the running
/// /not-running signal the UI uses; there is no process sniffing.
bool isConnected() const;

/// Answer a pending openDiff: accepted maps to FILE_SAVED, otherwise
/// DIFF_REJECTED. Unknown ids are ignored (client already gone).
void resolveDiff(const QString &requestId, bool accepted);
```

Signals:

```cpp
void connectedChanged(bool connected);
void openDiffRequested(const QString &requestId, const QString &oldPath,
                       const QString &newPath, const QString &newContents,
                       const QString &tabName);
void closeTabRequested(const QString &tabName);
void closeAllDiffTabsRequested();
```

Internals:

- `QWebSocketServer m_server` bound to `QHostAddress::LocalHost` with port 0,
  so the OS picks the port and nothing is reachable off-box.
- On `newConnection()`, read `socket->request().rawHeader("x-claude-code-ide-authorization")`
  and compare against the token. Mismatch: log and `close()`.
- One connected client at a time is the normal case, but several terminals may
  each run a `claude`. Keep a `QList<QWebSocket *>` and answer whichever socket
  a pending request came in on; `isConnected()` is "the list is not empty".
- Pending blocking requests: `QHash<QString, QPointer<QWebSocket>> m_pending`
  keyed by the JSON-RPC id. Cleared when the socket disconnects, which must
  also tell the owner to tear down any diff opened for it (task 003).
- JSON handling via `QJsonDocument`; no third-party dependency.
- Tools that can be answered without the editor being involved are answered
  inside this class from data the owner pushes in
  (`getWorkspaceFolders`, and `getDiagnostics` returning an empty list until
  someone needs more). `openDiff`, `close_tab` and `closeAllDiffTabs` are the
  only ones routed out as signals in this task.
- Unknown methods get a JSON-RPC "method not found" error rather than silence,
  so the CLI does not hang waiting.

Lock file handling:

- Directory `~/.claude/ide` created if missing, file written with owner-only
  permissions (`QFile::setPermissions` to `ReadOwner | WriteOwner`) since it
  carries the auth token.
- Removed in the destructor and on `closeEvent`. On start, delete any stale
  `<port>.lock` whose `pid` is our own from an earlier crashed run; never
  delete other processes' files.

### Config

New `[claude]` section in `src/config.{h,cpp}`. Two keys only:

- `enabled` (bool, default `true`): gates `ClaudeIdeServer::start()` entirely.
  Off means no lock file, no port, no environment injection.
- `ide_name` (string, default `"neobeam"`): value written as `ideName`, which
  is what the CLI shows in its `/ide` list.

Register the section in the `validSections` table in `src/config.cpp` (near
line 154) so the unknown-key warning machinery covers it, and add commented
defaults to the generated sample config next to the existing `[side_terminal]`
block.

## Changes to existing files

### `src/terminalpanel.{h,cpp}`

`TerminalConfig` gains:

```cpp
/// Extra "NAME=value" entries merged into the shell environment of every
/// split. Used for the Claude Code IDE handshake variables, which is how a
/// CLI started in this panel discovers neobeam without any process sniffing.
QStringList extraEnvironment;
```

In `createTermWidget()`, before the shell starts:

```cpp
if (!m_cfg.extraEnvironment.isEmpty())
    term->setEnvironment(QProcessEnvironment::systemEnvironment().toStringList()
                         + m_cfg.extraEnvironment);
```

`QTermWidget::setEnvironment()` replaces the environment wholesale, so the
inherited one must be included explicitly. The panel does not call it today,
so this is the only behavioural change to existing terminals; verify that a
plain shell still gets a normal environment (`env | wc -l` before and after).

Also add a small setter so MainWindow can hand the entries over after the
server binds:

```cpp
/// Apply extra environment entries to splits created from now on. Already
/// running shells keep the environment they started with.
void setExtraEnvironment(const QStringList &entries);
```

Both panels get the entries, not just the side one: the bottom terminal is a
perfectly reasonable place to run `claude`, and the lock file is per
application anyway.

Ordering matters: `ClaudeIdeServer::start()` must run before the panels create
their first split, so it goes early in the `MainWindow` constructor, next to
the other providers.

### `src/mainwindow.{h,cpp}`

- New member `ClaudeIdeServer *m_claudeIde = nullptr;` under
  `#ifdef NEOBEAM_HAS_CLAUDE_IDE`.
- Construct and `start(m_titleCwd or the startup folder)` when `[claude] enabled`.
- Push `terminalEnvironment()` into both `TerminalPanel` configs.
- In `updateTitleCwd()`, call `setWorkspaceDir()` so a project switch is
  reflected in the lock file. Connected clients are unaffected; this only
  changes what a later `/ide` matches against.
- Delete the lock file in `closeEvent()` before the window goes away.

## Acceptance

1. `cmake --build build -j$(nproc)` clean, and clean again with
   `-DNEOBEAM_CLAUDE_IDE=OFF`.
2. Starting neobeam creates exactly one `~/.claude/ide/<port>.lock`, mode 0600,
   with the real cwd in `workspaceFolders`.
3. Closing neobeam removes it. Killing neobeam and restarting it leaves exactly
   one lock file again.
4. `claude` started in the side terminal reports the IDE as connected without
   the user typing `/ide` (auto-connect from the environment variables), under
   both the `claude` and `claude-personal` aliases.
5. `isConnected()` goes false when that `claude` exits.
6. A WebSocket client connecting with a wrong or missing auth header is
   rejected.
7. `[claude] enabled = false` produces no lock file, no listening port and no
   extra environment variables in the terminal.

Per `AGENTS.md`, the user runs the binary and reports back; no smoke test in a
virtual display.

## Risks

- **Undocumented protocol.** Anthropic can change it. Contained: one class, one
  optional CMake flag, and the feature degrades to "no IDE integration" rather
  than breaking the editor.
- **`QWebSocket::request()` on a server-side socket.** The header check depends
  on Qt keeping the handshake request on accepted sockets. Verify in phase 0.
  If the header is not reachable, fall back to localhost-only binding plus a
  logged warning, and document that the token is not verified.
- **Environment injection regressions.** `setEnvironment()` is all-or-nothing;
  a mistake here breaks every terminal, not just Claude's.

## Non-goals

- Codex. Its app-server architecture makes the terminal-side approach
  impossible (see the analysis in `001-brief-side-term-ai.md`).
- IDE-to-Claude notifications (`selection_changed`, `at_mentioned`).
- Any diff rendering, which is task 003.
