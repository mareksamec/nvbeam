#include "terminalpanel.h"
#include <QShowEvent>
#include <QVBoxLayout>
#include <QDir>
#include <QShortcut>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>

TerminalPanel::TerminalPanel(const TerminalConfig &cfg, QWidget *parent)
    : QWidget(parent)
    , m_cfg(cfg)
    , m_cwd(QDir::homePath())
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Toolbar with terminal actions
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet("QToolBar { background: #000; border: none; spacing: 4px; }"
                             "QToolButton { color: #aaa; background: transparent; border: none; padding: 2px 6px; }"
                             "QToolButton:hover { background: #333; border-radius: 3px; }");

    auto *addAct = m_toolbar->addAction("+", this, &TerminalPanel::addTerminal);
    addAct->setToolTip("New terminal");
    auto *splitAct = m_toolbar->addAction("⎸", this, &TerminalPanel::splitHorizontal);
    splitAct->setToolTip("Split terminal");
    auto *closeAct = m_toolbar->addAction("✕", this, &TerminalPanel::removeCurrentTerminal);
    closeAct->setToolTip("Close terminal");

    layout->addWidget(m_toolbar);

    // Splitter holds terminal instances side by side
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setStyleSheet("QSplitter { background: #000; }"
                              "QSplitter::handle { background: #333; width: 2px; }");
    layout->addWidget(m_splitter, 1);

    setStyleSheet("background: #000; border-top: 1px solid #333;");
    setFixedHeight(cfg.height);
    hide();

    // Create initial terminal
    addTerminal();

    // Paste shortcuts: Shift+Insert and Ctrl+Shift+V
    auto *pasteShortcut1 = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Insert), this);
    connect(pasteShortcut1, &QShortcut::activated, this, &TerminalPanel::pasteClipboard);
    auto *pasteShortcut2 = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), this);
    connect(pasteShortcut2, &QShortcut::activated, this, &TerminalPanel::pasteClipboard);
}

void TerminalPanel::toggle()
{
    setVisible(!isVisible());
    emit visibilityChanged(isVisible());
}

void TerminalPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef NEOBEAM_HAS_TERMINAL
    for (auto *term : m_terminals) {
        if (term->property("_nb_pending").toBool()) {
            term->setProperty("_nb_pending", false);
            term->startShellProgram();
            if (!m_cfg.execCommand.isEmpty()) {
                term->sendText(m_cfg.execCommand + "\n");
            }
        }
    }
#endif
}

void TerminalPanel::setWorkingDirectory(const QString &dir)
{
    if (!m_cfg.followCwd || dir == m_cwd || dir.isEmpty())
        return;
    m_cwd = dir;
#ifdef NEOBEAM_HAS_TERMINAL
    if (m_currentTerm)
        m_currentTerm->changeDir(dir);
#endif
}

void TerminalPanel::addTerminal()
{
#ifdef NEOBEAM_HAS_TERMINAL
    auto *term = createTermWidget();
    m_splitter->addWidget(term);
    m_currentTerm = term;
    m_terminals.append(term);
    term->setFocus();
#endif
}

void TerminalPanel::splitHorizontal()
{
    addTerminal();
}

void TerminalPanel::removeCurrentTerminal()
{
#ifdef NEOBEAM_HAS_TERMINAL
    if (!m_currentTerm || m_terminals.size() <= 1)
        return;

    m_terminals.removeOne(m_currentTerm);
    m_currentTerm->deleteLater();
    m_currentTerm = m_terminals.isEmpty() ? nullptr : m_terminals.last();
    if (m_currentTerm)
        m_currentTerm->setFocus();
#endif
}

void TerminalPanel::pasteClipboard()
{
#ifdef NEOBEAM_HAS_TERMINAL
    if (m_currentTerm)
        m_currentTerm->pasteClipboard();
#endif
}

#ifdef NEOBEAM_HAS_TERMINAL
QTermWidget *TerminalPanel::createTermWidget()
{
    auto *term = new QTermWidget(0, this);

    // Font: use terminal-specific or fall back to global, with hinting/antialiasing
    QString family = m_cfg.fontFamily;
    int size = m_cfg.fontSize;
    QFont f(family, size);
    f.setStyleHint(QFont::Monospace);
    f.setHintingPreference(m_cfg.hinting);
    if (!m_cfg.antialiasing)
        f.setStyleStrategy(QFont::NoAntialias);
    term->setTerminalFont(f);

    // Color scheme (default "Linux" = white on black)
    term->setColorScheme(m_cfg.colorScheme);
    term->setScrollBarPosition(QTermWidgetInterface::ScrollBarRight);
    term->setWorkingDirectory(m_cwd);
    term->setTerminalOpacity(1.0);

    // NOTE: Sixel image protocol is not supported by upstream QTermWidget.
    // Tracking: https://github.com/lxqt/qtermwidget — no sixel code exists.
    // When/if upstream adds sixel support, enable it here.

    // Right-click paste (PuTTY-style)
    if (m_cfg.rightClickPaste) {
        term->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(term, &QWidget::customContextMenuRequested, this, [term]() {
            term->pasteClipboard();
        });
    }

    // Shell program
    if (!m_cfg.shell.isEmpty())
        term->setShellProgram(m_cfg.shell);
    if (!m_cfg.shellArgs.isEmpty())
        term->setArgs(m_cfg.shellArgs);

    // Defer shell start until visible so terminal gets correct size
    if (isVisible()) {
        term->startShellProgram();
        if (!m_cfg.execCommand.isEmpty()) {
            term->sendText(m_cfg.execCommand + "\n");
        }
    } else {
        term->setProperty("_nb_pending", true);
    }

    connect(term, &QTermWidget::termGetFocus, this, [this, term]() {
        m_currentTerm = term;
    });
    connect(term, &QTermWidget::finished, this, [this, term]() {
        m_terminals.removeOne(term);
        term->deleteLater();
        if (term == m_currentTerm)
            m_currentTerm = m_terminals.isEmpty() ? nullptr : m_terminals.last();
        if (m_terminals.isEmpty())
            hide();
    });

    return term;
}
#endif
