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

Config Config::load()
{
    Config cfg;
    QString path = configPath();

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
                "# follow_cwd = true\n"
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
