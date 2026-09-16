# Neobeam - Project Phases

## Phase 1: Skeleton + Connection ✓
- [x] CMake project scaffold with Qt6
- [x] Neovim process management (spawn `nvim --embed`, msgpack-RPC over stdin/stdout)
- [x] `nvim_ui_attach` with `ext_linegrid`
- [x] Grid renderer (QWidget painting character grid from `grid_line` events)
- [x] Build verified, nvim process connects successfully

## Phase 2: Input + Usability ✓
- [x] Keyboard input (Qt key events → Neovim input notation)
- [x] Mouse input (clicks, scroll, drag)
- [x] Cursor rendering (block/beam/underline per mode, blinking)
- [x] Resize handling (`nvim_ui_try_resize`)

## Phase 3: Visual Polish ✓
- [x] Font configuration (family, size, bold/italic, hinting, antialiasing)
- [x] `ext_hlstate` semantic highlights
- [x] Smooth scrolling (deferred to Phase 4 — needs per-window grids)
- [x] TOML config loading (~/.config/neobeam/neobeam.toml)

## Phase 4: Multi-grid + UI Chrome ✓
- [x] `ext_multigrid` (per-window grids, floating windows, z-ordering)
- [x] `ext_cmdline` (native command line widget)
- [x] `ext_popupmenu` (native completion popup)
- [x] `ext_tabline` (native tab bar)
- [x] `ext_messages` (native message area)
- [x] All UI extensions configurable in `[ui]` section of config
- [x] Native statusline (Qt widget, mode/file/position/diagnostics, configurable segments)

## Phase 5: Terminal + Advanced
- [x] Embedded QTermWidget (splits, follows nvim cwd)
- [x] Minimap (buffer content + highlight data)
- [x] Image rendering (via `vim.ui.img` API when available)

## Phase 6: Native Image Preview
- [ ] Custom RPC notification handler (`neobeam_image_show`, `neobeam_image_hide`)
- [ ] Signal pipeline: NvimProcess → MainWindow → ImageRenderer
- [ ] Bundled Lua plugin (`neobeam_preview.lua`) with adapter pattern for file browsers:
  - [ ] netrw / Lexplore
  - [ ] oil.nvim
  - [ ] neo-tree.nvim
  - [ ] snacks.nvim explorer
  - [ ] telescope.nvim (preview pane)
- [ ] Configurable preview position (right half, floating, fixed column)
- [ ] Supported formats: PNG, JPEG, GIF, BMP, WebP, SVG
- [ ] Graceful fallback (no-op when file is not an image)
- [ ] Config option `[preview]` section: enable/disable, max size, position preference

## Phase 7: Improvements
- [x] Improve the statusline (dividers, etc)
- [x] Mouse - terminal like mouse behaviour in nvim buffers on mouse= setting in vim. Allow selection with cursor with auto copy of selected test to clipboard that won't trigger nvim VISUAL mode on click or mouse drag.
- [x] Consider adding mouse behaviour to .toml config
- [x] Make text in the UI statusline selectable.
- [x] Ctrl + Shift + E splits the terminal
- [x] Ctrl + Shift + W closes active split
- [x] Instead add (arrow down button) that sends current nvim cwd to all open terminals.
- [x] When a folder is open with neobeam, session restore should be ignored and the folder path should be open both in nvim buffer and terminal cwd.
- [x] Ctrl+Tab anywhere in the neobeam should cycle between terminals and nvim area.
- [x] Side terminal - apart from bottom terminal pane add button to open side temrinal. This will be in the space next to the minimap. It is intended for the to run things like claude or nanocoder in the cli. It should always open in the same folder as current nvim cwd:
    - [x] Add config option for default command to run with side terminal e.g. claude, nanocoder
    - [x] Side terminal is not affected by the "Send cwd to all terminals button"
- [x] Add independent font configuration for the side terminal.
- [x] For all terminals, add keyboard shortcuts Ctrl - to decrease the font size and Ctrl = to increase it. Holding Ctrl + mouse wheel up and down will also increase/decrease the term font.
- [x] Add option to keep neobeam running when nvim buffer is exited
- [x] Project switcher
    - [x] Add project switcher - when neobeam is started without argument a project switcher with fzf search through recently opened folders and files (Like nvim Telescope)
    - [x] Switcher can use qt gui elements with typing area and a drop down list, maybe something like k runner in the middle of open neobeam window
    - [x] When a file is selected, cwd of nvim and all terminals (including side term) changes to the directory where the selected file is.
    - [x] You can also projects in neobeam.toml that will be on top of the switcher list.
    - [x] You can configure side term command for each specific folder or leave it without any command.
    - [x] We will remove the default side term command from the config
    - [x] When a project is selected with Project switcher, Neobeam window title should change to <Project name> - <path> the word "Neobeam" Should not show. e.g.:  Myproject - ~/projects/myproject -> project name shows while nvim cwd stays in the project dir; cd elsewhere reverts to "Neobeam - ~/path". New neobeam.title debug trace category for headless checks.
    - [x] Configurable window title: [ui] window_title template, empty = default behaviour. Placeholders %p project name, %d cwd (~), %D cwd (absolute), %f file name, %F file path (~), %% literal. Current file pushed via existing neobeam_file BufEnter notification; unknown placeholders render literally and warn in `neobeam -k`.
- [x] Config
    - [x] Add simple config validation e.g. `neobeam -k`
    - [x] Handle non-existent or wrong options in the .toml config.
- [x] **Term + Project improvements**
    - [x] Terminal select + copy to PRIMARY + CLIPBOARD 
        - [x] Add separate [terminal] config option for this? -> yes, [terminal] copy_on_select (default true)
        - [x] How to handle possible conflict with tools like wl-copy? -> no conflict; standard last-writer-wins selection semantics, no coordination needed
    - [x] Neobeam window title
        - [x] Make Neobeam uppercase
        - [x] Append project path or cwd folder path (if no project selected) e.g. Neobeam - ~/projects/neobeam
    - [x] Nvim buffer artifacts
        - [x] Sometimes when snacks.nvim file manager or nvim-tree is opened it leaves the vertical line in the buffer. Happens mainly when Neobeam window is stretched wide.
        - [x] Nvim terminal buffers (when you open one in the neobeam nvim buffer) also leaves the horizontal buffer separator artifact in the neobeam nvim buffer when closed.
        - [x] When there is a warning or error displayed in the nvim buffer statusline it is clipped. (e.g. lua plugin error, or some vim script error etc.) -> new approach on fix/warning-error-area: ext_messages now actually passed at ui_attach (was configured but never sent, so the message grid was in use); MessageWidget reworked into a timestamped, expandable 2-line message log below the buffer ("Messages" toggle, scrollbar, selectable + auto-copy like statusline). Verified headless (msg_show kinds echomsg/lua_error arrive incl. full traceback, msg_set_pos gone); awaiting visual confirmation
    - [x] Neobeam UI statusline:
        - [x] Add git status (optimize performance avoid frequent scanning, or use push model) -> push model: nvim autocmd rpcnotify + .git HEAD/index file watcher + cwd changes, debounced async `git status`; counts in accent_color ([ui] git_status option)

## Phase 8: Text selection mode
- [x] Implement mode where nvim buffer in neobeam behaves as if nvim is set to mouse= (mouse off)
    - [x] The cursor is set to beam and you have a selection in the term. -> mouse pointer becomes I-beam while mode is on; nvim caret stays nvim-controlled
    - [x] Selected text is copied to Clipboard -> CLIPBOARD + PRIMARY, same targets as the terminal panels; explicit selections copy regardless of [mouse] auto_copy
    - [x] Mode can be toggled with a button or keyboard shortcut Ctrl + Shift + . -> both: checkable "⌶" button in the bottom-right strip + Ctrl+Shift+.
    - [x] Holding Shift and drag selecting will also trigger this mode if not active (terminal like selectin behaviour) -> one-shot for that drag only; press is deferred so a plain Shift+click still reaches nvim as <S-LeftMouse>
    - [x] Drag selection in this mode won't trigger nvim visual selection mode.
    - [x] Fix uncovered along the way: drag coords are now clamped to the grid the selection started in, so dragging across a split/float no longer produces a garbage selection.
    - [x] Selection highlight follows the text on scroll (shifted by grid_scroll deltas); dropped once the selected rows scroll out of the window instead of floating over new content.
    - [x] Fix: multi-line selection copied empty lines. gridAt() iterated m_grids (QHash, randomized order) and grid 1 - the full-window backdrop that holds no buffer text under ext_multigrid - could match before the window grid, so the overlay drew at the right pixels but extraction read blank cells. Grid 1 is now excluded from the hit loop and only used as fallback. -> awaiting visual confirmation
    - [x] Shift + double click (or double click while the mode is on) selects the word under the pointer and copies it (xterm-style char classes: word = alnum + underscore). Previously there was no double-click handler at all: the deferred Shift+press leaked to nvim as two quick <S-LeftMouse> clicks, nvim drew its own visual word selection, and nothing was copied. A dragless Shift+click is now held for the double-click interval before being forwarded (flushed early on any other press/keypress), so plain Shift+click still reaches nvim as <S-LeftMouse>. -> awaiting visual confirmation


## Pipeline adjustments
 - [x] Allow to publish release build from branch
 - [ ] Do not run build pipelines in PR/branch when only .md files are updated.
 - [x] Add BSD build pipeline -> separate manual-only workflow (.github/workflows/openbsd.yml, vmactions/openbsd-vm, workflow_dispatch with release input); OpenBSD port skeleton in packaging/openbsd/
 - [ ] Create comprehensive project documentation with examples on GitHub - suggest best approach.

## Fixes/Adjustments

### General
- [x] Add switch -c to start neobeam with a custom config file path
- [x] Create example config file in the project root
### Term
- [ ] Enable sixel support for QTermWidget (blocked: upstream has no sixel support)
- [x] Add Shift+Insert and Ctrl+Shift+V bindings to paste clipboard to terminal
- [x] Add option to enable PuTTY-style right-click paste in the terminal to the config
- [x] Add options to enable text anti-aliasing and hinting for terminal font
### Improvements 2026-08-03
- [x] It seems something in neobeam terminals captures Shift+Tab for the terminals (e.g. Shift+Tab is used in Codex CLI)
    - It was Qt's own focus navigation: `Konsole::TerminalDisplay::focusNextPrevChild()` swallows
      plain Tab but deliberately leaves Shift+Tab (`Key_Backtab`) to the window, which moved focus
      out of the panel. `MainWindow::focusNextPrevChild()` now refuses navigation while a terminal
      has focus, so Qt falls through to the terminal's key handler and the shell gets `ESC[Z`.
- [x] Some links are still not clickable (when part of some TUI apps). Check if handling is same for the side terminal as main terminal.
    - Handling is identical for both panels: every split of both is created by
      `TerminalPanel::createTermWidget()`, which is the only place `urlActivated` is wired.
    - qtermwidget's `UrlFilter` only matches URLs with an explicit scheme, on a single line.
      Added on the neobeam side: right-click on a detected link offers Open/Copy Link (it used to
      just paste with `right_click_paste`), a wider matcher behind Ctrl+Shift+L that resolves the
      link in the terminal selection (scheme-less hosts, e-mail, fragments an application wrapped
      across lines), and `[terminal] word_characters` so a double-click grabs a whole URL.
    - Still impossible: OSC 8 hyperlinks (see Upstream issues below) - the URL never reaches the
      screen buffer, so nothing in neobeam can recover it.
- [x] If Neobeam is opened without side terminal command e.e.g "Open file with Neobeam" or from file picker, start with side terminal collapsed)
    - The saved "side terminal was visible" state is only restored when the launch resolves a
      `side_command`, and a suppressed panel does not overwrite that saved state on close, so the
      next launch inside the project still brings it back.
    - Starting with no arguments (switcher, also the `keep_running` restart after `:q`) resolves no
      project at startup, so `applyProjectSideCommand()` now opens the side terminal when the picked
      project defines a `side_command` - that path never showed the panel at all before.
- [x] Add "maximized" mode - Ctrl + Shift + x would hide all terminals, side terminal and also messages. Pressing it again will restore it to the previous state.
    - `MainWindow::toggleMaximizedMode()` captures which of the bottom terminal, side terminal and
      messages panel are actually visible before hiding them, so a panel the user had already closed
      stays closed on restore instead of popping back open.
    - Driven through the existing toggle buttons/`MessageWidget::hidePanel()`/`showPanel()` rather than
      raw `hide()`/`show()`, so button check-state and session persistence stay consistent with normal
      manual toggling.
    - Side terminal restore calls `show()` directly instead of going through its toggle button: that
      button's handler re-fetches nvim's cwd and focuses the terminal, which is right when a user opens
      it fresh but wrong when just restoring pre-maximize state. `showEvent`'s own `visibilityChanged`
      still keeps the button's checked state in sync (same pattern session restore already relies on).
      Keyboard focus is explicitly returned to the grid on both enter and exit either way.
- [x] Terminal prompt injection / refresh on git status. I'd like the bottom terminal to listen for git changes and when there is a new branch to either refresh / inject the terminal prompt (zsh or bash, depending what's enable in the config). Simple fallback solution would be to just send "Enter" into the terminal (if no letters are typed there to avoid executing some random command).
    - Went with the fallback: `GitStatus` (already push-updated via nvim autocmds + a
      `.git/HEAD`/`.git/index` file watcher, so it already sees branch switches typed
      straight into the terminal) gained a `branchChanged(QString)` signal, emitted only
      when the branch itself differs from the previous status, not on every status-count
      tick. `TerminalPanel` connects it to `refreshPromptOnBranchChange()`, which sends a
      bare `\r` to every split whose current line has nothing unsubmitted typed into it.
      Bottom terminal only, not the side terminal (usually an agent/REPL, not a shell -
      Enter there risks submitting an empty prompt to it).
    - Config-gated, off by default: `[terminal] refresh_prompt_on_git_change` (`config.h`/
      `config.cpp`, `TerminalConfig::refreshPromptOnGitChange`). It's still a keystroke
      injected into a live shell, so `refreshPromptOnBranchChange()` itself no-ops unless
      enabled - documented with a warning in README.md and neobeam.toml.example.
    - "Nothing typed" has no public API to check (QTermWidget exposes no way to read the
      shell's readline buffer or the current screen line), so it's tracked as a heuristic:
      an event filter on each split's internal `Konsole::TerminalDisplay` (same object
      already filtered for copy-on-select) watches `QKeyEvent`s and flips a per-split dirty
      bit - Enter/Ctrl+C/Ctrl+U clear it, everything else sets it. Conservative in one
      direction only: e.g. Backspacing back to an empty line still reads as dirty, so the
      failure mode is a skipped refresh, never a stray Enter into a line being typed.
    - Considered sending Ctrl+L (redraw) instead of Enter: rejected because neither bash's
      readline `clear-screen` nor zsh's ZLE `clear-screen` re-expand `PS1`/`RPS1` - both just
      redraw the already-buffered prompt text, so a branch segment (zsh `vcs_info`, etc.)
      would stay stale. Enter is what actually forces a fresh prompt expansion.

### Git segment 2026-08-21
- [x] Statusline git segment sometimes kept the old branch after a checkout made outside the app
    - Root cause: `GitStatus` watched `.git/HEAD` and `.git/index` as files. git publishes both by
      renaming a lockfile over the target, so the watch was left on the unlinked old inode, and the
      re-arm skipped any path the watcher still listed - which is every path, since the name resolves
      again after the rename. The watch stayed dead for the rest of the session. nvim sees nothing for
      an external checkout, so the watcher was the only trigger left.
    - `GitStatus::watchGit()` now also watches the git dir and `refs/heads` as directories (a
      directory inode is never replaced) and re-adds every path unconditionally on each event.
    - `git status` runs with `--no-optional-locks`: watching the git dir made a self-retrigger loop
      possible, since plain `git status` may rewrite `.git/index` to refresh its stat cache.
    - `MainWindow::eventFilter` refreshes on `QEvent::WindowActivate`, the one non-push trigger. It
      covers what no watcher can see: filesystems without change notification, and a repository
      created in the cwd while the app was unfocused (`refresh()` retries the git-dir resolve then).
- [x] Branch name no longer requires gitsigns
    - `StatuslineWidget::refresh()` preferred `vim.b.gitsigns_head` over the built-in provider, so the
      displayed name followed gitsigns' detection even though `GitStatus` had just run `git`.
    - The provider is now the default source. `[ui] gitsigns_branch = true` restores the old
      precedence; the counts come from the provider either way.


### Improvement segment 2026-08-31
- [x] Advanced git stats. I'd like that when I hover my mouse over git section of neobeam statusline I'll see some more detailed git stats (hover frame with text). It would have list of changed files. Number of commits etc. Changed files would be in OneDark green. If the list of changed file is too long. show just ...and 20 more files, etc. Mind the performance for this one.
    - No new git process: `GitStatusInfo` gained `changedFiles` (`gitstatus.h`), populated in
      `parseStatus()`'s existing per-entry loop (`gitstatus.cpp`), which already read every status
      line and previously discarded the path after counting it. "Number of commits" reads as
      ahead/behind vs upstream (already parsed from `git status -b`), not a new `git rev-list` call.
    - Tooltip built in `StatuslineWidget::refresh()` alongside the existing branch/counts text and
      set via `QLabel::setToolTip()` on `m_branchLabel` (rich HTML) - reuses Qt's native tooltip
      instead of a custom hover widget. Rebuilt on every `refresh()` (string formatting only, no
      I/O), not gated behind a "did git status change" check.
    - Capped at 20 files total (`kMaxTooltipFiles`), then `...and {overflow} more files`. Tracked
      (staged/modified) paths colored `#98c379` (OneDark green), untracked paths `#61afef` (OneDark
      blue, the same blue already used elsewhere in neobeam for the focus-border color) - tracked
      shown first, then untracked, both literal colors regardless of the statusline's own Tokyo
      Night palette. `GitStatusInfo` splits `changedFiles`/`untrackedFiles` into separate lists so
      the two can be colored differently without re-deriving status per path in the UI layer.
    - Known limitation: git quotes paths with spaces/unicode/special characters in porcelain v1
      output (`"..."` with C escapes); left unescaped in the tooltip rather than switching the
      line-based parser to `-z`/NUL-delimited output. Verified by hand against a scratch repo with
      30 changed/untracked entries including a spaced filename - not by running the GUI myself, per
      AGENTS.md - awaiting visual confirmation.
- [x] Git switcher - add interface for git branch switcher (similar to vscode) - probabbly somewhere under the status line
    - [x] Switch between branches with dropdown
    - [x] Or create new branch and switch to it (`git checkout -b some-branch`)
    - `BranchSwitcherWidget` (`branchswitcher.{h,cpp}`): a popup child of `MainWindow`, anchored to
      the statusline's git segment via `mapTo()` and flipped above it when the window has no room
      below (the usual case, the statusline sitting at the bottom of the editor column). Same input
      + list + palette as `SwitcherWidget`, so the two pickers read as one UI; the fuzzy scorer they
      share moved out of `switcher.cpp` into `fuzzy.h`.
    - Opened by clicking the git segment or with Ctrl+Shift+B, the latter handled in
      `MainWindow::eventFilter` rather than as a `QShortcut`: QTermWidget takes Ctrl+key presses
      before a shortcut fires, so a `QShortcut` reached everywhere except inside the terminal -
      the same reason the Ctrl+-/Ctrl+= font keys already live there. The modifier match is exact,
      so plain Ctrl+B (nvim's page-up, tmux's prefix) is untouched and only the Shift variant is
      claimed. No other neobeam binding uses it; the one wrinkle is that a terminal cannot
      normally tell Ctrl+Shift+B from Ctrl+B, so a tmux user who holds Shift over the prefix key
      gets the switcher - Ctrl+Shift+G (VSCode's Source Control binding) is the alternative if
      that ever bites. The click needed `SelectableLabel`
      to gain an optional click handler, which fires only on a release that selected no text, so
      selecting the statusline text still works. `StatuslineWidget::setGitSegmentClickable()` gates
      both the handler and the pointer cursor, and adds the "click to switch branches" line to the
      existing hover tooltip. A click on the segment while the popup is up dismisses it through the
      input's focus loss before the click is delivered, so `toggle()` ignores an open request within
      250ms of a close instead of reopening what the click just closed.
    - No new git plumbing: `GitStatus` gained `requestBranches()`/`checkoutBranch()` plus
      `branchesReady`/`checkoutFinished`, since it already resolves the repository and owns every
      git invocation. The widget runs no process itself. Listing is one
      `git for-each-ref --sort=-committerdate refs/heads refs/remotes` per open (not cached: a
      fetch or a branch made in the terminal would make a cached list wrong), asked for on open
      only, so the segment's own refresh path is untouched.
    - Remote-only branches are listed after the locals, and checking one out drops the remote
      prefix (`origin/feature-x` -> `feature-x`), which is what makes git create a local tracking
      branch instead of detaching HEAD. A remote whose stripped name already exists locally is not
      listed twice. `refs/remotes/*/HEAD` is skipped by matching the full refname: git shortens
      that symref to the bare remote name ("origin"), not to "origin/HEAD", so a short-name check
      would have missed it.
    - `git checkout` rather than `git switch`: `switch` is 2.23+, and checkout's DWIM rule is what
      the remote-branch case relies on anyway. Creating uses `checkout -b`, which also works on an
      unborn HEAD (verified in a scratch `git init`), so the create row stays useful in a fresh
      repository.
    - A refused checkout (dirty worktree, branch checked out in another worktree, invalid name)
      keeps the popup open and shows git's stderr verbatim, capped at 400 chars. On success the
      popup closes, `GitStatus::refresh()` runs without waiting for the .git watcher, nvim gets a
      `checktime` so open buffers are rechecked against the new working tree, and the existing
      `branchChanged` -> terminal prompt refresh keeps a branch-aware shell prompt current.
    - The create row is last in the list, not first as in VSCode: switching to an existing branch
      is the common case, and Enter must not create a branch by surprise. It appears only for a
      non-empty, whitespace-free name that no listed branch has (both the display name and the
      checkout target are compared, so "feature-x" is not offered when only "origin/feature-x"
      exists).
    - The segment is led by a branch glyph (`BranchIcon` in `uiwidgets.cpp`), custom-painted from
      ionicons' git-branch outline - three rings, a trunk and a cubic curve - instead of loaded
      from the SVG. Painting it means the pen takes the segment's own foreground colour, so it
      follows the statusline palette with no per-theme recolouring and no runtime Qt6::Svg, which
      the rest of neobeam avoids too. The artwork's bounding box inside the 512 viewBox (the rings
      sit off-centre in it) is mapped onto the glyph, pen width included, and the icon shares the
      branch slot's colours, visibility, tooltip and click handler. Geometry checked by rendering
      the same paint code to a PNG offscreen, not by running the GUI.
    - Glyph height and the padding either side are multiples of the segment font's cap height, not
      pixel counts: the statusline font is point-sized, so on a high-DPI screen the text grows with
      the logical DPI while a fixed pixel glyph does not - it showed up on a 4K display as a glyph
      half the height of the branch name and crammed against it. `applyFontMetrics()` recomputes
      them on every `QEvent::FontChange`, which is also how a move to a differently scaled screen
      arrives. `segmentFont()` now hands the same font to `SelectableLabel` and the glyph so the
      two cannot drift apart. Still fixed in pixels, same family of bug, not touched here:
      `StatuslineWidget`'s `setFixedHeight(28)` and `PowerlineDivider`'s `setFixedWidth(12)`.
    - The padding is applied as given rather than split evenly across the widget - an earlier
      version centred the glyph in its own width, which silently averaged the two padding
      constants, so the right-hand one never took effect. The glyph's painted width is fractional,
      so the widget width ceils it; rounding down clipped the right edge at zero right padding.
    - Styled apart from the project switcher on request: highlighted with `[ui] focus_border_color`,
      the OneDark blue neobeam already uses for the focused pane, instead of the green
      `accent_color`. The border and the selected row are that same colour at fixed alphas via a
      small `rgbaOf()` helper, so configuring the one colour recolours the popup coherently and no
      new config key was needed.
    - Monospaced rows with the commit dates in an aligned column. `showRows()` measures in pixels
      rather than counting characters, because the row glyphs are geometric shapes a monospace
      font usually lacks - they come from a fallback whose advance is not one cell - then pads
      each name out to the widest with spaces. The fonts are set on the widgets instead of in the
      style sheet: a style sheet font wins at paint time, which would leave the alignment measured
      with metrics that are not the ones drawn. Family and size come from `[ui]
      branch_switcher_font` / `branch_switcher_font_size` (defaults "monospace" and 10, the values
      they were hardcoded to); the size applies to the list rows and the input line adds
      `kInputSizeBump`. Out-of-range sizes are clamped to 4-72 with a warning, the bounds the
      editor and terminal font zoom already uses.
    - The name column takes what is left of the popup once the widest date and the gap are
      reserved (`popupWidth()`, shared with `reposition()`), so a date is never pushed past the
      right edge at any window size; a name too long for the column is middle-elided and stays in
      full in the row's tooltip. Checked by rendering the real widget offscreen at 1920 and 900px
      window widths, not by running the GUI.
    - The popup is laid out while still hidden and only shown once the branch list lands (or after
      150ms, if git is slow enough that showing nothing would read as the click having missed).
      Revealing it empty first made it appear under the statusline - short enough to fit there -
      and jump above it a few milliseconds later once the rows arrived, which is what the flash
      reported after the first build was. A pending open counts as open for `toggle()` and is
      cancelled by `dismiss()`, having never taken focus.
    - Which side of the anchor the popup sits on follows its height until the list has landed,
      then freezes for the rest of the open: deciding it from the empty loading-state layout
      picked "below" and left the grown popup clamped over the statusline, and re-deciding on
      every relayout sent it hopping across the statusline as a filter shrank the list. The bottom
      edge stays pinned just above the anchor, so shrinking grows the gap upward instead of
      moving the popup. Checked offscreen with the anchor at the window bottom and mid-window (a
      terminal panel open below the statusline), before and after typing a filter.
    - Gated on `[ui] git_status` (it is created alongside `GitStatus`); with the segment off there
      is neither a click target nor a shortcut. Built and compiles clean; not run by me per
      AGENTS.md - awaiting visual confirmation.
- [x] Increase/Decrease font size for nvim buffers (Ctrl + and Ctrl -) - Evaluate possiblities.
    - `GridWidget::changeFontSize()` steps `m_font`'s point size (clamped 4..72, the same bounds the
      terminal panel uses) and recomputes the cell metrics through the new `applyFontMetrics()`,
      which the constructor now shares. `recalculateGrid()` then emits `gridSizeReady`, so nvim is
      told the new column/row count before the repaint.
    - Driven from the `MainWindow::eventFilter` branches that already served the terminal: the key
      branch falls through to the grid when no terminal panel has focus, the wheel branch when the
      pointer is over the grid. Neither `<C-=>` nor `<C-->` is a default nvim mapping.
    - In-memory only, like the terminal's own font size: a restart starts from `[font] size` again.
    - Not done: honoring `:set guifont`. `GridWidget::handleOptionSet()` already receives the
      `option_set` events and reads only `mouse`, so it is the hook if this is wanted later. It would
      also bring font *family* changes, at the cost of a guifont string parser and a rule for which
      source wins after a manual zoom.
- [x] Allow maximization also for bottom term and side term.
    - `m_maximized` became `MaximizedPane { None, Editor, Terminal, SideTerminal }`; Ctrl+Shift+X
      picks the target from `terminalPanelFor(QApplication::focusWidget())`, editor as fallback.
    - Bottom terminal maximizes by hiding `m_hsplitter` (the editor area and side terminal are its
      two halves); the side terminal by hiding `m_editorArea` and closing the bottom panel through
      `m_termBtn`, so button state and session persistence stay consistent with normal use.
    - Both splitter states are captured on the way in and restored on the way out: a hidden child's
      space is handed to its siblings and is not given back on show. `closeEvent` un-maximizes
      before saving, so quitting while maximized does not persist the collapsed layout.
- [x] Project switcher path mode - type a full path with autocomplete and open the folder or file.
    - Ctrl+D toggles the single input line between `Mode::Projects` (the existing fuzzy query over
      pinned projects and recents) and `Mode::Path`. Path mode starts prefilled with nvim's cwd plus
      a separator, so its contents are listed immediately; the text does not carry across a mode
      switch, since a fuzzy query and a path do not share a syntax. Tab is left to completion, which
      is the key a path field is expected to answer; Ctrl+D shadows only QLineEdit's X11
      delete-forward binding, which Del already covers.
    - `refilterPath()` splits the text at the last `/`: the left half names the directory to list
      (`~` expanded, relative paths anchored at nvim's cwd via `MainWindow::openSwitcher`), the
      right half is matched with the same `fuzzyScore()` the project mode uses. Dotfiles appear only
      once the fragment itself starts with `.`; rows are capped at `kMaxPathEntries` (200).
    - Tab writes the highlighted candidate into the input and each Tab after that steps to the next
      one, wrapping. `fillFromRow()` blocks `textChanged` while doing it, so the candidate set being
      cycled cannot be rebuilt from the text just written. Enter then confirms the pending
      completion (refilter only, no open), which is what descends into a folder; the Enter after
      that opens. Right fills and confirms in one keystroke, at the end of the line only.
    - With no completion pending, Enter prefers an exactly typed existing path over the highlighted
      row until the selection is moved, so `~/Projects/` opens that folder while `~/Proj` opens the
      completion.
    - Selected entries reuse `applySwitcherSelection()` unchanged: the item carries `Folder` or
      `File` per what is on disk, so a folder sets the cwd and opens the file browser and a file
      opens with its parent as cwd. No new config keys.

## Side TERM - AI agent features

### Claude
[ ] **Support diff view**
- IF Claude CLI supports it (does it have some api/control interface?) Enable if claude is running in the side term (how to do the detection properly? E.g. I use alias claude and claude-personal for personal account)
- (available in Claude Code cloud and some editors like Zed)
- Diff view can be expanded next to the side term
- Add button and a keyboard shortcut for it

### Codex (ChatGPT)
- Same for Codex CLI if possible.


## Project rename

Planned rename neobeam → neoqute ("neobeam" is already used by multiple projects on GitHub).
Since the repo is still private and no packages are published, this is a clean find-and-replace +
file renames with no user migration or compatibility shims needed. Do it in one commit/branch —
the Lua RPC event names exist on both the C++ and Lua side and must change together.

- [ ] Verify `neoqute` is free on GitHub and AUR before starting (note: name echoes qutebrowser, unrelated project — accept or reconsider)
- [ ] Rename the GitHub repo
- [ ] Build system:
    - [ ] `CMakeLists.txt`: `project(neobeam ...)`, target name (changes installed binary to `bin/neoqute`), `NEOBEAM_*` variables and compile definitions (`NEOBEAM_HAS_TERMINAL`, `NEOBEAM_ICON_SIZES`, `NEOBEAM_TERMINAL` option — also referenced in both workflows)
    - [ ] Icon pipeline: rename `resources/icons/neobeam.svg` + placeholder SVG, generated `neobeam_<size>.png` names in `cmake/AppIcons.cmake` and `main.cpp`
- [ ] Qt application identity:
    - [ ] `main.cpp`: `setApplicationName` (determines config dir `~/.config/neoqute/`), `setDesktopFileName`
    - [ ] `config.cpp`: hardcoded `neobeam.toml` filename, generated config template text, default window title `"Neobeam - ~/path"`, `neobeam -k` hints
- [ ] Desktop file + packaging:
    - [ ] `packaging/neobeam.desktop`: filename, `Name`, `Exec`, `Icon`, `StartupWMClass` (must match `setApplicationName` for taskbar grouping)
    - [ ] `packaging/arch/PKGBUILD` + `packaging/aur/neobeam-git/`: pkgname, dir name, `url=`, regenerate `.SRCINFO`
    - [ ] `.github/workflows/release.yml`: tarball prefix and artifact names
- [ ] Lua/RPC layer (rename C++ and Lua sides together):
    - [ ] `_G.neobeam` table, augroup names, RPC event names (`neobeam_cwd`, `neobeam_file`, `neobeam_git_event`, `neobeam_image_show/hide`) — listeners in `mainwindow.cpp` match these strings exactly
    - [ ] `lua/neobeam-plugin.lua` filename (and the `neobeam_preview.lua` install name it documents)
    - [ ] QLoggingCategory names (`neobeam.minimap`, `neobeam.redraw`, `neobeam.title`) + verify skill references
- [ ] Config examples + docs: `neobeam.toml`, `neobeam.toml.example`, `README.md`, `PROJECT.md`, `AGENTS.md`
- [ ] `Neoblue.colorscheme` stays — separate name, no need to touch

## Upstream issues

### qtermwidget
- `QTermWidget::copyAvailable(bool)` is never emitted (verified in 2.4.0 sources). The only
  emitter is the `TerminalDisplay::selectionChanged()` slot, but the connection from
  `ScreenWindow::selectionChanged` to it is missing in `TerminalDisplay::setScreenWindow()`,
  so there is no usable selection-change notification in the public API.
  - Workaround in neobeam: copy-on-select (`[terminal] copy_on_select`) installs an event
    filter on the internal `Konsole::TerminalDisplay` widget and copies the selection on
    left-button release (`TerminalPanel::eventFilter`).
  - If fixed upstream, the event filter can be replaced with a plain connect to
    `QTermWidget::copyAvailable`.
- No sixel image protocol support (blocks the "Enable sixel support" item above). QtermWidget issue here: [552](https://github.com/lxqt/qtermwidget/issues/552)
- No OSC 8 hyperlink support (verified in 2.4.0: `Vt102Emulation` parses no `ESC ] 8 ; ; uri ST`
  sequence, so the URI is discarded and only the display text reaches the screen buffer). TUI
  applications that print a label with a hidden URL are therefore unclickable in neobeam, and no
  workaround is possible from the outside - the filter chain that produces link hotspots is not
  reachable through the public API either (`TerminalDisplay::filterChain()` needs a header
  qtermwidget does not install).
  - Workaround for the cases that *are* recoverable (scheme-less URLs, links an application wrapped
    across lines): select the link and press Ctrl+Shift+L (`TerminalPanel::openSelectedLink`).
