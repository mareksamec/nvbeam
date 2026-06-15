#pragma once

#include <QString>
#include <QStringList>
#include <QFont>

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
    QString terminalColorScheme = "Linux"; // QTermWidget color scheme
    QString terminalShell;                // empty = system default shell
    QStringList terminalShellArgs;        // args passed to shell
    QString terminalExecCommand;          // command executed after shell starts (e.g. "tmux")
    bool terminalRightClickPaste = false; // PuTTY-style right-click paste
    bool terminalAntialiasing = true;     // font antialiasing in terminal
    QFont::HintingPreference terminalHinting = QFont::PreferDefaultHinting;

    // UI extensions (native chrome)
    bool extPopupmenu = true;
    bool extTabline = true;
    bool extMessages = true;
    bool bufferline = true;           // show open buffers instead of tabpages
    bool extStatusline = true;        // native statusline widget
    QString accentColor = "#50fa7b";  // greenish accent for native elements
    QString statuslineDivider = "powerline"; // "powerline", "pipe", "none"

    // Mouse settings
    bool mouseTerminalSelection = true;  // terminal-style selection when vim mouse= empty
    bool mouseAutoCopy = true;           // auto-copy selection to clipboard on release
    QString mouseSelectionColor = "#44475a"; // selection highlight color

    // Load config from given path, or standard path if empty
    static Config load(const QString &path = {});

    // Returns the configured QFont
    QFont font() const;

    // Returns the default config file path
    static QString configPath();
};
