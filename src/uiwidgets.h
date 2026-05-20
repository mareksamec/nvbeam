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

/// Native tabline widget
class TablineWidget : public QTabBar
{
    Q_OBJECT
public:
    explicit TablineWidget(const QString &accent, QWidget *parent = nullptr);
    void update(int curtab, const QVariantList &tabs);

signals:
    void tabSelected(int tabId);

private:
    QVector<int> m_tabIds;
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
