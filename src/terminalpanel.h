#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QList>
#include <QString>
#include <QStringList>

#ifdef NEOBEAM_HAS_TERMINAL
#include <qtermwidget.h>
#endif

struct TerminalConfig {
    QString fontFamily = "Monospace";
    int fontSize = 10;
    bool followCwd = true;
    int height = 300;
    QString colorScheme = "Linux";
    QString shell;            // empty = system default
    QStringList shellArgs;
    QString execCommand;      // run after shell starts
};

/// VSCode-like embedded terminal panel with split support
class TerminalPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(const TerminalConfig &cfg, QWidget *parent = nullptr);

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
    TerminalConfig m_cfg;
    QString m_cwd;
};
