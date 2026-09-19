#include "colorscheme.h"

#include <QCoreApplication>
#include <QFile>
#include <QSettings>

namespace {

/// One "r,g,b" entry. Invalid QColor when the group is absent or the values
/// are not three numbers in range.
QColor readColorGroup(QSettings &ini, const QString &group)
{
    const QStringList parts = ini.value(group + QStringLiteral("/Color")).toStringList();
    if (parts.size() != 3)
        return {};
    int rgb[3];
    for (int i = 0; i < 3; ++i) {
        bool ok = false;
        rgb[i] = parts[i].toInt(&ok);
        if (!ok || rgb[i] < 0 || rgb[i] > 255)
            return {};
    }
    return QColor(rgb[0], rgb[1], rgb[2]);
}

} // namespace

QStringList neobeamSchemeDirs()
{
    return {
        QCoreApplication::applicationDirPath() + "/../share/neobeam/color-schemes",
        QCoreApplication::applicationDirPath() + "/../resources/color-schemes",
        QCoreApplication::applicationDirPath() + "/color-schemes",
    };
}

QString findColorSchemeFile(const QString &scheme)
{
    if (scheme.contains(QLatin1Char('/')))
        return QFile::exists(scheme) ? scheme : QString();

    QStringList dirs = neobeamSchemeDirs();
    dirs << QStringLiteral("/usr/share/qtermwidget6/color-schemes")
         << QStringLiteral("/usr/local/share/qtermwidget6/color-schemes")
         << QStringLiteral("/usr/share/qtermwidget5/color-schemes");
    for (const QString &dir : dirs) {
        const QString path = dir + QLatin1Char('/') + scheme + QStringLiteral(".colorscheme");
        if (QFile::exists(path))
            return path;
    }
    return {};
}

TerminalScheme loadTerminalScheme(const QString &scheme)
{
    TerminalScheme out;
    const QString path = findColorSchemeFile(scheme);
    if (path.isEmpty())
        return out;

    QSettings ini(path, QSettings::IniFormat);
    out.valid = true;
    out.background = readColorGroup(ini, QStringLiteral("Background"));
    out.foreground = readColorGroup(ini, QStringLiteral("Foreground"));
    for (int i = 0; i < 8; ++i) {
        const QString base = QStringLiteral("Color%1").arg(i);
        out.ansi[i] = readColorGroup(ini, base);
        out.ansiIntense[i] = readColorGroup(ini, base + QStringLiteral("Intense"));
    }
    return out;
}
