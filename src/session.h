#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

class QSettings;

/// Persists app-managed session state (window geometry, terminal layout) to an
/// INI file alongside the user config, at AppConfigLocation/session.ini.
///
/// This is intentionally separate from Config (neobeam.toml): the TOML file is
/// user-authored and generated with everything commented out, so writing opaque
/// binary blobs (QSplitter/geometry state) back into it would clobber the user's
/// formatting. Session state is machine-written and lives in its own file.
class Session
{
public:
    Session();
    ~Session();

    /// Absolute path to the session INI file.
    static QString filePath();

    // --- Window geometry (best-effort; WM/compositor may override) ---
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

    // --- Editor/terminal vertical splitter proportions ---
    QByteArray verticalSplitterState() const;
    void setVerticalSplitterState(const QByteArray &state);

    // --- Terminal panel layout ---
    bool hasTerminalState() const;          // false until first save
    int terminalSplitCount() const;         // number of terminal splits (>= 1)
    void setTerminalSplitCount(int count);
    QByteArray terminalSplitterState() const;
    void setTerminalSplitterState(const QByteArray &state);
    bool terminalExpanded() const;          // whether panel was visible
    void setTerminalExpanded(bool expanded);

    // --- Side terminal ---
    bool sideTerminalVisible() const;       // whether the side terminal was visible
    void setSideTerminalVisible(bool visible);
    // Width of a single side terminal split in pixels (the panel runs one per
    // configured side command), 0 when nothing was saved yet.
    int sideTerminalSplitWidth() const;
    void setSideTerminalSplitWidth(int width);

    // --- Recently opened files/folders (project switcher) ---
    // Most-recent-first, deduplicated, capped at maxEntries on insert.
    QStringList recentFiles() const;
    void addRecentFile(const QString &path, int maxEntries);
    QStringList recentFolders() const;
    void addRecentFolder(const QString &path, int maxEntries);

    /// Flush pending writes to disk.
    void sync();

private:
    QSettings *m_settings = nullptr;
};
