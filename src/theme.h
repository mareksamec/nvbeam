#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

/// The colours of nvbeam's own chrome: the statusline, bufferline, command
/// line, message log, popups, switchers and panel buttons. Each role names
/// what a colour is for, so a theme is one palette in one place and a widget
/// asks for "border" rather than a hex value.
///
/// The nvim buffer keeps the user's nvim colourscheme, and the terminals and
/// the change review panel follow the terminal colour scheme, which a theme
/// only suggests (terminalScheme).
struct Theme {
    QString name;

    QColor background;      // bars, popups, message log
    QColor surface;         // input fields, raised statusline segments
    QColor selection;       // selected or hovered row, menu item
    QColor border;          // frames, separators, splitter handles
    QColor text;            // main text
    QColor subtext;         // list rows, inactive tabs, panel buttons
    QColor dim;             // hints, timestamps, disabled items, dividers

    QColor accent;          // [ui] accent_color default: Normal mode, highlights
    QColor focus;           // [ui] focus_border_color default
    QColor mouseSelection;  // [mouse] selection_color default
    QColor error;           // error messages, diagnostics, Replace mode
    QColor warning;         // warning messages

    QColor modeInsert;
    QColor modeVisual;
    QColor modeCommand;

    QColor gitChanged;      // staged or modified files in the git tooltip
    QColor gitUntracked;    // untracked files in the git tooltip

    QString terminalScheme; // terminal colour scheme used when none is set
};

/// The name of the theme nvbeam uses when [ui] theme is not set.
QString defaultThemeName();

/// Names of the built-in themes, default first.
QStringList themeNames();

/// The built-in theme called `name`, or nullptr when there is none.
const Theme *findTheme(const QString &name);
