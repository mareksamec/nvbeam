#include "terminalkeys.h"

#include <QTest>

/// lineInputEffect() decides whether a terminal split has unsubmitted input on
/// its current line, which is what gates the prompt refresh after a branch
/// switch (TerminalPanel::refreshPromptOnBranchChange()).
class TestTerminalKeys : public QObject
{
    Q_OBJECT

private slots:
    void modifiersAloneSayNothingAboutTheLine();
    void enterClearsTheLine();
    void ctrlCAndCtrlUClearTheLine();
    void plainCAndUType();
    void ordinaryKeysDirtyTheLine();
};

void TestTerminalKeys::modifiersAloneSayNothingAboutTheLine()
{
    // The regression this function exists for: Qt delivers a KeyPress for the
    // modifier itself, so Ctrl+Shift+B (branch switcher) or Ctrl+Tab (leave
    // the terminal) pressed with the terminal focused used to mark the split
    // dirty, and the prompt was then never refreshed again.
    for (int key : {Qt::Key_Control, Qt::Key_Shift, Qt::Key_Alt, Qt::Key_AltGr,
                    Qt::Key_Meta, Qt::Key_Super_L, Qt::Key_CapsLock,
                    Qt::Key_NumLock, Qt::Key_unknown}) {
        QCOMPARE(lineInputEffect(key, Qt::NoModifier), LineInputEffect::Ignore);
        QCOMPARE(lineInputEffect(key, Qt::ControlModifier | Qt::ShiftModifier),
                 LineInputEffect::Ignore);
    }
}

void TestTerminalKeys::enterClearsTheLine()
{
    QCOMPARE(lineInputEffect(Qt::Key_Return, Qt::NoModifier), LineInputEffect::Clears);
    QCOMPARE(lineInputEffect(Qt::Key_Enter, Qt::KeypadModifier), LineInputEffect::Clears);
}

void TestTerminalKeys::ctrlCAndCtrlUClearTheLine()
{
    QCOMPARE(lineInputEffect(Qt::Key_C, Qt::ControlModifier), LineInputEffect::Clears);
    QCOMPARE(lineInputEffect(Qt::Key_U, Qt::ControlModifier), LineInputEffect::Clears);
}

void TestTerminalKeys::plainCAndUType()
{
    QCOMPARE(lineInputEffect(Qt::Key_C, Qt::NoModifier), LineInputEffect::Dirties);
    QCOMPARE(lineInputEffect(Qt::Key_U, Qt::ShiftModifier), LineInputEffect::Dirties);
}

void TestTerminalKeys::ordinaryKeysDirtyTheLine()
{
    // Backspace included: conservative by design, an empty line that was typed
    // into and erased still reads as dirty.
    for (int key : {Qt::Key_A, Qt::Key_Space, Qt::Key_Backspace, Qt::Key_Up,
                    Qt::Key_Tab, Qt::Key_1})
        QCOMPARE(lineInputEffect(key, Qt::NoModifier), LineInputEffect::Dirties);
}

QTEST_GUILESS_MAIN(TestTerminalKeys)
#include "tst_terminalkeys.moc"
