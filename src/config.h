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
    bool terminalAntialiasing = true;     // font antialiasing in terminal
    QFont::HintingPreference terminalHinting = QFont::PreferDefaultHinting;
    int terminalScrollback = 1000;        // scrollback buffer size in lines (>= 1)

    // Side terminal (docked next to the minimap, for CLI tools like claude)
    QString sideTerminalCommand;          // command run when the side terminal opens (e.g. "claude")
    int sideTerminalWidth = 400;          // startup width in pixels
    QString sideTerminalFont;             // empty = use terminal font
    int sideTerminalFontSize = 0;         // 0 = use terminal font size

    // UI extensions (native chrome)
    bool extPopupmenu = true;
    bool extTabline = true;
    bool extMessages = true;
    bool bufferline = true;           // show open buffers instead of tabpages
    bool extStatusline = true;        // native statusline widget
    QString accentColor = "#50fa7b";  // greenish accent for native elements
    QString statuslineDivider = "powerline"; // "powerline", "pipe", "none"
    QString focusBorderColor = "#61afef"; // 1px border around the focused grid/terminal pane

    // Mouse settings
    bool mouseTerminalSelection = true;  // terminal-style selection when vim mouse= empty
    bool mouseAutoCopy = true;           // auto-copy selection to clipboard on release
    QString mouseSelectionColor = "#44475a"; // selection highlight color

    // Session restore
    bool sessionRestoreWindow = true;    // window geometry + editor/terminal split proportions
    bool sessionRestoreTerminal = true;  // terminal split count, proportions, expanded state

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
