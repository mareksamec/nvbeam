#pragma once

#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVariant>
#include <QPainter>
#include <QPaintEvent>
#include "gitstatus.h"

class QPlainTextEdit;
class QPushButton;

/// Native popup menu widget
class PopupmenuWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PopupmenuWidget(const QString &accent, QWidget *parent = nullptr);
    void show(const QVariantList &items, int selected, int row, int col,
              int grid, int cellWidth, int cellHeight);
    void select(int idx);
    void hidePopup();

private:
    QListWidget *m_list;
    QString m_accent;
};

/// Native tabline widget (can show tabpages or buffers)
class TablineWidget : public QTabBar
{
    Q_OBJECT
public:
    explicit TablineWidget(const QString &accent, QWidget *parent = nullptr);
    void update(int curtab, const QVariantList &tabs);
    void updateBuffers(const QVariantList &buffers, int currentBuf);

signals:
    void tabSelected(int tabId);
    void bufferSelected(int bufId);

private:
    QVector<int> m_tabIds;
    bool m_bufferMode = false;
};

/// Native command line bar (ext_cmdline, implied by ext_messages): renders the
/// active : / ? / prompt line with a block cursor while nvim keeps handling
/// the input. Hidden when no cmdline is active. cmdline_block_* events
/// (multi-line :function input) are not rendered.
class CmdlineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CmdlineWidget(const QString &accent, QWidget *parent = nullptr);

public slots:
    void showCmdline(const QVariantList &content, int pos, const QString &firstc,
                     const QString &prompt, int indent, int level);
    void setPos(int pos, int level);
    void hideCmdline();
    void setSpecialChar(const QString &c, bool shift);

private:
    void render();

    QLabel *m_label;
    QString m_accent;
    QString m_firstc, m_prompt, m_content, m_special;
    int m_bytePos = 0; // cursor position, a byte offset into m_content's UTF-8
    int m_indent = 0;
};

/// Native message area (ext_messages): a timestamped log of msg_show events,
/// two lines tall by default, expandable via a "Messages" header button.
/// Text is mouse-selectable and auto-copies on release like the statusline.
class MessageWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MessageWidget(const QString &accent, QWidget *parent = nullptr);
    /// Move keyboard focus into the log, expanding it first so arrow/page
    /// keys have something to scroll (Ctrl+Shift+M).
    void focusLog();
    bool logHasFocus() const;
    /// Collapse back to the two-line view (Ctrl+Shift+M out; a plain click
    /// away leaves the pane expanded).
    void collapse();
    /// Fully hide the panel (the "x" button), handing its space back to the
    /// grid. Restored via Ctrl+Shift+M / showPanel().
    void hidePanel();
    /// Restore a panel hidden by hidePanel(), at its previous expand state.
    void showPanel();
    void showMsg(const QString &kind, const QVariantList &content, bool replaceLast);
    void clear();
    void showMode(const QVariantList &content);
    void showRuler(const QVariantList &content);

signals:
    /// Emitted just before a height-affecting change (expand/collapse,
    /// hide/show) is applied, carrying the panel's total height in pixels
    /// once the change lands. Lets the grid's nvim-side resize be requested
    /// immediately instead of waiting for Qt's deferred layout pass to fire
    /// GridWidget::resizeEvent.
    void aboutToResize(int newHeightPx);

private:
    void applyHeight();
    /// Predicts the panel's total height once applyHeight()/hide()/show()
    /// takes effect, from the current geometry and m_expanded — used to
    /// populate aboutToResize() before the change actually happens.
    int targetHeight() const;

    QPlainTextEdit *m_log;
    QPushButton *m_toggle;
    QPushButton *m_hideButton;
    QLabel *m_modeLabel;
    // Document position where the last message starts, so replace_last can
    // overwrite it in place (nvim uses it for progress-style updates).
    int m_lastMsgPos = -1;
    bool m_expanded = false;
};

/// Native statusline widget displaying mode, file, position, diagnostics
class StatuslineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StatuslineWidget(const QString &accent, const QString &dividerStyle = "powerline",
                              QWidget *parent = nullptr);

    /// Update all statusline segments from Neovim state
    void updateStatus(const QVariantMap &data);

    /// Update the current mode display
    void setMode(const QString &mode);

public slots:
    /// Update the git segment (branch + staged/modified/untracked and
    /// ahead/behind counts) from the push-model GitStatus provider.
    void updateGit(const GitStatusInfo &info);

private:
    // Per-segment selectable labels (concrete types live in the .cpp).
    QLabel *m_modeLabel = nullptr;
    QLabel *m_branchLabel = nullptr;
    QLabel *m_fileLabel = nullptr;
    QLabel *m_diagLabel = nullptr;
    QLabel *m_encLabel = nullptr;
    QLabel *m_posLabel = nullptr;
    // One trailing divider per segment slot, in slot order.
    QVector<QWidget *> m_dividers;

    QString m_dividerStyle;
    QString m_accent;
    QString m_mode = "NORMAL";
    QString m_file;
    QString m_branch;   // from statusline.lua (gitsigns, includes the branch glyph)
    GitStatusInfo m_git; // from the Qt-side GitStatus provider
    QString m_diag;
    QString m_pos;
    QString m_encoding;
    QColor modeColor() const;

    /// Rebuild label text/colours/visibility and divider colours in place.
    void refresh();
};
