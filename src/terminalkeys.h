#pragma once

#include <Qt>

/// What a key press in a terminal split means for the "is there unsubmitted
/// input on the current line" heuristic that gates the prompt refresh after a
/// branch switch (TerminalPanel::refreshPromptOnBranchChange()).
///
/// There is no public API to read the shell's own input buffer, so keystrokes
/// are tracked instead. Pure and header-only so the unit tests can exercise it
/// without a terminal widget.
enum class LineInputEffect {
    Ignore,   ///< Says nothing about the line: modifiers, focus/lock keys.
    Clears,   ///< The line was submitted, aborted or cleared.
    Dirties,  ///< Something was typed into the line.
};

/// Classify one key press. Conservative towards Dirties - e.g. a Backspace
/// back to an empty line still reads as dirty - since the cost of skipping a
/// prompt refresh is far lower than injecting Enter into a line the user is
/// mid-typing.
///
/// Modifier keys are Ignore rather than Dirties: Qt delivers a KeyPress for
/// the modifier itself before the combination completes, so pressing
/// Ctrl+Shift+B (open the branch switcher) or Ctrl+Tab (leave the terminal)
/// while the terminal has focus would otherwise mark the split dirty forever,
/// even though nothing was typed into the shell.
inline LineInputEffect lineInputEffect(int key, Qt::KeyboardModifiers mods)
{
    switch (key) {
    case Qt::Key_Control:
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Meta:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
    case Qt::Key_unknown:
        return LineInputEffect::Ignore;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return LineInputEffect::Clears;
    case Qt::Key_C:
    case Qt::Key_U:
        // Ctrl+C aborts the line, Ctrl+U clears it; the plain letters type it.
        return mods & Qt::ControlModifier ? LineInputEffect::Clears
                                          : LineInputEffect::Dirties;
    default:
        return LineInputEffect::Dirties;
    }
}
