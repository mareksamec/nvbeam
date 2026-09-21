#pragma once

#include <QKeySequence>
#include <QString>

/// The window's keyboard bindings, named once. MainWindow builds its
/// QShortcuts from these and the toolbars label their buttons with the same
/// values, so a binding and the hint that advertises it cannot drift apart.
///
/// Bindings that are not QShortcuts (Ctrl+Tab, Ctrl+Shift+B, the font-size
/// keys) are matched inside MainWindow::eventFilter and stay there: they need
/// the focused widget to decide, which a QKeySequence cannot express.
namespace Shortcuts {

inline QKeySequence toggleTerminal() { return QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft); }
inline QKeySequence toggleSideTerminal()
{
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_QuoteLeft);
}
inline QKeySequence splitTerminal() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E); }
inline QKeySequence closeTerminalSplit() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W); }
inline QKeySequence openSelectedLink() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L); }
inline QKeySequence broadcastCwd() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down); }
inline QKeySequence toggleSelectionMode()
{
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Period);
}
inline QKeySequence focusMessages() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M); }
inline QKeySequence maximizePane() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X); }
inline QKeySequence toggleChangeReview() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G); }
inline QKeySequence pasteInsert() { return QKeySequence(Qt::SHIFT | Qt::Key_Insert); }
inline QKeySequence pasteClipboard() { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V); }
inline QKeySequence toggleProjectSwitcher()
{
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P);
}

/// A button's hint with the binding that triggers the same action, in the
/// platform's own notation ("Ctrl+Shift+`").
inline QString hint(const QString &text, const QKeySequence &keys)
{
    return QStringLiteral("%1 (%2)").arg(text, keys.toString(QKeySequence::NativeText));
}

} // namespace Shortcuts
