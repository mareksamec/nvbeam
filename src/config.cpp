#include "config.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <toml.hpp>

QString Config::configPath()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return configDir + "/neobeam.toml";
}

Config Config::load(const QString &customPath)
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
                "# height = 300  # startup height in pixels\n"
                "# font = \"\"  # terminal font (empty = use global font)\n"
                "# font_size = 0  # terminal font size (0 = use global)\n"
                "# color_scheme = \"Linux\"  # QTermWidget color scheme\n"
                "# shell = \"\"  # shell program (empty = system default)\n"
                "# shell_args = []  # arguments passed to shell\n"
                "# exec_command = \"\"  # command to run after shell starts (e.g. \"tmux\")\n"
                "# right_click_paste = false  # PuTTY-style right-click paste\n"
                "# antialiasing = true  # font antialiasing in terminal\n"
                "# hinting = \"default\"  # font hinting: none, slight, medium, full, default\n"
                "\n"
                "# [ui]\n"
                "# ext_cmdline = true\n"
                "# ext_popupmenu = true\n"
                "# ext_tabline = true\n"
                "# ext_messages = true\n"
                "# bufferline = true  # show open buffers instead of tabpages\n"
                "# statusline = true  # native statusline (mode, file, position, diagnostics)\n"
                "# accent_color = \"#50fa7b\"\n"
            );
            file.close();
            qDebug() << "Created default config at:" << path;
        }
        return cfg;
    }

    try {
        auto data = toml::parse(path.toStdString());

        if (data.contains("font")) {
            auto &font = data.at("font");
            if (font.contains("family")) {
                cfg.fontFamily = QString::fromStdString(toml::find<std::string>(font, "family"));
            }
            if (font.contains("size")) {
                cfg.fontSize = toml::find<int>(font, "size");
            }
            if (font.contains("hinting")) {
                auto h = toml::find<std::string>(font, "hinting");
                if (h == "none") cfg.hinting = QFont::PreferNoHinting;
                else if (h == "slight") cfg.hinting = QFont::PreferVerticalHinting;
                else if (h == "full") cfg.hinting = QFont::PreferFullHinting;
                else cfg.hinting = QFont::PreferDefaultHinting;
            }
            if (font.contains("antialiasing")) {
                cfg.antialiasing = toml::find<bool>(font, "antialiasing");
            }
        }

        if (data.contains("terminal")) {
            auto &term = data.at("terminal");
            if (term.contains("follow_cwd")) {
                cfg.terminalFollowCwd = toml::find<bool>(term, "follow_cwd");
            }
            if (term.contains("height"))
                cfg.terminalHeight = toml::find<int>(term, "height");
            if (term.contains("font"))
                cfg.terminalFont = QString::fromStdString(toml::find<std::string>(term, "font"));
            if (term.contains("font_size"))
                cfg.terminalFontSize = toml::find<int>(term, "font_size");
            if (term.contains("color_scheme"))
                cfg.terminalColorScheme = QString::fromStdString(toml::find<std::string>(term, "color_scheme"));
            if (term.contains("shell"))
                cfg.terminalShell = QString::fromStdString(toml::find<std::string>(term, "shell"));
            if (term.contains("shell_args")) {
                auto args = toml::find<std::vector<std::string>>(term, "shell_args");
                for (const auto &a : args)
                    cfg.terminalShellArgs.append(QString::fromStdString(a));
            }
            if (term.contains("exec_command"))
                cfg.terminalExecCommand = QString::fromStdString(toml::find<std::string>(term, "exec_command"));
            if (term.contains("right_click_paste"))
                cfg.terminalRightClickPaste = toml::find<bool>(term, "right_click_paste");
            if (term.contains("antialiasing"))
                cfg.terminalAntialiasing = toml::find<bool>(term, "antialiasing");
            if (term.contains("hinting")) {
                auto h = toml::find<std::string>(term, "hinting");
                if (h == "none") cfg.terminalHinting = QFont::PreferNoHinting;
                else if (h == "slight") cfg.terminalHinting = QFont::PreferVerticalHinting;
                else if (h == "full") cfg.terminalHinting = QFont::PreferFullHinting;
                else cfg.terminalHinting = QFont::PreferDefaultHinting;
            }
        }

        if (data.contains("ui")) {
            auto &ui = data.at("ui");
            if (ui.contains("ext_cmdline"))
                cfg.extCmdline = toml::find<bool>(ui, "ext_cmdline");
            if (ui.contains("ext_popupmenu"))
                cfg.extPopupmenu = toml::find<bool>(ui, "ext_popupmenu");
            if (ui.contains("ext_tabline"))
                cfg.extTabline = toml::find<bool>(ui, "ext_tabline");
            if (ui.contains("ext_messages"))
                cfg.extMessages = toml::find<bool>(ui, "ext_messages");
            if (ui.contains("accent_color"))
                cfg.accentColor = QString::fromStdString(toml::find<std::string>(ui, "accent_color"));
            if (ui.contains("bufferline"))
                cfg.bufferline = toml::find<bool>(ui, "bufferline");
            if (ui.contains("statusline"))
                cfg.extStatusline = toml::find<bool>(ui, "statusline");
        }
    } catch (const std::exception &e) {
        qWarning() << "Failed to parse config:" << e.what();
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
