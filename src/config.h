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
    int terminalHeight = 300;             // startup height in pixels
    QString terminalFont;                 // empty = use global font
    int terminalFontSize = 0;             // 0 = use global fontSize
    QString terminalColorScheme = "Linux"; // QTermWidget color scheme
    QString terminalShell;                // empty = system default shell
    QStringList terminalShellArgs;        // args passed to shell
    QString terminalExecCommand;          // command executed after shell starts (e.g. "tmux")

    // UI extensions (native chrome)
    bool extCmdline = true;
    bool extPopupmenu = true;
    bool extTabline = true;
    bool extMessages = true;
    bool bufferline = true;           // show open buffers instead of tabpages
    bool extStatusline = true;        // native statusline widget
    QString accentColor = "#50fa7b";  // greenish accent for native elements

    // Load config from standard path, returns default if not found
    static Config load();

    // Returns the configured QFont
    QFont font() const;

    // Returns the config file path
    static QString configPath();
};
