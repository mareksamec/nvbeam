#include "config.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QColor>
#include <toml.hpp>

namespace {

void report(QList<ConfigIssue> *issues, ConfigIssue::Severity sev, const QString &msg)
{
    if (issues)
        issues->append({sev, msg});
    else
        qWarning().noquote() << (sev == ConfigIssue::Error ? "Config error:" : "Config warning:") << msg;
}

const char *typeName(const toml::value &v)
{
    switch (v.type()) {
    case toml::value_t::boolean:  return "boolean";
    case toml::value_t::integer:  return "integer";
    case toml::value_t::floating: return "float";
    case toml::value_t::string:   return "string";
    case toml::value_t::array:    return "array";
    case toml::value_t::table:    return "table";
    default:                      return "value";
    }
}

void warnType(QList<ConfigIssue> *issues, const char *sec, const char *key,
              const char *expected, const toml::value &got)
{
    report(issues, ConfigIssue::Warning,
           QStringLiteral("[%1] %2: expected %3, got %4 (using default)")
               .arg(QLatin1String(sec), QLatin1String(key), QLatin1String(expected),
                    QLatin1String(typeName(got))));
}

bool readBool(const toml::value &sec, const char *secName, const char *key,
              bool &out, QList<ConfigIssue> *issues)
{
    if (!sec.contains(key))
        return false;
    const auto &v = sec.at(key);
    if (!v.is_boolean()) {
        warnType(issues, secName, key, "boolean", v);
        return false;
    }
    out = v.as_boolean();
    return true;
}

bool readInt(const toml::value &sec, const char *secName, const char *key,
             int &out, QList<ConfigIssue> *issues)
{
    if (!sec.contains(key))
        return false;
    const auto &v = sec.at(key);
    if (!v.is_integer()) {
        warnType(issues, secName, key, "integer", v);
        return false;
    }
    out = static_cast<int>(v.as_integer());
    return true;
}

bool readString(const toml::value &sec, const char *secName, const char *key,
                QString &out, QList<ConfigIssue> *issues)
{
    if (!sec.contains(key))
        return false;
    const auto &v = sec.at(key);
    if (!v.is_string()) {
        warnType(issues, secName, key, "string", v);
        return false;
    }
    out = QString::fromStdString(v.as_string());
    return true;
}

void readStringList(const toml::value &sec, const char *secName, const char *key,
                    QStringList &out, QList<ConfigIssue> *issues)
{
    if (!sec.contains(key))
        return;
    const auto &v = sec.at(key);
    if (!v.is_array()) {
        warnType(issues, secName, key, "array of strings", v);
        return;
    }
    QStringList result;
    for (const auto &el : v.as_array()) {
        if (!el.is_string()) {
            warnType(issues, secName, key, "array of strings", el);
            return;
        }
        result.append(QString::fromStdString(el.as_string()));
    }
    out = result;
}

void readColor(const toml::value &sec, const char *secName, const char *key,
               QString &out, QList<ConfigIssue> *issues)
{
    QString c;
    if (!readString(sec, secName, key, c, issues))
        return;
    if (!c.isEmpty() && !QColor(c).isValid()) {
        report(issues, ConfigIssue::Warning,
               QStringLiteral("[%1] %2: \"%3\" is not a valid color (using default)")
                   .arg(QLatin1String(secName), QLatin1String(key), c));
        return;
    }
    out = c;
}

void readHinting(const toml::value &sec, const char *secName, const char *key,
                 QFont::HintingPreference &out, QList<ConfigIssue> *issues)
{
    QString h;
    if (!readString(sec, secName, key, h, issues))
        return;
    if (h == "none") out = QFont::PreferNoHinting;
    else if (h == "slight") out = QFont::PreferVerticalHinting;
    else if (h == "full") out = QFont::PreferFullHinting;
    else if (h == "medium" || h == "default") out = QFont::PreferDefaultHinting;
    else {
        out = QFont::PreferDefaultHinting;
        report(issues, ConfigIssue::Warning,
               QStringLiteral("[%1] %2: unknown value \"%3\" (expected none, slight, medium, full or default)")
                   .arg(QLatin1String(secName), QLatin1String(key), h));
    }
}

struct KnownSection {
    const char *name;
    std::initializer_list<const char *> keys;
};

const KnownSection kKnownSections[] = {
    {"font", {"family", "size", "hinting", "antialiasing"}},
    {"terminal", {"follow_cwd", "start_expanded", "height", "font", "font_size",
                  "color_scheme", "foreground", "background", "shell", "shell_args",
                  "exec_command", "right_click_paste", "copy_on_select",
                  "antialiasing", "hinting", "scrollback"}},
    {"side_terminal", {"width", "font", "font_size"}},
    {"ui", {"ext_popupmenu", "ext_tabline", "ext_messages", "bufferline", "statusline",
            "git_status", "accent_color", "statusline_divider", "focus_border_color",
            "window_title"}},
    {"mouse", {"terminal_selection", "auto_copy", "selection_color"}},
    {"session", {"restore_window", "restore_terminal"}},
    {"switcher", {"keep_running", "max_recent"}},
};

void warnUnknownKeys(const toml::value &data, QList<ConfigIssue> *issues)
{
    for (const auto &kv : data.as_table()) {
        const QString name = QString::fromStdString(kv.first);
        // [[projects]] is an array of tables, validated in readProjects().
        if (name == QLatin1String("projects"))
            continue;
        // Moved option: point at the replacement instead of a generic warning.
        if (name == QLatin1String("side_terminal") && kv.second.is_table()
            && kv.second.contains("command")) {
            report(issues, ConfigIssue::Warning,
                   QStringLiteral("[side_terminal] command was removed; set side_command "
                                  "on a [[projects]] entry instead"));
        }
        const KnownSection *known = nullptr;
        for (const auto &s : kKnownSections) {
            if (name == QLatin1String(s.name)) {
                known = &s;
                break;
            }
        }
        if (!known) {
            if (kv.second.is_table())
                report(issues, ConfigIssue::Warning,
                       QStringLiteral("unknown section [%1]").arg(name));
            else
                report(issues, ConfigIssue::Warning,
                       QStringLiteral("unknown top-level option \"%1\"").arg(name));
            continue;
        }
        if (!kv.second.is_table()) {
            report(issues, ConfigIssue::Warning,
                   QStringLiteral("%1: expected a [%1] section table").arg(name));
            continue;
        }
        for (const auto &entry : kv.second.as_table()) {
            // Already reported above with a pointer to the replacement.
            if (name == QLatin1String("side_terminal") && entry.first == "command")
                continue;
            bool found = false;
            for (const char *k : known->keys) {
                if (entry.first == k) {
                    found = true;
                    break;
                }
            }
            if (!found)
                report(issues, ConfigIssue::Warning,
                       QStringLiteral("[%1] unknown option \"%2\"")
                           .arg(name, QString::fromStdString(entry.first)));
        }
    }
}

// Section accessor that tolerates a non-table value (warned by warnUnknownKeys)
const toml::value *sectionTable(const toml::value &data, const char *name)
{
    if (!data.contains(name))
        return nullptr;
    const auto &v = data.at(name);
    return v.is_table() ? &v : nullptr;
}

QString expandTilde(const QString &path)
{
    if (path == QLatin1String("~"))
        return QDir::homePath();
    if (path.startsWith(QLatin1String("~/")))
        return QDir::homePath() + path.mid(1);
    return path;
}

void readProjects(const toml::value &data, QList<ProjectEntry> &out,
                  QList<ConfigIssue> *issues)
{
    if (!data.contains("projects"))
        return;
    const auto &v = data.at("projects");
    if (!v.is_array()) {
        report(issues, ConfigIssue::Warning,
               QStringLiteral("projects: expected an array of tables ([[projects]] entries)"));
        return;
    }
    int index = 0;
    for (const auto &entry : v.as_array()) {
        ++index;
        const char *sec = "projects";
        if (!entry.is_table()) {
            report(issues, ConfigIssue::Warning,
                   QStringLiteral("[[projects]] entry %1: expected a table").arg(index));
            continue;
        }
        for (const auto &kv : entry.as_table()) {
            if (kv.first != "path" && kv.first != "name" && kv.first != "side_command")
                report(issues, ConfigIssue::Warning,
                       QStringLiteral("[[projects]] entry %1: unknown option \"%2\"")
                           .arg(index).arg(QString::fromStdString(kv.first)));
        }
        ProjectEntry project;
        if (!readString(entry, sec, "path", project.path, issues) || project.path.isEmpty()) {
            report(issues, ConfigIssue::Warning,
                   QStringLiteral("[[projects]] entry %1: missing required \"path\" (entry ignored)")
                       .arg(index));
            continue;
        }
        project.path = QDir::cleanPath(expandTilde(project.path));
        if (!QDir(project.path).exists())
            report(issues, ConfigIssue::Warning,
                   QStringLiteral("[[projects]] entry %1: path does not exist: %2")
                       .arg(index).arg(project.path));
        readString(entry, sec, "name", project.name, issues);
        readString(entry, sec, "side_command", project.sideCommand, issues);
        if (project.name.isEmpty())
            project.name = QFileInfo(project.path).fileName();
        out.append(project);
    }
}

} // namespace

QString Config::configPath()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return configDir + "/neobeam.toml";
}

Config Config::load(const QString &customPath, QList<ConfigIssue> *issues)
{
    Config cfg;
    QString path = customPath.isEmpty() ? configPath() : customPath;

    if (!QFile::exists(path)) {
        // Write default config as commented-out reference
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(
                "# Neobeam configuration\n"
                "\n"
                "# [font]\n"
                "# family = \"Monospace\"\n"
                "# size = 10\n"
                "# hinting = \"default\"  # none, slight, medium, full, default\n"
                "# antialiasing = true\n"
                "\n"
                "# [terminal]\n"
                "# follow_cwd = true  # terminal follows nvim working directory\n"
                "# start_expanded = false  # open with the terminal panel visible\n"
                "# height = 300  # startup height in pixels\n"
                "# font = \"\"  # terminal font (empty = use global font)\n"
                "# font_size = 0  # terminal font size (0 = use global)\n"
                "# color_scheme = \"Neoblue\"  # QTermWidget color scheme (Neoblue, OneDark, Dracula ship with neobeam)\n"
                "# foreground = \"\"  # override scheme text color, e.g. \"#f8f8f2\" (empty = scheme default)\n"
                "# background = \"\"  # override scheme background color (empty = scheme default)\n"
                "# shell = \"\"  # shell program (empty = system default)\n"
                "# shell_args = []  # arguments passed to shell\n"
                "# exec_command = \"\"  # command to run after shell starts (e.g. \"tmux\")\n"
                "# right_click_paste = false  # PuTTY-style right-click paste\n"
                "# copy_on_select = true  # copy selection to PRIMARY + CLIPBOARD as it is made\n"
                "# antialiasing = true  # font antialiasing in terminal\n"
                "# hinting = \"default\"  # font hinting: none, slight, medium, full, default\n"
                "# scrollback = 1000  # scrollback buffer size in lines\n"
                "\n"
                "# [side_terminal]\n"
                "# width = 400  # startup width in pixels\n"
                "# font = \"\"  # side terminal font (empty = use terminal font)\n"
                "# font_size = 0  # side terminal font size (0 = use terminal font size)\n"
                "\n"
                "# [ui]\n"
                "# ext_popupmenu = true\n"
                "# ext_tabline = true\n"
                "# ext_messages = true\n"
                "# bufferline = true  # show open buffers instead of tabpages\n"
                "# statusline = true  # native statusline (mode, file, position, diagnostics)\n"
                "# git_status = true  # git branch + status counts in the statusline (push-updated, no polling)\n"
                "# accent_color = \"#50fa7b\"\n"
                "# statusline_divider = \"powerline\"  # powerline, pipe, none\n"
                "# focus_border_color = \"#61afef\"  # 1px border around the focused grid/terminal pane\n"
                "# window_title = \"\"  # custom title template (empty = default \"Neobeam - ~/path\",\n"
                "#                     # or \"<project> - ~/path\" after a switcher project pick).\n"
                "#                     # Placeholders: %p project name, %d cwd (~-shortened), %D cwd,\n"
                "#                     # %f file name, %F file path (~-shortened), %% literal %\n"
                "\n"
                "# [mouse]\n"
                "# terminal_selection = true   # terminal-style select/copy when vim mouse= is empty\n"
                "# auto_copy = true            # auto-copy selection to clipboard on release\n"
                "# selection_color = \"#44475a\" # selection highlight color\n"
                "\n"
                "# [session]\n"
                "# restore_window = true    # window size + editor/terminal proportions (WM/Wayland may override)\n"
                "# restore_terminal = true  # terminal split count, proportions, expanded state\n"
                "\n"
                "# [switcher]\n"
                "# keep_running = true  # show the project switcher instead of quitting when nvim exits\n"
                "# max_recent = 20      # recent files/folders kept in the switcher list (each)\n"
                "\n"
                "# Pinned projects, shown on top of the project switcher list.\n"
                "# Repeat the [[projects]] block once per project.\n"
                "# [[projects]]\n"
                "# path = \"~/work/myproject\"\n"
                "# name = \"myproject\"      # optional display name (default: folder name)\n"
                "# side_command = \"claude\" # optional command for the side terminal in this project\n"
                "#\n"
                "# [[projects]]\n"
                "# path = \"~/work/otherproject\"  # only path is required; side terminal runs a plain shell\n"
            );
            file.close();
            qDebug() << "Created default config at:" << path;
        }
        return cfg;
    }

    try {
        auto data = toml::parse(path.toStdString());

        warnUnknownKeys(data, issues);

        if (const auto *font = sectionTable(data, "font")) {
            readString(*font, "font", "family", cfg.fontFamily, issues);
            readInt(*font, "font", "size", cfg.fontSize, issues);
            readHinting(*font, "font", "hinting", cfg.hinting, issues);
            readBool(*font, "font", "antialiasing", cfg.antialiasing, issues);
        }

        if (const auto *term = sectionTable(data, "terminal")) {
            readBool(*term, "terminal", "follow_cwd", cfg.terminalFollowCwd, issues);
            readBool(*term, "terminal", "start_expanded", cfg.terminalStartExpanded, issues);
            readInt(*term, "terminal", "height", cfg.terminalHeight, issues);
            readString(*term, "terminal", "font", cfg.terminalFont, issues);
            readInt(*term, "terminal", "font_size", cfg.terminalFontSize, issues);
            readString(*term, "terminal", "color_scheme", cfg.terminalColorScheme, issues);
            readColor(*term, "terminal", "foreground", cfg.terminalForeground, issues);
            readColor(*term, "terminal", "background", cfg.terminalBackground, issues);
            readString(*term, "terminal", "shell", cfg.terminalShell, issues);
            readStringList(*term, "terminal", "shell_args", cfg.terminalShellArgs, issues);
            readString(*term, "terminal", "exec_command", cfg.terminalExecCommand, issues);
            readBool(*term, "terminal", "right_click_paste", cfg.terminalRightClickPaste, issues);
            readBool(*term, "terminal", "copy_on_select", cfg.terminalCopyOnSelect, issues);
            readBool(*term, "terminal", "antialiasing", cfg.terminalAntialiasing, issues);
            readHinting(*term, "terminal", "hinting", cfg.terminalHinting, issues);
            if (readInt(*term, "terminal", "scrollback", cfg.terminalScrollback, issues)) {
                // Clamp to >= 1: the unlimited (-1) and disabled (0) history
                // modes are intentionally not exposed yet.
                cfg.terminalScrollback = qMax(1, cfg.terminalScrollback);
            }
        }

        if (const auto *side = sectionTable(data, "side_terminal")) {
            if (readInt(*side, "side_terminal", "width", cfg.sideTerminalWidth, issues))
                cfg.sideTerminalWidth = qMax(150, cfg.sideTerminalWidth);
            readString(*side, "side_terminal", "font", cfg.sideTerminalFont, issues);
            readInt(*side, "side_terminal", "font_size", cfg.sideTerminalFontSize, issues);
        }

        if (const auto *ui = sectionTable(data, "ui")) {
            readBool(*ui, "ui", "ext_popupmenu", cfg.extPopupmenu, issues);
            readBool(*ui, "ui", "ext_tabline", cfg.extTabline, issues);
            readBool(*ui, "ui", "ext_messages", cfg.extMessages, issues);
            readColor(*ui, "ui", "accent_color", cfg.accentColor, issues);
            readBool(*ui, "ui", "bufferline", cfg.bufferline, issues);
            readBool(*ui, "ui", "statusline", cfg.extStatusline, issues);
            readBool(*ui, "ui", "git_status", cfg.uiGitStatus, issues);
            QString divider;
            if (readString(*ui, "ui", "statusline_divider", divider, issues)) {
                if (divider == "powerline" || divider == "pipe" || divider == "none")
                    cfg.statuslineDivider = divider;
                else
                    report(issues, ConfigIssue::Warning,
                           QStringLiteral("[ui] statusline_divider: unknown value \"%1\" "
                                          "(expected powerline, pipe or none)").arg(divider));
            }
            readColor(*ui, "ui", "focus_border_color", cfg.focusBorderColor, issues);
            if (readString(*ui, "ui", "window_title", cfg.uiWindowTitle, issues)) {
                // Unknown placeholders render literally; flag them so a typo
                // like %P is caught by `neobeam -k` instead of shipping as-is.
                const QString &tpl = cfg.uiWindowTitle;
                for (int i = 0; i + 1 < tpl.size(); ++i) {
                    if (tpl.at(i) != QLatin1Char('%'))
                        continue;
                    const QChar token = tpl.at(++i);
                    if (!QStringLiteral("pdDfF%").contains(token))
                        report(issues, ConfigIssue::Warning,
                               QStringLiteral("[ui] window_title: unknown placeholder \"%%1\" "
                                              "(expected %p, %d, %D, %f, %F or %%)")
                                   .arg(token));
                }
            }
        }

        if (const auto *mouse = sectionTable(data, "mouse")) {
            readBool(*mouse, "mouse", "terminal_selection", cfg.mouseTerminalSelection, issues);
            readBool(*mouse, "mouse", "auto_copy", cfg.mouseAutoCopy, issues);
            readColor(*mouse, "mouse", "selection_color", cfg.mouseSelectionColor, issues);
        }

        if (const auto *session = sectionTable(data, "session")) {
            readBool(*session, "session", "restore_window", cfg.sessionRestoreWindow, issues);
            readBool(*session, "session", "restore_terminal", cfg.sessionRestoreTerminal, issues);
        }

        if (const auto *switcher = sectionTable(data, "switcher")) {
            readBool(*switcher, "switcher", "keep_running", cfg.switcherKeepRunning, issues);
            if (readInt(*switcher, "switcher", "max_recent", cfg.switcherMaxRecent, issues))
                cfg.switcherMaxRecent = qBound(0, cfg.switcherMaxRecent, 200);
        }

        readProjects(data, cfg.projects, issues);
    } catch (const std::exception &e) {
        report(issues, ConfigIssue::Error,
               QStringLiteral("failed to parse %1:\n%2").arg(path, QString::fromUtf8(e.what())));
    }

    return cfg;
}

QFont Config::font() const
{
    QFont f(fontFamily, fontSize);
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    f.setHintingPreference(hinting);
    if (!antialiasing) {
        f.setStyleStrategy(QFont::NoAntialias);
    }
    return f;
}
