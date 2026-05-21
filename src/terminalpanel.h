#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QList>
#include <QString>

#ifdef NEOBEAM_HAS_TERMINAL
#include <qtermwidget.h>
#endif

/// VSCode-like embedded terminal panel with split support
class TerminalPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(const QString &fontFamily, int fontSize,
                           bool followCwd, QWidget *parent = nullptr);

    /// Update the working directory (called when nvim cwd changes)
    void setWorkingDirectory(const QString &dir);

    /// Toggle panel visibility
    void toggle();

signals:
    void visibilityChanged(bool visible);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void addTerminal();
    void splitHorizontal();
    void removeCurrentTerminal();

private:
#ifdef NEOBEAM_HAS_TERMINAL
    QTermWidget *createTermWidget();
    QTermWidget *m_currentTerm = nullptr;
    QList<QTermWidget *> m_terminals;
#endif

    QSplitter *m_splitter = nullptr;
    QToolBar *m_toolbar = nullptr;
    QString m_fontFamily;
    int m_fontSize;
    bool m_followCwd;
    QString m_cwd;
};
