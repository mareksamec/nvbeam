#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

/// The colors a QTermWidget ".colorscheme" file declares. Loaded by neobeam
/// itself rather than through QTermWidget, which has no getter for them, so
/// that native panels (the change review panel) can be drawn in the same
/// colors as the terminal even in a build without terminal support.
struct TerminalScheme {
    bool valid = false;        // false when the scheme file was not found or not readable
    QColor background;
    QColor foreground;
    QColor ansi[8];            // Color0..Color7: black, red, green, yellow, blue, magenta, cyan, white
    QColor ansiIntense[8];     // their Color<N>Intense counterparts
};

/// Index of each ANSI slot in TerminalScheme::ansi, so callers name a color
/// instead of a number.
enum AnsiColor {
    AnsiBlack = 0,
    AnsiRed,
    AnsiGreen,
    AnsiYellow,
    AnsiBlue,
    AnsiMagenta,
    AnsiCyan,
    AnsiWhite,
};

/// Directories neobeam ships its own schemes in, covering both the installed
/// layout (/usr/bin/neobeam -> /usr/share/neobeam/color-schemes) and the
/// source-tree layout used when running from the build directory.
QStringList neobeamSchemeDirs();

/// Locate the .colorscheme file for a scheme name across neobeam's own dirs
/// and the system qtermwidget dirs. A name containing a slash is taken as a
/// path and used as-is, the same contract QTermWidget::setColorScheme() has.
/// Returns an empty string if not found.
QString findColorSchemeFile(const QString &scheme);

/// A fraction of `over` mixed into `under`, for the shades a color scheme does
/// not declare (a hover background, a diff line's tint).
QColor blendColors(const QColor &under, const QColor &over, qreal fraction);

/// Read a scheme by name or path. The file is QSettings-compatible INI with
/// one group per color holding "r,g,b"; a group that is missing or malformed
/// leaves that color invalid rather than failing the whole load.
TerminalScheme loadTerminalScheme(const QString &scheme);
