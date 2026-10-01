#include <QApplication>
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QFile>
#include <QStringList>
#include <cstdio>
#include "mainwindow.h"
#include "config.h"

// Validate the config file and print findings. Exit codes:
// 0 = clean, 1 = warnings only, 2 = errors (unparsable or missing file).
static int checkConfig(const QString &customPath)
{
    const QString path = customPath.isEmpty() ? Config::configPath() : customPath;
    if (!QFile::exists(path)) {
        fprintf(stderr, "nvbeam: config file not found: %s\n", qPrintable(path));
        return 2;
    }

    QList<ConfigIssue> issues;
    Config::load(path, &issues);

    if (issues.isEmpty()) {
        printf("%s: OK\n", qPrintable(path));
        return 0;
    }

    bool hasError = false;
    for (const auto &issue : issues) {
        const bool err = issue.severity == ConfigIssue::Error;
        hasError = hasError || err;
        fprintf(stderr, "%s: %s\n", err ? "error" : "warning", qPrintable(issue.message));
    }
    return hasError ? 2 : 1;
}

// The optional features this binary was built with, appended to --version.
// Both are compile-time: a package built without qtermwidget6 or
// KF6SyntaxHighlighting still runs, it just silently lacks the terminal panel
// or the code coloring in review diffs, and --version is where that shows.
static QString buildFeatures()
{
    QStringList features;
#ifdef NVBEAM_HAS_TERMINAL
    features << QStringLiteral("terminal");
#endif
#ifdef NVBEAM_HAS_SYNTAX
    features << QStringLiteral("syntax");
#endif
    return features.isEmpty() ? QStringLiteral("none") : features.join(QLatin1Char(' '));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("nvbeam");
    app.setApplicationVersion(QStringLiteral(NVBEAM_VERSION " (built with: %1)")
                                  .arg(buildFeatures()));

    // Application icon. The build rasterizes an SVG master into a PNG size
    // ladder and embeds it as a Qt resource (no runtime Qt6::Svg dependency).
    // We load every available size into one QIcon so Qt can pick the best match
    // per request — crisp at small sizes and on HiDPI displays alike.
#ifdef NVBEAM_ICON_SIZES
    {
        QIcon icon;
        const int sizes[] = { NVBEAM_ICON_SIZES };
        for (int s : sizes)
            icon.addFile(QStringLiteral(":/icons/nvbeam_%1.png").arg(s),
                         QSize(s, s));
        if (!icon.isNull())
            app.setWindowIcon(icon);
    }
#endif
    // Tie the window to its .desktop entry so Wayland/X11 group the window
    // under the correct launcher icon (matches StartupWMClass).
    QGuiApplication::setDesktopFileName("nvbeam");

    QCommandLineParser parser;
    parser.setApplicationDescription("Neovim GUI client");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption fontOption({"f", "font"}, "Font family to use", "family");
    QCommandLineOption fontSizeOption("font-size", "Font size in points", "size");
    QCommandLineOption nativeUiOption("native-ui", "Enable all native UI elements");
    QCommandLineOption configOption({"c", "config"}, "Path to config file", "path");
    QCommandLineOption checkOption({"k", "check-config"}, "Validate the config file and exit");
    parser.addOption(fontOption);
    parser.addOption(fontSizeOption);
    parser.addOption(nativeUiOption);
    parser.addOption(configOption);
    parser.addOption(checkOption);

    // Parse known options, rest goes to nvim
    parser.parse(app.arguments());

    // parse() leaves unknown arguments for nvim, but unlike process() it also
    // leaves -h/--help and -v/--version unhandled, so act on them here. Both
    // print and exit.
    if (parser.isSet(QStringLiteral("help")))
        parser.showHelp(0);
    if (parser.isSet(QStringLiteral("version")))
        parser.showVersion();

    if (parser.isSet(checkOption))
        return checkConfig(parser.value(configOption));

    Config cfg = Config::load(parser.value(configOption));

    if (parser.isSet(fontOption)) {
        cfg.fontFamily = parser.value(fontOption);
    }
    if (parser.isSet(fontSizeOption)) {
        cfg.fontSize = parser.value(fontSizeOption).toInt();
    }
    if (parser.isSet(nativeUiOption)) {
        cfg.extPopupmenu = true;
        cfg.extTabline = true;
        cfg.extMessages = true;
    }

    // Collect unknown args to pass to nvim
    QStringList nvimArgs = parser.unknownOptionNames();
    // Also pass positional args and raw unknown args
    QStringList allArgs = app.arguments().mid(1);
    QStringList knownFlags = {"-f", "--font", "--font-size", "--native-ui", "-c", "--config", "-k", "--check-config", "-h", "--help", "-v", "--version"};
    QStringList filteredArgs;
    for (int i = 0; i < allArgs.size(); i++) {
        QString arg = allArgs[i];
        bool skip = false;
        for (const auto &flag : knownFlags) {
            if (arg == flag) {
                skip = true;
                // Skip the value too if it's a value-taking option
                if (arg == "-f" || arg == "--font" || arg == "--font-size" || arg == "-c" || arg == "--config") {
                    i++; // skip next arg (the value)
                }
                break;
            }
            if (arg.startsWith(flag + "=")) {
                skip = true;
                break;
            }
        }
        if (!skip) {
            filteredArgs << arg;
        }
    }

    MainWindow window(cfg, filteredArgs);
    window.show();

    return app.exec();
}
