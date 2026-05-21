#include "terminalpanel.h"
#include <QShowEvent>
#include <QVBoxLayout>
#include <QDir>

TerminalPanel::TerminalPanel(const QString &fontFamily, int fontSize,
                             bool followCwd, QWidget *parent)
    : QWidget(parent)
    , m_fontFamily(fontFamily)
    , m_fontSize(fontSize)
    , m_followCwd(followCwd)
    , m_cwd(QDir::homePath())
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Toolbar with terminal actions
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet("QToolBar { background: #1e1e2e; border: none; spacing: 4px; }"
                             "QToolButton { color: #cdd6f4; background: transparent; border: none; padding: 2px 6px; }"
                             "QToolButton:hover { background: #313244; border-radius: 3px; }");

    auto *addAct = m_toolbar->addAction("+", this, &TerminalPanel::addTerminal);
    addAct->setToolTip("New terminal");
    auto *splitAct = m_toolbar->addAction("⎸", this, &TerminalPanel::splitHorizontal);
    splitAct->setToolTip("Split terminal");
    auto *closeAct = m_toolbar->addAction("✕", this, &TerminalPanel::removeCurrentTerminal);
    closeAct->setToolTip("Close terminal");

    layout->addWidget(m_toolbar);

    // Splitter holds terminal instances side by side
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setStyleSheet("QSplitter { background: #1e1e2e; }"
                              "QSplitter::handle { background: #313244; width: 2px; }");
    layout->addWidget(m_splitter, 1);

    setStyleSheet("background: #1e1e2e; border-top: 1px solid #313244;");
    setMinimumHeight(150);
    hide();

    // Create initial terminal
    addTerminal();
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
        }
    }
#endif
}

void TerminalPanel::setWorkingDirectory(const QString &dir)
{
    if (!m_followCwd || dir == m_cwd || dir.isEmpty())
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

#ifdef NEOBEAM_HAS_TERMINAL
QTermWidget *TerminalPanel::createTermWidget()
{
    auto *term = new QTermWidget(0, this);
    QFont f(m_fontFamily, m_fontSize);
    f.setStyleHint(QFont::Monospace);
    term->setTerminalFont(f);
    term->setColorScheme("Linux");
    term->setScrollBarPosition(QTermWidgetInterface::ScrollBarRight);
    term->setWorkingDirectory(m_cwd);
    term->setTerminalOpacity(1.0);

    // Defer shell start until visible so terminal gets correct size
    if (isVisible()) {
        term->startShellProgram();
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
