#include "colorscheme.h"
#include "theme.h"

#include <QTest>

/// The built-in chrome themes: complete, readable, and each paired with a
/// terminal colour scheme nvbeam ships.
class TestTheme : public QObject
{
    Q_OBJECT

private slots:
    void theDefaultThemeExists()
    {
        QVERIFY(findTheme(defaultThemeName()));
        QCOMPARE(themeNames().first(), defaultThemeName());
        QVERIFY(!findTheme(QStringLiteral("no-such-theme")));
    }

    void everyRoleIsSet_data()
    {
        QTest::addColumn<QString>("name");
        for (const QString &name : themeNames())
            QTest::newRow(qPrintable(name)) << name;
    }

    void everyRoleIsSet()
    {
        QFETCH(QString, name);
        const Theme *theme = findTheme(name);
        QVERIFY(theme);
        QCOMPARE(theme->name, name);
        const QColor roles[] = {
            theme->background, theme->surface, theme->selection, theme->border,
            theme->text, theme->subtext, theme->dim, theme->accent, theme->focus,
            theme->mouseSelection, theme->error, theme->warning, theme->modeInsert,
            theme->modeVisual, theme->modeCommand, theme->gitChanged, theme->gitUntracked};
        for (const QColor &role : roles)
            QVERIFY(role.isValid());
    }

    /// Text has to stand out from the background it is drawn on: plain text
    /// clearly, dimmed text (hints, timestamps) at least visibly.
    void textIsReadable_data() { everyRoleIsSet_data(); }
    void textIsReadable()
    {
        QFETCH(QString, name);
        const Theme *theme = findTheme(name);
        const auto contrast = [](const QColor &a, const QColor &b) {
            return qAbs(qGray(a.rgb()) - qGray(b.rgb()));
        };
        QVERIFY2(contrast(theme->text, theme->background) >= 100, qPrintable(name));
        QVERIFY2(contrast(theme->text, theme->surface) >= 100, qPrintable(name));
        QVERIFY2(contrast(theme->subtext, theme->background) >= 60, qPrintable(name));
        QVERIFY2(contrast(theme->dim, theme->background) >= 50, qPrintable(name));
    }

    void shipsItsTerminalScheme_data() { everyRoleIsSet_data(); }
    void shipsItsTerminalScheme()
    {
        QFETCH(QString, name);
        const Theme *theme = findTheme(name);
        const TerminalScheme scheme = loadTerminalScheme(
            QStringLiteral(NVBEAM_SOURCE_DIR "/resources/color-schemes/%1.colorscheme")
                .arg(theme->terminalScheme));
        QVERIFY2(scheme.valid, qPrintable(theme->terminalScheme));
        QVERIFY(scheme.background.isValid());
        QVERIFY(scheme.foreground.isValid());
        for (int i = 0; i < 8; ++i) {
            QVERIFY(scheme.ansi[i].isValid());
            QVERIFY(scheme.ansiIntense[i].isValid());
        }
    }
};

QTEST_MAIN(TestTheme)
#include "tst_theme.moc"
