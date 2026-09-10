#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QFont>

/// A problem found while loading the config file
struct ConfigIssue {
    enum Severity { Warning, Error };
    Severity severity;
    QString message;
};

/// A pinned project from [[projects]] in neobeam.toml. Shown at the top of
/// the project switcher list.
struct ProjectEntry {
    QString path;         // absolute, tilde-expanded folder path
    QString name;         // display name; empty = folder basename
    QString sideCommand;  // side terminal command for this project; empty = plain shell
};

/// Application configuration loaded from TOML
struct Config {
    // Font settings
    QString fontFamily = "Monospace";
    int fontSize = 10;               // points
    QFont::HintingPreference hinting = QFont::PreferDefaultHinting;
    bool antialiasing = true;

    // Terminal settings
    bool terminalFollowCwd = true;
    bool terminalStartExpanded = false;   // open with terminal panel visible
    int terminalHeight = 300;             // startup height in pixels
    QString terminalFont;                 // empty = use global font
    int terminalFontSize = 0;             // 0 = use global fontSize
    QString terminalColorScheme = "Neoblue"; // QTermWidget color scheme (Neoblue, OneDark, Dracula ship with neobeam)
    QString terminalForeground;           // override scheme text color (e.g. "#f8f8f2"); empty = scheme default
    QString terminalBackground;           // override scheme background color; empty = scheme default
    QString terminalShell;                // empty = system default shell
    QStringList terminalShellArgs;        // args passed to shell
    QString terminalExecCommand;          // command executed after shell starts (e.g. "tmux")
    bool terminalRightClickPaste = false; // PuTTY-style right-click paste
    bool terminalCopyOnSelect = true;     // copy selection to PRIMARY + CLIPBOARD as it is made
    // Non-alphanumeric characters that count as part of a word when
    // double-clicking in the terminal. The default extends Konsole's
    // ":@-./_~" with the query-string characters so a double-click selects a
    // whole URL (which Ctrl+Shift+L then opens).
    QString terminalWordCharacters = ":@-./_~?&=%#+";
    bool terminalAntialiasing = true;     // font antialiasing in terminal
    QFont::HintingPreference terminalHinting = QFont::PreferDefaultHinting;
    int terminalScrollback = 1000;        // scrollback buffer size in lines (>= 1)
    // Send a bare Enter to any bottom-terminal split with nothing unsubmitted
    // typed into it after a branch switch (checkout/switch made in nvim or in
    // the terminal itself), so a branch-aware shell prompt picks up the new
    // name. Off by default: it injects a keystroke into a live shell.
    bool terminalRefreshPromptOnGitChange = false;

    // Side terminal (docked next to the minimap, for CLI tools like claude).
    // The command it runs comes from the matching [[projects]] entry's
    // side_command; there is no global default.
    int sideTerminalWidth = 400;          // startup width in pixels
    QString sideTerminalFont;             // empty = use terminal font
    int sideTerminalFontSize = 0;         // 0 = use terminal font size

    // UI extensions (native chrome)
    bool extPopupmenu = true;
    bool extTabline = true;
    bool extMessages = true;
    bool bufferline = true;           // show open buffers instead of tabpages
    bool extStatusline = true;        // native statusline widget
    bool uiGitStatus = true;          // git branch/status segment in the statusline (push-updated)
    bool uiGitsignsBranch = false;    // take the branch name from gitsigns (vim.b.gitsigns_head)
                                      // instead of the built-in git provider; gitsigns then
                                      // becomes a required plugin for the branch to show at all
    QString uiWindowTitle;            // window title template: %p project, %d cwd (~), %D cwd,
                                      // %f file name, %F file path (~), %% literal; empty = default title
    QString accentColor = "#50fa7b";  // greenish accent for native elements
    QString statuslineDivider = "powerline"; // "powerline", "pipe", "none"
    QString focusBorderColor = "#61afef"; // 1px border around the focused grid/terminal pane

    // Git branch switcher popup (the picker under the statusline's git segment).
    // A monospace family is what lets its commit-date column line up; a
    // proportional one still works, the dates just stop aligning exactly.
    QString branchSwitcherFont = "monospace";
    int branchSwitcherFontSize = 10;      // list rows, in points; the input line above them
                                          // renders one point larger

    // Mouse settings
    bool mouseTerminalSelection = true;  // terminal-style selection when vim mouse= empty
    bool mouseAutoCopy = true;           // auto-copy selection to clipboard on release
    QString mouseSelectionColor = "#44475a"; // selection highlight color

    // Session restore
    bool sessionRestoreWindow = true;    // window geometry + editor/terminal split proportions
    bool sessionRestoreTerminal = true;  // terminal split count, proportions, expanded state

    // Project switcher
    QList<ProjectEntry> projects;        // pinned projects, shown on top of the list
    bool switcherKeepRunning = true;     // show the switcher instead of quitting when nvim exits
    int switcherMaxRecent = 20;          // recent files/folders kept in the list (each)

    // Load config from given path, or standard path if empty.
    // Problems (unknown options, wrong types, invalid values) are collected
    // into issues when given, otherwise logged via qWarning. A bad value
    // never aborts the load; the affected option keeps its default.
    static Config load(const QString &path = {}, QList<ConfigIssue> *issues = nullptr);

    // Returns the configured QFont
    QFont font() const;

    // Returns the default config file path
    static QString configPath();
};
