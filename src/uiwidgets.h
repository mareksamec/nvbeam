#pragma once

#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QVariant>

/// Native command line widget
class CmdlineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CmdlineWidget(const QString &accent, QWidget *parent = nullptr);
    void show(const QVariantList &content, int pos, const QString &firstc,
              const QString &prompt, int indent, int level);
    void hide(int level);
    void setPos(int pos, int level);

private:
    QLabel *m_label;
};

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
    explicit StatuslineWidget(const QString &accent, QWidget *parent = nullptr);

    /// Update all statusline segments from Neovim state
    void updateStatus(const QVariantMap &data);

    /// Update the current mode display
    void setMode(const QString &mode);

private:
    QLabel *m_modeLabel;
    QLabel *m_fileLabel;
    QLabel *m_branchLabel;
    QLabel *m_diagLabel;
    QLabel *m_posLabel;
    QLabel *m_encodingLabel;
    QString m_accent;
};
