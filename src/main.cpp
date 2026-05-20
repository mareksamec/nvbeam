#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("neobeam");
    app.setApplicationVersion("0.1.0");

    // All args after the executable are passed to nvim
    QStringList nvimArgs;
    for (int i = 1; i < argc; i++) {
        nvimArgs << argv[i];
    }

    MainWindow window(nvimArgs);
    window.show();

    return app.exec();
}
