#pragma once

#include <QString>
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

    // Load config from standard path, returns default if not found
    static Config load();

    // Returns the configured QFont
    QFont font() const;

    // Returns the config file path
    static QString configPath();
};
