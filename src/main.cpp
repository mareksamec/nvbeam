#include <QApplication>
#include <QCommandLineParser>
#include "mainwindow.h"
#include "config.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("neobeam");
    app.setApplicationVersion("0.1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Neovim GUI client");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption fontOption({"f", "font"}, "Font family to use", "family");
    QCommandLineOption fontSizeOption("font-size", "Font size in points", "size");
    parser.addOption(fontOption);
    parser.addOption(fontSizeOption);

    // Parse known options, rest goes to nvim
    parser.parse(app.arguments());

    Config cfg = Config::load();

    if (parser.isSet(fontOption)) {
        cfg.fontFamily = parser.value(fontOption);
    }
    if (parser.isSet(fontSizeOption)) {
        cfg.fontSize = parser.value(fontSizeOption).toInt();
    }

    // Collect unknown args to pass to nvim
    QStringList nvimArgs = parser.unknownOptionNames();
    // Also pass positional args and raw unknown args
    QStringList allArgs = app.arguments().mid(1);
    QStringList knownFlags = {"-f", "--font", "--font-size", "-h", "--help", "-v", "--version"};
    QStringList filteredArgs;
    for (int i = 0; i < allArgs.size(); i++) {
        QString arg = allArgs[i];
        bool skip = false;
        for (const auto &flag : knownFlags) {
            if (arg == flag) {
                skip = true;
                // Skip the value too if it's a value-taking option
                if (arg == "-f" || arg == "--font" || arg == "--font-size") {
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
