#include "theme.h"

#include <QList>

namespace {

/// The built-in themes. Each light theme takes its colours from its upstream
/// project; the role each upstream colour fills is noted next to it.
QList<Theme> builtInThemes()
{
    QList<Theme> themes;

    // The original nvbeam chrome: Tokyo Night surfaces with OneDark and
    // Dracula accents.
    Theme dark;
    dark.name = QStringLiteral("dark");
    dark.background = QColor("#1a1b26");
    dark.surface = QColor("#24283b");
    dark.selection = QColor("#313244");
    dark.border = QColor("#333333");
    dark.text = QColor("#c0caf5");
    dark.subtext = QColor("#a9b1d6");
    dark.dim = QColor("#565f89");
    dark.accent = QColor("#50fa7b");
    dark.focus = QColor("#61afef");
    dark.mouseSelection = QColor("#44475a");
    dark.error = QColor("#f7768e");
    dark.warning = QColor("#e0af68");
    dark.modeInsert = QColor("#c678dd");
    dark.modeVisual = QColor("#56b6c2");
    dark.modeCommand = QColor("#e5c07b");
    dark.gitChanged = QColor("#98c379");
    dark.gitUntracked = QColor("#61afef");
    dark.terminalScheme = QStringLiteral("Neoblue");
    themes << dark;

    // Catppuccin Latte, palette v1.8.0
    // (github.com/catppuccin/palette, palette.json, commit 07d02aa110).
    Theme latte;
    latte.name = QStringLiteral("catppuccin-latte");
    latte.background = QColor("#e6e9ef");      // mantle
    latte.surface = QColor("#ccd0da");         // surface0
    latte.selection = QColor("#bcc0cc");       // surface1
    latte.border = QColor("#acb0be");          // surface2
    latte.text = QColor("#4c4f69");            // text
    latte.subtext = QColor("#6c6f85");         // subtext0
    latte.dim = QColor("#8c8fa1");             // overlay1
    latte.accent = QColor("#40a02b");          // green
    latte.focus = QColor("#1e66f5");           // blue
    latte.mouseSelection = QColor("#bcc0cc");  // surface1
    latte.error = QColor("#d20f39");           // red
    latte.warning = QColor("#df8e1d");         // yellow
    latte.modeInsert = QColor("#8839ef");      // mauve
    latte.modeVisual = QColor("#179299");      // teal
    latte.modeCommand = QColor("#fe640b");     // peach
    latte.gitChanged = QColor("#40a02b");      // green
    latte.gitUntracked = QColor("#1e66f5");    // blue
    latte.terminalScheme = QStringLiteral("CatppuccinLatte");
    themes << latte;

    // vim-paper (github.com/yorickpeterse/vim-paper, colors/paper.vim,
    // commit 1a998b3818). Roles follow the plugin's own highlight groups:
    // StatusLine and Pmenu on lgrey1, PmenuSel and VertSplit on lgrey2.
    Theme paper;
    paper.name = QStringLiteral("vim-paper");
    paper.background = QColor("#f2eede");      // background
    paper.surface = QColor("#d8d5c7");         // lgrey1
    paper.selection = QColor("#bfbcaf");       // lgrey2
    paper.border = QColor("#bfbcaf");          // lgrey2
    paper.text = QColor("#000000");            // black
    paper.subtext = QColor("#555555");         // dgrey
    paper.dim = QColor("#777777");             // grey
    paper.accent = QColor("#216609");          // green
    paper.focus = QColor("#1e6fcc");           // blue
    paper.mouseSelection = QColor("#f2de91");  // lyellow
    paper.error = QColor("#cc3e28");           // red
    paper.warning = QColor("#a55000");         // orange
    paper.modeInsert = QColor("#5c21a5");      // purple
    paper.modeVisual = QColor("#158c86");      // cyan
    paper.modeCommand = QColor("#b58900");     // yellow
    paper.gitChanged = QColor("#216609");      // green
    paper.gitUntracked = QColor("#1e6fcc");    // blue
    paper.terminalScheme = QStringLiteral("VimPaper");
    themes << paper;

    return themes;
}

const QList<Theme> &themes()
{
    static const QList<Theme> all = builtInThemes();
    return all;
}

} // namespace

QString defaultThemeName()
{
    return QStringLiteral("dark");
}

QStringList themeNames()
{
    QStringList names;
    for (const Theme &theme : themes())
        names << theme.name;
    return names;
}

const Theme *findTheme(const QString &name)
{
    for (const Theme &theme : themes()) {
        if (theme.name == name)
            return &theme;
    }
    return nullptr;
}
