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

/// Native message area widget
class MessageWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MessageWidget(const QString &accent, QWidget *parent = nullptr);
    void showMsg(const QString &kind, const QVariantList &content, bool replaceLast);
    void clear();
    void showMode(const QVariantList &content);
    void showRuler(const QVariantList &content);

private:
    QLabel *m_label;
    QLabel *m_modeLabel;
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
    QString m_branch;
    QString m_diag;
    QString m_pos;
    QString m_encoding;
    QColor modeColor() const;

    /// Rebuild label text/colours/visibility and divider colours in place.
    void refresh();
};
