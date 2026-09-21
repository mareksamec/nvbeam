#include "mainwindow.h"
#include "branchswitcher.h"
#include "gridwidget.h"
#include "nvimprocess.h"
#include "uiwidgets.h"
#include "terminalpanel.h"
#include "minimap.h"
#include "imagerenderer.h"
#include "gitstatus.h"
#include "config.h"
#include "session.h"
#include "shortcuts.h"
#include "logging.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QKeyEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QGuiApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QSplitter>
#include "changereview.h"
#include "colorscheme.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {

/// How far the hover background is nudged from the panel background towards
/// the foreground.
constexpr qreal kHoverBlend = 0.18;

/// How far the panel's own background is lifted off the terminal's: the diff
/// reads better on a slightly lighter, greyer ground, and the lift also tells
/// the two panes apart where they meet.
constexpr qreal kPanelLift = 0.08;

/// How far a file row's header band, and the hairline under it, are nudged
/// from the panel background towards a neutral grey.
constexpr qreal kRowHeaderBlend = 0.12;
constexpr qreal kRowBorderBlend = 0.28;

/// Paint the review panel in the terminal's colors: the same scheme file the
/// side terminal loads, with the [terminal] foreground/background overrides
/// applied on top of it, exactly as TerminalPanel does. A color the scheme
/// does not declare leaves the ReviewConfig default in place, so a missing or
/// half-written scheme degrades to readable rather than to invisible.
void applyTerminalScheme(ReviewConfig &review, const Config &cfg)
{
    const TerminalScheme scheme = loadTerminalScheme(cfg.terminalColorScheme);
    auto take = [](QColor &target, const QColor &value) {
        if (value.isValid())
            target = value;
    };
    take(review.background, scheme.background);
    take(review.foreground, scheme.foreground);
    take(review.added, scheme.ansi[AnsiGreen]);
    take(review.removed, scheme.ansi[AnsiRed]);
    take(review.hunk, scheme.ansi[AnsiBlue]);
    take(review.file, scheme.ansi[AnsiMagenta]);
    // Dimmed text (directories, tags, toolbar): the scheme's bright black,
    // which is what a terminal itself uses for de-emphasized output.
    take(review.dim, scheme.ansiIntense[AnsiBlack]);
    take(review.selection, scheme.ansi[AnsiBlack]);

    // The overrides win over the scheme, the same precedence the terminal
    // applies in TerminalPanel::effectiveColorScheme().
    take(review.foreground, QColor(cfg.terminalForeground));
    take(review.background, QColor(cfg.terminalBackground));
    review.background = blendColors(review.background, review.foreground, kPanelLift);

    // None of these are scheme colors: the hover tint, the header band and its
    // hairline are the background nudged towards a neutral grey of the
    // foreground's brightness, so they work on a dark and on a light scheme
    // alike, and a tinted foreground gives them no colour cast.
    const QColor neutral = QColor::fromHsl(0, 0, qGray(review.foreground.rgb()));
    review.hover = blendColors(review.background, neutral, kHoverBlend);
    review.rowHeader = blendColors(review.background, neutral, kRowHeaderBlend);
    review.rowBorder = blendColors(review.background, neutral, kRowBorderBlend);
}

} // namespace

/// Load a Lua script from the lua/ directory next to the executable.
/// Sources are cached after the first successful read; the scripts are only
/// (re)sent to nvim on attach, never per poll.
static QString loadLua(const QString &name)
{
    static QHash<QString, QString> cache;
    const auto cached = cache.constFind(name);
    if (cached != cache.constEnd())
        return *cached;
    // Try the installed share dir first (/usr/bin/neobeam -> /usr/share/neobeam/lua),
    // then the source-tree layout used when running from the build directory.
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/../share/neobeam/lua/" + name,
        QCoreApplication::applicationDirPath() + "/../lua/" + name,
        QCoreApplication::applicationDirPath() + "/lua/" + name,
    };
    for (const auto &path : searchPaths) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString src = QString::fromUtf8(f.readAll());
            cache.insert(name, src);
            return src;
        }
    }
    qWarning("Failed to load lua/%s", qPrintable(name));
    return {};
}

MainWindow::MainWindow(const Config &cfg, const QStringList &nvimArgs, QWidget *parent)
    : QMainWindow(parent)
    , m_cfg(cfg)
{
    setWindowTitle("Neobeam");
    resize(800, 600);

    // A single positional argument that is an existing directory means the
    // user opened a folder (e.g. `neobeam .`). Window geometry and terminal
    // split layout still restore normally; only the nvim/terminal cwd is
    // overridden to point at the folder (handled once nvim is ready, below).
    QString openFolder;
    if (nvimArgs.size() == 1) {
        QFileInfo info(nvimArgs.first());
        if (info.isDir())
            openFolder = QDir::cleanPath(info.absoluteFilePath());
    }
    m_pendingFolder = openFolder;
    // No arguments at all: present the project switcher once nvim is up.
    m_showSwitcherOnReady = nvimArgs.isEmpty();

    m_session = new Session();

    // Seed the recently-used lists from what was passed on the command line.
    if (m_cfg.switcherMaxRecent > 0) {
        if (!openFolder.isEmpty()) {
            m_session->addRecentFolder(openFolder, m_cfg.switcherMaxRecent);
        } else if (nvimArgs.size() == 1) {
            QFileInfo info(nvimArgs.first());
            if (info.isFile())
                m_session->addRecentFile(QDir::cleanPath(info.absoluteFilePath()),
                                         m_cfg.switcherMaxRecent);
        }
    }

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Vertical splitter: the editor area (top) and the terminal panel (bottom)
    // are resizable relative to each other by dragging the handle.
    m_vsplitter = new QSplitter(Qt::Vertical, central);
    m_vsplitter->setStyleSheet("QSplitter::handle { background: #333; height: 2px; }");
    m_vsplitter->setChildrenCollapsible(false);

    // Editor area container: tabline, grid+minimap row, messages, statusline.
    // A QFrame (rather than plain QWidget) so a focus-highlight border can be
    // drawn around the whole column via stylesheet, matching the terminal
    // splits' wrapper approach.
    m_editorArea = new QFrame(m_vsplitter);
    m_editorArea->setFrameShape(QFrame::NoFrame);
    // Scoped by object name rather than the bare "QFrame" type selector: QLabel
    // (used by the statusline segments) is itself a QFrame subclass and would
    // otherwise pick up the same border rule.
    m_editorArea->setObjectName("nbEditorArea");
    auto *editorArea = m_editorArea;
    auto *editorLayout = new QVBoxLayout(editorArea);
    editorLayout->setContentsMargins(1, 1, 1, 1); // room for the focus-highlight border
    editorLayout->setSpacing(0);

    m_tabline = new TablineWidget(cfg.accentColor, editorArea);
    editorLayout->addWidget(m_tabline);

    m_grid = new GridWidget(cfg.font(), editorArea);
    m_grid->setMouseConfig(cfg.mouseTerminalSelection, cfg.mouseAutoCopy,
                           QColor(cfg.mouseSelectionColor));

    // Grid + minimap side by side
    m_minimap = new MinimapWidget(editorArea);
    auto *editorRow = new QHBoxLayout();
    editorRow->setContentsMargins(0, 0, 0, 0);
    editorRow->setSpacing(0);
    editorRow->addWidget(m_grid, 1);
    editorRow->addWidget(m_minimap);
    editorLayout->addLayout(editorRow, 1);

    // ext_messages implies ext_cmdline: nvim externalizes the command line
    // too, so a native bar has to render it. Sits between the grid and the
    // message log, hidden unless a cmdline is active.
    m_cmdline = new CmdlineWidget(cfg.accentColor, editorArea);
    if (cfg.extMessages)
        editorLayout->addWidget(m_cmdline);

    m_messages = new MessageWidget(cfg.accentColor, editorArea);
    if (cfg.extMessages)
        editorLayout->addWidget(m_messages);
    else
        m_messages->hide(); // not in the layout; would otherwise paint at (0,0)

    m_statusline = new StatuslineWidget(cfg.accentColor, cfg.statuslineDivider, editorArea);
    // Branch name source: the built-in GitStatus provider by default, gitsigns
    // (vim.b.gitsigns_head) only when the user asks for it.
    m_statusline->setGitsignsBranch(cfg.uiGitsignsBranch);
    if (cfg.extStatusline)
        editorLayout->addWidget(m_statusline);

    // Highlight the whole editor column (tabline/grid/minimap/statusline) with
    // a 1px border while the grid holds keyboard focus.
    m_editorArea->setStyleSheet(QStringLiteral("QFrame#nbEditorArea { border: 1px solid transparent; }"));
    // Neobeam has no TTY, so nvim never gets the terminal focus-report
    // escape sequences that normally fire FocusGained/FocusLost. Emit the
    // same autocmd events ourselves so nvim behaves as it would in a real
    // terminal. Firing the event alone isn't enough to reload external file
    // changes though: unlike real GUI Vim (where focus-triggered timestamp
    // checking is built in), nvim only rechecks on FocusGained if the user's
    // own config defines a `checktime` autocmd for it, which most don't. So
    // run checktime ourselves alongside the event to match GUI Vim's behavior
    // regardless of the user's config.
    connect(m_grid, &GridWidget::focusChanged, this, [this](bool focused) {
        m_editorArea->setStyleSheet(focused
            ? QStringLiteral("QFrame#nbEditorArea { border: 1px solid %1; }").arg(m_cfg.focusBorderColor)
            : QStringLiteral("QFrame#nbEditorArea { border: 1px solid transparent; }"));
        const QString autocmdEvent = focused ? QStringLiteral("FocusGained") : QStringLiteral("FocusLost");
        qCDebug(nbFocus) << "grid" << (focused ? "focused" : "unfocused") << "-> doautocmd" << autocmdEvent;
        const QString cmd = focused
            ? QStringLiteral("doautocmd <nomodeline> FocusGained | checktime")
            : QStringLiteral("doautocmd <nomodeline> FocusLost");
        m_nvim->request("nvim_command", {cmd});
    });

    // Embedded terminal panel (VSCode-like, below statusline)
    TerminalConfig termCfg;
    termCfg.fontFamily = cfg.terminalFont.isEmpty() ? cfg.fontFamily : cfg.terminalFont;
    termCfg.fontSize = cfg.terminalFontSize > 0 ? cfg.terminalFontSize : cfg.fontSize;
    termCfg.followCwd = cfg.terminalFollowCwd;
    termCfg.height = cfg.terminalHeight;
    termCfg.colorScheme = cfg.terminalColorScheme;
    termCfg.foreground = cfg.terminalForeground;
    termCfg.background = cfg.terminalBackground;
    termCfg.shell = cfg.terminalShell;
    termCfg.shellArgs = cfg.terminalShellArgs;
    termCfg.execCommand = cfg.terminalExecCommand;
    termCfg.rightClickPaste = cfg.terminalRightClickPaste;
    termCfg.copyOnSelect = cfg.terminalCopyOnSelect;
    termCfg.wordCharacters = cfg.terminalWordCharacters;
    termCfg.antialiasing = cfg.terminalAntialiasing;
    termCfg.hinting = cfg.terminalHinting;
    termCfg.scrollback = cfg.terminalScrollback;
    termCfg.refreshPromptOnGitChange = cfg.terminalRefreshPromptOnGitChange;
    termCfg.focusBorderColor = QColor(cfg.focusBorderColor);
    m_terminal = new TerminalPanel(termCfg, m_vsplitter);

    // Side terminal: docked to the right of the editor area (next to the
    // minimap), intended for CLI tools like claude or nanocoder. It does not
    // follow nvim's cwd while running (changeDir types a "cd" command into
    // the terminal, which would corrupt an interactive tool's input); instead
    // it receives nvim's cwd once, right before it is shown.
    TerminalConfig sideCfg = termCfg;
    sideCfg.followCwd = false;
    // The side terminal command comes from the [[projects]] entry matching
    // the opened folder (none = plain shell). Resolved here, before the panel
    // exists, because session restore may start its shell in this constructor.
    sideCfg.execCommand.clear();
    for (const ProjectEntry &project : cfg.projects) {
        if (!openFolder.isEmpty() && project.path == openFolder) {
            sideCfg.execCommand = project.sideCommand;
            break;
        }
    }
    // Whether this launch has a side terminal command at all. Without one there
    // is no CLI tool to come back to, so the panel starts collapsed even when
    // the session says it was open - the case of opening a single file
    // ("Open with Neobeam", a file picker) rather than a pinned project.
    const bool haveSideCommand = !sideCfg.execCommand.isEmpty();
    sideCfg.sidePanel = true;
    // Independent font: fall back to the bottom terminal's font when unset.
    if (!cfg.sideTerminalFont.isEmpty())
        sideCfg.fontFamily = cfg.sideTerminalFont;
    if (cfg.sideTerminalFontSize > 0)
        sideCfg.fontSize = cfg.sideTerminalFontSize;

    // Horizontal splitter: editor area (left) and side terminal (right).
    auto *hsplitter = new QSplitter(Qt::Horizontal, m_vsplitter);
    m_hsplitter = hsplitter;
    hsplitter->setStyleSheet("QSplitter::handle { background: #333; width: 2px; }");
    hsplitter->setChildrenCollapsible(false);
    m_sideTerminal = new TerminalPanel(sideCfg, hsplitter);
    // Change review panel: third child, so it opens to the right of the side
    // terminal that runs the agent whose changes it shows. Hidden until asked
    // for (the review button or Ctrl+Shift+G).
    ReviewConfig reviewCfg;
    reviewCfg.width = cfg.reviewWidth;
    reviewCfg.refreshMs = cfg.reviewRefreshMs;
    reviewCfg.timeoutMs = cfg.reviewTimeoutMs;
    reviewCfg.base = cfg.reviewBase;
    // Font falls back to the side terminal's, which itself falls back to the
    // bottom terminal's: the panel sits beside them and should match.
    reviewCfg.fontFamily = !cfg.reviewFont.isEmpty() ? cfg.reviewFont : sideCfg.fontFamily;
    reviewCfg.fontSize = cfg.reviewFontSize > 0 ? cfg.reviewFontSize : sideCfg.fontSize;
    reviewCfg.maxLines = cfg.reviewMaxLines;
    reviewCfg.fileMaxHeight = cfg.reviewFileMaxHeight;
    reviewCfg.showContext = cfg.reviewShowContext;
    reviewCfg.syntax = cfg.reviewSyntax;
    applyTerminalScheme(reviewCfg, cfg);
    // An explicit file color beats the scheme, the same precedence the
    // [terminal] foreground/background overrides have inside the scheme pass.
    if (const QColor fileColor(cfg.reviewFileColor); fileColor.isValid())
        reviewCfg.file = fileColor;
    m_review = new ChangeReviewPanel(reviewCfg, hsplitter);
    m_review->hide();

    hsplitter->addWidget(editorArea);
    hsplitter->addWidget(m_sideTerminal);
    hsplitter->addWidget(m_review);
    hsplitter->setStretchFactor(0, 1); // editor area takes extra space
    hsplitter->setStretchFactor(1, 0);
    hsplitter->setStretchFactor(2, 0);
    // A hidden child gets no splitter space, so the panel's width can only be
    // applied once it is on screen. It takes that space from the editor area,
    // leaving the side terminal at whatever width the user dragged it to, and
    // reopens at the width it had when it was last closed.
    connect(m_review, &ChangeReviewPanel::visibilityChanged, this,
            [hsplitter, width = cfg.reviewWidth](bool visible) mutable {
        QList<int> sizes = hsplitter->sizes();
        if (sizes.size() < 3)
            return;
        if (!visible) {
            if (sizes[2] > 0)
                width = sizes[2];
            return;
        }
        sizes[0] = qMax(1, sizes[0] - width);
        sizes[2] = width;
        hsplitter->setSizes(sizes);
    });
    // Seed the side terminal with its configured startup width on its first
    // show; setting sizes while the panel is hidden has no effect (hidden
    // widgets get no splitter space).
    connect(m_sideTerminal, &TerminalPanel::visibilityChanged, this,
            [this, hsplitter, seeded = false](bool visible) mutable {
        if (!visible || seeded)
            return;
        seeded = true;
        const int w = qMax(1, hsplitter->width());
        hsplitter->setSizes({ qMax(1, w - m_cfg.sideTerminalWidth), m_cfg.sideTerminalWidth });
    });

    // Assemble the vertical splitter: editor row on top, terminal below.
    m_vsplitter->addWidget(hsplitter);
    m_vsplitter->addWidget(m_terminal);
    m_vsplitter->setStretchFactor(0, 1); // editor area takes extra space
    m_vsplitter->setStretchFactor(1, 0);
    layout->addWidget(m_vsplitter, 1);

    // Ctrl+` toggles terminal panel
    auto *termShortcut = new QShortcut(Shortcuts::toggleTerminal(), this);
    connect(termShortcut, &QShortcut::activated, m_terminal, &TerminalPanel::toggle);

    // Ctrl+Shift+E splits the terminal; Ctrl+Shift+W closes the active split.
    // Both act on the panel that currently holds focus (side terminal when
    // focused there, bottom panel otherwise).
    auto focusedPanel = [this]() -> TerminalPanel * {
        QWidget *focused = QApplication::focusWidget();
        if (focused && m_sideTerminal->isVisible() && m_sideTerminal->isAncestorOf(focused))
            return m_sideTerminal;
        return m_terminal;
    };
    auto *termSplitShortcut = new QShortcut(Shortcuts::splitTerminal(), this);
    connect(termSplitShortcut, &QShortcut::activated, this,
            [focusedPanel]() { focusedPanel()->addTerminal(); });
    auto *termCloseSplitShortcut = new QShortcut(Shortcuts::closeTerminalSplit(), this);
    connect(termCloseSplitShortcut, &QShortcut::activated, this,
            [focusedPanel]() { focusedPanel()->removeCurrentTerminal(); });

    // Ctrl+Shift+L opens the link in the focused terminal's selection. This is
    // the fallback for links qtermwidget's URL filter does not detect on its
    // own (see TerminalPanel::openSelectedLink); it only acts on the panel that
    // has focus, so it can never open a stale selection from the other one.
    auto *termLinkShortcut = new QShortcut(Shortcuts::openSelectedLink(), this);
    connect(termLinkShortcut, &QShortcut::activated, this, [this]() {
        if (TerminalPanel *panel = terminalPanelFor(QApplication::focusWidget()))
            panel->openSelectedLink();
    });

    // Ctrl+Shift+Down performs the same action as the terminal's ↓ toolbar button.
    auto *termCwdShortcut = new QShortcut(Shortcuts::broadcastCwd(), this);
    connect(termCwdShortcut, &QShortcut::activated, m_terminal, &TerminalPanel::cwdBroadcastRequested);

    // Ctrl+Shift+. toggles text selection mode (terminal-style selection in
    // the nvim buffer regardless of nvim's 'mouse' option).
    auto *selModeShortcut = new QShortcut(Shortcuts::toggleSelectionMode(), this);
    connect(selModeShortcut, &QShortcut::activated, this, [this]() {
        m_grid->setSelectionModeActive(!m_grid->selectionModeActive());
    });

    // Ctrl+Shift+M toggles keyboard focus between the message log and the
    // nvim grid (Ctrl+Tab from the log also returns to the grid).
    auto *msgFocusShortcut = new QShortcut(Shortcuts::focusMessages(), this);
    connect(msgFocusShortcut, &QShortcut::activated, this, [this]() {
        if (!m_cfg.extMessages) return; // feature disabled entirely
        if (!m_messages->isVisible()) {
            // Hidden via the panel's own "x" button: restore it first.
            m_messages->showPanel();
            m_messages->focusLog();
            return;
        }
        if (m_messages->logHasFocus()) {
            // Shortcut-out collapses; clicking away elsewhere leaves the
            // pane expanded (see the header toggle).
            m_messages->collapse();
            m_grid->setFocus();
        } else {
            m_messages->focusLog();
        }
    });

    // Ctrl+Shift+X: give the whole window to the focused pane (buffer area,
    // bottom terminal or side terminal) by hiding the others; pressing it
    // again restores exactly the ones that were actually visible before.
    auto *maximizeShortcut = new QShortcut(Shortcuts::maximizePane(), this);
    connect(maximizeShortcut, &QShortcut::activated, this, &MainWindow::toggleMaximizedMode);

    // Ctrl+Shift+G: toggle the change review panel (same as the side
    // terminal's review button).
    auto *reviewShortcut = new QShortcut(Shortcuts::toggleChangeReview(), this);
    connect(reviewShortcut, &QShortcut::activated, this, &MainWindow::toggleChangeReview);
    connect(m_sideTerminal, &TerminalPanel::changeReviewRequested,
            this, &MainWindow::toggleChangeReview);

    // Arrow-down toolbar button: broadcast nvim's current cwd to all splits of
    // the panel whose button was clicked.
    auto connectCwdBroadcast = [this](TerminalPanel *panel) {
        connect(panel, &TerminalPanel::cwdBroadcastRequested, this, [this, panel]() {
            m_nvim->request("nvim_exec_lua",
                {QStringLiteral("return vim.fn.getcwd()"), QVariantList{}},
                [panel](const QVariant &result) {
                    panel->broadcastCwd(result.toString());
                });
        });
    };
    connectCwdBroadcast(m_terminal);
    connectCwdBroadcast(m_sideTerminal);

    // Paste shortcuts: route to terminal or nvim depending on focus
    auto pasteHandler = [this]() {
        if (m_terminal->isVisible() && m_terminal->isAncestorOf(focusWidget())) {
            m_terminal->pasteClipboard();
        } else if (m_sideTerminal->isVisible() && m_sideTerminal->isAncestorOf(focusWidget())) {
            m_sideTerminal->pasteClipboard();
        } else {
            QString text = QGuiApplication::clipboard()->text();
            if (!text.isEmpty())
                m_nvim->request("nvim_paste", {text, true, -1});
        }
    };
    auto *pasteShortcut1 = new QShortcut(Shortcuts::pasteInsert(), this);
    connect(pasteShortcut1, &QShortcut::activated, this, pasteHandler);
    auto *pasteShortcut2 = new QShortcut(Shortcuts::pasteClipboard(), this);
    connect(pasteShortcut2, &QShortcut::activated, this, pasteHandler);

    // Bottom-right toggle buttons (minimap, terminal)
    auto *btnBar = new QWidget(central);
    auto *btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(0, 0, 4, 0);
    btnLayout->setSpacing(4);
    btnLayout->addStretch();

    auto btnStyle = QStringLiteral(
        "QPushButton { color: #888; background: #1a1b26; border: 1px solid #333; "
        "border-radius: 3px; padding: 2px 8px; font-size: 9pt; }"
        "QPushButton:hover { color: #cdd6f4; background: #313244; }"
        "QPushButton:checked { color: #50fa7b; border-color: #50fa7b; }");

    auto *minimapBtn = new QPushButton("⊟", central);
    minimapBtn->setToolTip("Toggle minimap");
    minimapBtn->setCheckable(true);
    minimapBtn->setChecked(true);
    minimapBtn->setFixedSize(28, 20);
    minimapBtn->setStyleSheet(btnStyle);
    btnLayout->addWidget(minimapBtn);

    m_termBtn = new QPushButton("⌨", central);
    m_termBtn->setToolTip(Shortcuts::hint(QStringLiteral("Toggle terminal"),
                                          Shortcuts::toggleTerminal()));
    m_termBtn->setCheckable(true);
    m_termBtn->setChecked(false);
    m_termBtn->setFixedSize(28, 20);
    m_termBtn->setStyleSheet(btnStyle);
    btnLayout->addWidget(m_termBtn);

    m_sideTermBtn = new QPushButton("◨", central);
    m_sideTermBtn->setToolTip(Shortcuts::hint(QStringLiteral("Toggle side terminal"),
                                              Shortcuts::toggleSideTerminal()));
    m_sideTermBtn->setCheckable(true);
    m_sideTermBtn->setChecked(false);
    m_sideTermBtn->setFixedSize(28, 20);
    m_sideTermBtn->setStyleSheet(btnStyle);
    btnLayout->addWidget(m_sideTermBtn);

    auto *selModeBtn = new QPushButton("⌶", central);
    selModeBtn->setToolTip(Shortcuts::hint(QStringLiteral("Toggle text selection mode"),
                                           Shortcuts::toggleSelectionMode()));
    selModeBtn->setCheckable(true);
    selModeBtn->setChecked(false);
    selModeBtn->setFixedSize(28, 20);
    selModeBtn->setStyleSheet(btnStyle);
    btnLayout->addWidget(selModeBtn);

    btnBar->setFixedHeight(24);
    layout->addWidget(btnBar);

    connect(minimapBtn, &QPushButton::toggled, m_minimap, &QWidget::setVisible);
    // Polls are skipped while the minimap is hidden, so kick one viewport
    // poll on re-show; it schedules a content scan if the buffer changed.
    connect(minimapBtn, &QPushButton::toggled, this, [this](bool on) {
        if (on && m_minimapViewTimer)
            m_minimapViewTimer->start();
    });
    connect(m_termBtn, &QPushButton::toggled, m_terminal, &QWidget::setVisible);
    connect(m_terminal, &TerminalPanel::visibilityChanged, m_termBtn, &QPushButton::setChecked);

    // Selection-mode button and Ctrl+Shift+. stay in sync via the grid's
    // signal; setSelectionModeActive no-ops on repeat values, so the
    // button<->grid connection loop terminates.
    connect(selModeBtn, &QPushButton::toggled, m_grid, &GridWidget::setSelectionModeActive);
    connect(m_grid, &GridWidget::selectionModeChanged, selModeBtn, &QPushButton::setChecked);

    // Side terminal opens in nvim's current cwd: resolve it first, seed the
    // panel with it, and only then show (showing starts the pending shell, so
    // the directory must be set before that).
    connect(m_sideTermBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!on) {
            m_sideTerminal->hide();
            return;
        }
        // Already visible: the button is being synced to the panel (session
        // restore shows the panel directly, before m_nvim exists).
        if (m_sideTerminal->isVisible())
            return;
        m_nvim->request("nvim_exec_lua",
            {QStringLiteral("return vim.fn.getcwd()"), QVariantList{}},
            [this](const QVariant &result) {
                const QString cwd = result.toString();
                if (!cwd.isEmpty())
                    m_sideTerminal->broadcastCwd(cwd);
                m_sideTerminal->show();
                m_sideTerminal->focusTerminal();
            });
    });
    // Ctrl+Shift+` toggles the side terminal. It goes through the button
    // rather than the panel: the button's handler above resolves nvim's cwd
    // and seeds the splitter width before showing, which showing the panel
    // directly would skip.
    auto *sideTermShortcut = new QShortcut(Shortcuts::toggleSideTerminal(), this);
    connect(sideTermShortcut, &QShortcut::activated, m_sideTermBtn, &QPushButton::toggle);
    connect(m_sideTerminal, &TerminalPanel::visibilityChanged, m_sideTermBtn, &QPushButton::setChecked);
    // Once the panel has been open in this session its visibility is real user
    // state again, so closeEvent may persist it (see m_sideTerminalSuppressed).
    connect(m_sideTerminal, &TerminalPanel::visibilityChanged, this, [this](bool visible) {
        if (visible)
            m_sideTerminalSuppressed = false;
    });

    setCentralWidget(central);

    // --- Session restore ---
    // Terminal layout: restore split count + proportions before deciding
    // visibility, so the panel comes back exactly as it was left.
    const bool restoreTerm = cfg.sessionRestoreTerminal && m_session->hasTerminalState();
    if (restoreTerm)
        m_terminal->restoreSplits(m_session->terminalSplitCount(),
                                  m_session->terminalSplitterState());

    // Expanded state: when session restore is active it takes precedence over
    // the start_expanded config default; otherwise fall back to the config.
    const bool startExpanded = restoreTerm ? m_session->terminalExpanded()
                                           : cfg.terminalStartExpanded;
    // Toggling the button (after connections are wired) keeps panel and button
    // in sync and triggers TerminalPanel::showEvent, which starts the shell.
    if (startExpanded)
        m_termBtn->setChecked(true);

    // Side terminal visibility: restored only when this launch actually has a
    // side command (see haveSideCommand above). Shown directly rather than via
    // the button: the button handler queries nvim's cwd, but m_nvim does not
    // exist yet. At startup the shell's default cwd matches nvim's anyway.
    // showEvent syncs the button and seeds the panel width.
    const bool restoreSide = restoreTerm && m_session->sideTerminalVisible();
    m_sideTerminalSuppressed = restoreSide && !haveSideCommand;
    if (restoreSide && haveSideCommand)
        m_sideTerminal->show();
    qCDebug(nbSession) << "restore: terminal" << restoreTerm << "expanded" << startExpanded
                       << "| side terminal saved visible" << m_session->sideTerminalVisible()
                       << "side command" << haveSideCommand
                       << "-> shown" << (restoreSide && haveSideCommand);

    // Editor/terminal split proportions. If no saved state, seed the terminal
    // with its configured startup height.
    if (cfg.sessionRestoreWindow && !m_session->verticalSplitterState().isEmpty()) {
        m_vsplitter->restoreState(m_session->verticalSplitterState());
    } else {
        const int h = qMax(1, height());
        m_vsplitter->setSizes({ qMax(1, h - cfg.terminalHeight), cfg.terminalHeight });
    }

    // Window geometry (best-effort; the window manager / Wayland compositor may
    // override size and will ignore position on Wayland). Applied before show().
    if (cfg.sessionRestoreWindow && !m_session->windowGeometry().isEmpty()) {
        restoreGeometry(m_session->windowGeometry());
    }

    // Popupmenu is a floating child of the grid
    m_popupmenu = new PopupmenuWidget(cfg.accentColor, m_grid);

    // Image renderer overlay (for future vim.ui.img API)
    m_imageRenderer = new ImageRenderer(m_grid);

    m_nvim = new NvimProcess(this);
    connect(m_nvim, &NvimProcess::redraw, m_grid, &GridWidget::handleRedraw);
    connect(m_nvim, &NvimProcess::finished, this, [this]() {
        if (m_quitting || !m_cfg.switcherKeepRunning) {
            close();
            return;
        }
        // nvim exited (:q) but the window stays: drop the dead instance's
        // grid state, start a fresh empty nvim and present the switcher over
        // it (the ready handler re-attaches the UI and opens it).
        m_grid->resetGrids();
        m_showSwitcherOnReady = true;
        m_nvim->start({});
    });
    connect(m_grid, &GridWidget::gridSizeReady, this, [this](int rows, int cols) {
        resizeNvimGrid(cols, rows);
    });
    connect(m_nvim, &NvimProcess::ready, this, [this]() {
        // Folder open (e.g. `neobeam .`): point both nvim's cwd and every
        // terminal split (including any restored from the session) at the
        // folder, overriding whatever cwd nvim/the terminals started with.
        // Applies only to the first instance; keep_running restarts start
        // in the default cwd until the switcher picks a project.
        if (!m_pendingFolder.isEmpty()) {
            m_nvim->request("nvim_set_current_dir", {m_pendingFolder});
            m_terminal->broadcastCwd(m_pendingFolder);
            updateTitleCwd(m_pendingFolder);
            m_pendingFolder.clear();
        }
        QTimer::singleShot(0, this, [this]() {
            int cols = m_grid->width() / m_grid->cellWidth();
            int rows = m_grid->height() / m_grid->cellHeight();
            if (cols < 1) cols = 80;
            if (rows < 1) rows = 24;

            QVariantMap opts;
            opts["ext_linegrid"] = true;
            opts["ext_hlstate"] = true;
            opts["ext_multigrid"] = true;
            if (m_cfg.extPopupmenu) opts["ext_popupmenu"] = true;
            if (m_cfg.extTabline) opts["ext_tabline"] = true;
            if (m_cfg.extMessages) opts["ext_messages"] = true;
            m_nvim->request("nvim_ui_attach", {cols, rows, opts});

            if (m_showSwitcherOnReady) {
                m_showSwitcherOnReady = false;
                openSwitcher();
            }

            // Register the bundled Lua helpers once per nvim instance: they
            // define functions in a `neobeam` table, and the poll timers call
            // tiny `return neobeam.*()` chunks instead of resending and
            // recompiling whole files on every fire.
            for (const char *script : {"statusline.lua", "bufferline.lua", "minimap.lua"}) {
                const QString lua = loadLua(QString::fromLatin1(script));
                if (!lua.isEmpty())
                    m_nvim->request("nvim_exec_lua", {lua, QVariantList{}});
            }

            // Autocmd-based push notifications need our RPC channel id.
            // Re-runs on each keep_running restart; the augroups are cleared
            // on re-registration.
            m_nvim->request("nvim_get_api_info", {},
                            [this](const QVariant &result) {
                const int chan = result.toList().value(0).toInt();
                if (chan <= 0) return;
                // Git-event autocmd (lua/gitstatus.lua) for the statusline.
                if (m_gitStatus) {
                    const QString lua = loadLua("gitstatus.lua");
                    if (!lua.isEmpty())
                        m_nvim->request("nvim_exec_lua", {lua, QVariantList{chan}});
                }
                // Cwd/current-file push (lua/events.lua): replaces the old
                // 500ms cwd poll and 2s MRU poll.
                const QString events = loadLua("events.lua");
                if (!events.isEmpty())
                    m_nvim->request("nvim_exec_lua", {events, QVariantList{chan}});
            });
        });
    });

    // Input
    connect(m_grid, &GridWidget::inputKey, this, [this](const QString &keys) {
        m_nvim->request("nvim_input", {keys});
    });
    connect(m_grid, &GridWidget::inputMouse, this,
            [this](const QString &button, const QString &action,
                   const QString &mod, int grid, int row, int col) {
        m_nvim->request("nvim_input_mouse", {button, action, mod, grid, row, col});
    });

    // File drops onto the buffer area: open each dropped file in nvim.
    //
    // Targeting: the drop resolves the window under the cursor; we focus it
    // first so the file opens where it was dropped (multi-grid aware).
    //
    // Modifiers select how the file is opened (mirrors common editor habits):
    //   (none)  :edit      open in the target window
    //   Shift   :split     open in a horizontal split
    //   Ctrl    :vsplit    open in a vertical split
    //   Alt     :tabedit   open in a new tab
    //
    // Using nvim_cmd with a list of args lets Neovim handle path escaping
    // (spaces, %, # etc.) instead of building a fragile command string.
    connect(m_grid, &GridWidget::filesDropped, this,
            [this](const QStringList &paths, int win, Qt::KeyboardModifiers mods) {
        // Focus the window under the drop point, if known.
        if (win >= 0)
            m_nvim->request("nvim_set_current_win", { win });

        QString openCmd = "edit";
        if (mods & Qt::AltModifier)
            openCmd = "tabedit";
        else if (mods & Qt::ControlModifier)
            openCmd = "vsplit";
        else if (mods & Qt::ShiftModifier)
            openCmd = "split";

        for (const QString &path : paths) {
            QVariantMap cmd;
            cmd["cmd"] = openCmd;
            cmd["args"] = QVariantList{ path };
            m_nvim->request("nvim_cmd", { cmd, QVariantMap{} });
        }
    });

    // Externalized UI signals
    connect(m_grid, &GridWidget::popupmenuShow, this,
            [this](const QVariantList &items, int selected, int row, int col, int grid) {
        Q_UNUSED(grid)
        m_popupmenu->show(items, selected, row, col, grid,
                          m_grid->cellWidth(), m_grid->cellHeight());
    });
    connect(m_grid, &GridWidget::popupmenuSelect, m_popupmenu, &PopupmenuWidget::select);
    connect(m_grid, &GridWidget::popupmenuHide, m_popupmenu, &PopupmenuWidget::hidePopup);

    connect(m_grid, &GridWidget::tablineUpdate, m_tabline, &TablineWidget::update);
    connect(m_tabline, &TablineWidget::tabSelected, this, [this](int tabId) {
        m_nvim->request("nvim_set_current_tabpage", {tabId});
    });

    connect(m_tabline, &TablineWidget::bufferSelected, this, [this](int bufId) {
        m_nvim->request("nvim_set_current_buf", {bufId});
    });

    // Minimap click scrolls to line
    connect(m_minimap, &MinimapWidget::lineClicked, this, [this](int line) {
        m_nvim->request("nvim_win_set_cursor", {0, QVariantList{line + 1, 0}});
    });

    connect(m_grid, &GridWidget::cmdlineShow, m_cmdline, &CmdlineWidget::showCmdline);
    connect(m_grid, &GridWidget::cmdlinePos, m_cmdline, &CmdlineWidget::setPos);
    connect(m_grid, &GridWidget::cmdlineHide, m_cmdline, &CmdlineWidget::hideCmdline);
    connect(m_grid, &GridWidget::cmdlineSpecialChar, m_cmdline, &CmdlineWidget::setSpecialChar);
    connect(m_grid, &GridWidget::msgShow, m_messages, &MessageWidget::showMsg);
    connect(m_grid, &GridWidget::msgClear, m_messages, &MessageWidget::clear);
    connect(m_grid, &GridWidget::msgShowmode, m_messages, &MessageWidget::showMode);
    connect(m_grid, &GridWidget::msgRuler, m_messages, &MessageWidget::showRuler);

    // Pre-warm the grid's nvim-side resize the instant the messages panel is
    // about to hide/show/expand/collapse, rather than waiting for Qt's
    // deferred layout pass to fire GridWidget::resizeEvent on its own: that
    // extra event-loop turn is what let a stale, mismatched frame flash
    // through during the panel's own height changes.
    connect(m_messages, &MessageWidget::aboutToResize, this, [this](int newMsgHeightPx) {
        if (m_grid->cellWidth() <= 0 || m_grid->cellHeight() <= 0) return;
        const int currentContribution = m_messages->isVisible() ? m_messages->height() : 0;
        const int newGridHeight = m_grid->height() + (currentContribution - newMsgHeightPx);
        const int cols = m_grid->width() / m_grid->cellWidth();
        const int rows = qMax(1, newGridHeight) / m_grid->cellHeight();
        resizeNvimGrid(cols, rows);
    });

    // Bufferline: refresh buffer list on each flush (throttled)
    if (cfg.bufferline) {
        auto *bufTimer = new QTimer(this);
        bufTimer->setSingleShot(true);
        bufTimer->setInterval(100);
        connect(bufTimer, &QTimer::timeout, this, [this]() {
            // `neobeam and ...` guards the window between attach and helper
            // registration (both run in the ready handler).
            m_nvim->request("nvim_exec_lua",
                {QStringLiteral("return neobeam and neobeam.bufferline()"), QVariantList{}},
                [this](const QVariant &result) {
                QVariantMap data = result.toMap();
                if (data.isEmpty()) return;
                int currentBuf = data["current"].toInt();
                QVariantList bufs = data["bufs"].toList();
                m_tabline->updateBuffers(bufs, currentBuf);
            });
        });
        connect(m_grid, &GridWidget::flushed, bufTimer, [bufTimer]() {
            if (!bufTimer->isActive()) bufTimer->start();
        });
    }

    // Statusline: poll Neovim for mode, file, position, diagnostics (throttled)
    if (cfg.extStatusline) {
        auto *statusTimer = new QTimer(this);
        statusTimer->setSingleShot(true);
        statusTimer->setInterval(100);
        connect(statusTimer, &QTimer::timeout, this, [this]() {
            m_nvim->request("nvim_exec_lua",
                {QStringLiteral("return neobeam and neobeam.statusline()"), QVariantList{}},
                [this](const QVariant &result) {
                const QVariantMap data = result.toMap();
                if (!data.isEmpty())
                    m_statusline->updateStatus(data);
            });
        });
        connect(m_grid, &GridWidget::flushed, statusTimer, [statusTimer]() {
            if (!statusTimer->isActive()) statusTimer->start();
        });
    }

    // Git segment of the statusline: push-model provider, refreshed only on
    // cwd changes (via updateTitleCwd), nvim write/DirChanged notifications
    // (lua/gitstatus.lua, registered in the ready handler), .git changes seen
    // by GitStatus's own watcher, and window activation (see eventFilter).
    // No polling.
    if (cfg.extStatusline && cfg.uiGitStatus) {
        m_gitStatus = new GitStatus(this);
        connect(m_gitStatus, &GitStatus::changed,
                m_statusline, &StatuslineWidget::updateGit);
        // A commit or a branch switch changes the whole diff at once; the
        // review panel picks it up here instead of on its next poll.
        connect(m_gitStatus, &GitStatus::changed, this, [this](const GitStatusInfo &) {
            if (m_review && m_review->isVisible())
                m_review->refresh();
        });
        connect(m_nvim, &NvimProcess::notification, this,
                [this](const QString &method, const QVariantList &) {
            if (method == QLatin1String("neobeam_git_event"))
                m_gitStatus->refresh();
        });

        // Bottom terminal only: a branch switch (including one made by typing
        // `git checkout`/`switch` straight into this terminal, caught by
        // GitStatus's own .git/HEAD watcher) leaves a branch-aware shell
        // prompt (zsh vcs_info, etc.) showing the old name until the next
        // command. Not wired to the side terminal, which usually runs an
        // agent/REPL rather than a shell - injecting Enter there risks
        // submitting an empty prompt to it.
        connect(m_gitStatus, &GitStatus::branchChanged,
                m_terminal, &TerminalPanel::refreshPromptOnBranchChange);

        // VSCode-style branch switcher: a popup hanging off the statusline's
        // git segment, opened by clicking the segment or with Ctrl+Shift+B.
        // Created only alongside the git provider, which is what runs every
        // git command for it.
        // Highlighted with the focus-border blue, not the green accent: it
        // keeps the branch picker visually distinct from the project switcher.
        m_branchSwitcher = new BranchSwitcherWidget(cfg.focusBorderColor, cfg.branchSwitcherFont,
                                                    cfg.branchSwitcherFontSize, this);
        m_statusline->setGitSegmentClickable(true);
        connect(m_statusline, &StatuslineWidget::gitSegmentClicked,
                this, &MainWindow::toggleBranchSwitcher);
        connect(m_branchSwitcher, &BranchSwitcherWidget::branchesRequested,
                m_gitStatus, &GitStatus::requestBranches);
        connect(m_gitStatus, &GitStatus::branchesReady,
                m_branchSwitcher, &BranchSwitcherWidget::setBranches);
        connect(m_branchSwitcher, &BranchSwitcherWidget::checkoutRequested,
                m_gitStatus, &GitStatus::checkoutBranch);
        connect(m_gitStatus, &GitStatus::checkoutFinished,
                m_branchSwitcher, &BranchSwitcherWidget::checkoutFinished);
        connect(m_branchSwitcher, &BranchSwitcherWidget::dismissed,
                this, [this]() { m_grid->setFocus(); });
        // A checkout rewrites the working tree under nvim's open buffers.
        // nvim only rereads them on a checktime, the same call the focus
        // handler makes for externally changed files.
        connect(m_gitStatus, &GitStatus::checkoutFinished, this,
                [this](bool ok, const QString &, const QString &) {
            if (ok)
                m_nvim->request("nvim_command", {QStringLiteral("checktime")});
        });
    }

    // Minimap, two-tier: a cheap viewport-only poll per flush keeps the
    // scroll/cursor overlay live, while the expensive content rebuild (a full
    // synchronous treesitter scan inside nvim) is debounced to run once after
    // edits settle. Previously the scan ran up to 50x/s while typing. Both
    // tiers no-op while the minimap is hidden.
    {
        m_minimapScanTimer = new QTimer(this);
        m_minimapScanTimer->setSingleShot(true);
        m_minimapScanTimer->setInterval(300);
        connect(m_minimapScanTimer, &QTimer::timeout, this, [this]() {
            if (!m_minimap->isVisible()) return;
            m_nvim->request("nvim_exec_lua",
                {QStringLiteral("return neobeam and neobeam.minimap_scan()"), QVariantList{}},
                [this](const QVariant &result) {
                QVariantMap data = result.toMap();
                qCDebug(nbMinimap) << "scan: empty" << data.isEmpty()
                                   << "lines" << data["lines"].toList().size()
                                   << "tick" << data["tick"].toInt();
                if (data.isEmpty()) return;
                int tick   = data["tick"].toInt();
                int total  = data["total"].toInt();
                int top    = data["top"].toInt() - 1;
                int bot    = data["bot"].toInt();
                int cursor = data["cursor"].toInt() - 1;
                // Only "lock in" this tick once the scan covered the whole capped
                // span (max 2000 lines, matching minimap.lua). This prevents a
                // partial scan from suppressing future refreshes via the tick gate.
                int scanned = data["scanned"].toInt();
                if (scanned >= qMin(total, 2000))
                    m_minimapLastTick = tick;
                QVariantList hlLines = data["lines"].toList();
                QVector<MinimapLine> lines;
                lines.reserve(hlLines.size());
                for (const auto &lineVar : hlLines) {
                    MinimapLine ml;
                    QVariantList cols = lineVar.toList();
                    ml.colors.reserve(cols.size());
                    for (const auto &colVar : cols) {
                        QString hex = colVar.toString();
                        ml.colors.append(hex.isEmpty() ? QColor(0x56, 0x5f, 0x89) : QColor(hex));
                    }
                    lines.append(ml);
                }
                m_minimap->updateContent(lines, total, top, bot - top, cursor);
            });
        });

        m_minimapViewTimer = new QTimer(this);
        m_minimapViewTimer->setSingleShot(true);
        m_minimapViewTimer->setInterval(20);
        connect(m_minimapViewTimer, &QTimer::timeout, this, [this]() {
            if (!m_minimap->isVisible()) return;
            m_nvim->request("nvim_exec_lua",
                {QStringLiteral("return neobeam and neobeam.minimap_view()"), QVariantList{}},
                [this](const QVariant &result) {
                QVariantMap data = result.toMap();
                qCDebug(nbMinimap) << "view: empty" << data.isEmpty()
                                   << "tick" << data["tick"].toInt()
                                   << "last" << m_minimapLastTick;
                if (data.isEmpty()) return;
                int top    = data["top"].toInt() - 1;
                int bot    = data["bot"].toInt();
                int cursor = data["cursor"].toInt() - 1;
                m_minimap->setViewport(top, bot - top, cursor);
                // Content changed: (re)start the scan debounce, so the rebuild
                // runs once, shortly after the last change.
                if (data["tick"].toInt() != m_minimapLastTick)
                    m_minimapScanTimer->start();
            });
        });
        connect(m_grid, &GridWidget::flushed, m_minimapViewTimer, [this]() {
            if (!m_minimapViewTimer->isActive()) m_minimapViewTimer->start();
        });
    }

    // Cwd and current-file tracking is push-based: lua/events.lua (registered
    // in the ready handler) notifies on DirChanged/BufEnter plus once at
    // registration, so nothing polls while nvim is idle. Feeds terminal
    // follow-cwd, the window title and the switcher's recently-used list.
    connect(m_nvim, &NvimProcess::notification, this,
            [this](const QString &method, const QVariantList &args) {
        if (method == QLatin1String("neobeam_cwd")) {
            const QString cwd = args.value(0).toString();
            if (cwd.isEmpty()) return;
            if (m_cfg.terminalFollowCwd)
                m_terminal->setWorkingDirectory(cwd);
            updateTitleCwd(cwd);
            // The home directory is nvim's default cwd, not a project.
            if (m_cfg.switcherMaxRecent > 0 && cwd != m_lastMruCwd
                && cwd != QDir::homePath() && QDir(cwd).exists()) {
                m_lastMruCwd = cwd;
                m_session->addRecentFolder(cwd, m_cfg.switcherMaxRecent);
            }
        } else if (method == QLatin1String("neobeam_file")) {
            const QString file = args.value(0).toString();
            // %f/%F in [ui] window_title: unnamed and non-file buffers
            // (term://, ...) show as empty rather than a raw buffer name.
            m_titleFile = file.contains(QLatin1String("://")) ? QString() : file;
            applyWindowTitle();
            // Skip unnamed buffers and non-file buffers (term://, ...).
            if (m_cfg.switcherMaxRecent > 0 && !file.isEmpty() && file != m_lastMruFile
                && !file.contains(QLatin1String("://")) && QFileInfo(file).isFile()) {
                m_lastMruFile = file;
                m_session->addRecentFile(file, m_cfg.switcherMaxRecent);
            }
        }
    });

    // Project switcher overlay (pinned projects + recent folders/files).
    m_switcher = new SwitcherWidget(cfg.accentColor, this);
    connect(m_switcher, &SwitcherWidget::itemSelected, this,
            &MainWindow::applySwitcherSelection);
    connect(m_switcher, &SwitcherWidget::dismissed, this,
            [this]() { m_grid->setFocus(); });

    // Ctrl+Shift+P toggles the switcher manually.
    auto *switcherShortcut = new QShortcut(Shortcuts::toggleProjectSwitcher(), this);
    connect(switcherShortcut, &QShortcut::activated, this, [this]() {
        if (m_switcher->isVisible())
            m_switcher->dismiss();
        else
            openSwitcher();
    });

    m_nvim->start(nvimArgs);

    // Ctrl+Tab cycles focus through the editor grid and each terminal split.
    // It is caught by an application-level event filter because both the grid
    // (GridWidget::event) and QTermWidget consume Tab before a QShortcut would
    // see it.
    qApp->installEventFilter(this);

    // Focus the editor grid on startup rather than the terminal. The terminal
    // panel grabs focus in its showEvent when started expanded, so defer this
    // until the event loop has processed those show events.
    QTimer::singleShot(0, this, [this]() { m_grid->setFocus(); });
}

MainWindow::~MainWindow()
{
    // stop() delivers the process's finished signal synchronously; the
    // keep_running handler must not restart nvim (or close()) mid-teardown.
    m_quitting = true;
    if (m_nvim) {
        m_nvim->stop();
    }
    delete m_session;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_quitting = true;
    // Quitting while a pane is maximized would persist the maximized layout:
    // panels the user still considers open read as hidden, and the splitter
    // states are the collapsed ones. Restore first, then save what the user
    // actually had.
    if (m_maximized != MaximizedPane::None)
        leaveMaximized();
    // Persist session state. We always write it; the restore side is gated by
    // config, so saving unconditionally keeps the file ready if the user later
    // enables restore.
    if (m_session) {
        m_session->setWindowGeometry(saveGeometry());
        if (m_vsplitter)
            m_session->setVerticalSplitterState(m_vsplitter->saveState());
        if (m_terminal) {
            m_session->setTerminalSplitCount(m_terminal->splitCount());
            m_session->setTerminalSplitterState(m_terminal->splitterState());
            m_session->setTerminalExpanded(m_terminal->isExpanded());
        }
        // A launch without a side command deliberately starts the panel
        // collapsed; leaving it that way must not erase the remembered state
        // for the next launch that does have a command.
        if (m_sideTerminal && !(m_sideTerminalSuppressed && !m_sideTerminal->isVisible()))
            m_session->setSideTerminalVisible(m_sideTerminal->isVisible());
        m_session->sync();
    }
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Coming back to the window is the one non-push git trigger, and the only
    // one that covers what a file watcher structurally cannot see: a git dir
    // on a filesystem without change notification, a branch switched while an
    // inotify watch was momentarily stale, or a repository created in the
    // current cwd while the app was unfocused. Costs one debounced
    // `git status` per activation, and never consumes the event.
    // WindowActivate rather than ApplicationActivate: the latter is deprecated
    // in Qt 6 and not delivered on every platform. It reaches every child
    // widget of the window, hence the sender check.
    if (event->type() == QEvent::WindowActivate && watched == this) {
        if (m_gitStatus)
            m_gitStatus->refresh();
        // The review panel skips its poll while the window is in the
        // background, so coming back to it is what brings the diff up to date.
        if (m_review && m_review->isVisible())
            m_review->refresh();
    }

    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        // Key_Backtab is what Ctrl+Shift+Tab produces; treat both directions
        // the same since there are only two panes to cycle between.
        if ((ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab)
            && (ke->modifiers() & Qt::ControlModifier)) {
            // Key_Backtab is what Ctrl+Shift+Tab produces.
            // From the message log, Ctrl+Tab returns straight to the grid
            // instead of entering the grid/terminal cycle.
            if (m_messages->logHasFocus())
                m_grid->setFocus();
            else
                cycleGridTerminalFocus(ke->key() == Qt::Key_Tab);
            return true;
        }
        // Ctrl+Shift+B toggles the git branch switcher from anywhere in the
        // window - buffer, either terminal, the message log - and closes it
        // again while it is open. Handled here rather than as a QShortcut for
        // the same reason as the font keys below: QTermWidget takes Ctrl+key
        // presses before a shortcut fires, so a QShortcut would work
        // everywhere except inside the terminal. Exact modifier match, so
        // Ctrl+B (nvim's page-up, tmux's prefix) is left alone and only the
        // Shift variant is claimed.
        if (m_branchSwitcher && ke->key() == Qt::Key_B
            && ke->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
            toggleBranchSwitcher();
            return true;
        }
        // Ctrl+- / Ctrl+= change the font size of the focused terminal panel.
        // Handled here rather than via QShortcut because QTermWidget consumes
        // plain Ctrl+key presses before shortcuts fire.
        if (ke->modifiers() == Qt::ControlModifier
            && (ke->key() == Qt::Key_Minus || ke->key() == Qt::Key_Equal)) {
            const int delta = ke->key() == Qt::Key_Equal ? 1 : -1;
            if (TerminalPanel *panel = terminalPanelFor(QApplication::focusWidget())) {
                panel->changeFontSize(delta);
                return true;
            }
            // Same keys resize the buffer font when the grid has focus.
            // Neither is a default nvim mapping, so nothing is shadowed.
            if (m_grid->hasFocus()) {
                m_grid->changeFontSize(delta);
                return true;
            }
        }
    }
    // Ctrl+wheel over a terminal zooms its panel's font.
    if (event->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent *>(event);
        if (we->modifiers() & Qt::ControlModifier) {
            const int dy = we->angleDelta().y();
            if (TerminalPanel *panel = terminalPanelFor(qobject_cast<QWidget *>(watched))) {
                if (dy != 0)
                    panel->changeFontSize(dy > 0 ? 1 : -1);
                return true;
            }
            // Ctrl+wheel over the buffer area, matching the terminal. Keyed on
            // the widget under the pointer, not the focused one, so the pane
            // being pointed at is the one that zooms.
            if (watched == m_grid) {
                if (dy != 0)
                    m_grid->changeFontSize(dy > 0 ? 1 : -1);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::focusNextPrevChild(bool next)
{
    // Shift+Tab (Qt::Key_Backtab) arrives here from the terminal:
    // Konsole::TerminalDisplay swallows plain Tab itself but deliberately
    // leaves the backwards direction to the window (TerminalDisplay::
    // focusNextPrevChild). Accepting it would move keyboard focus out of the
    // panel; refusing it makes Qt fall through to the terminal's key handler,
    // which sends the expected ESC[Z to the shell - Shift+Tab is a real binding
    // in TUI applications (Codex CLI, fzf, ...). Everything outside the
    // terminals keeps normal Tab navigation.
    if (terminalPanelFor(QApplication::focusWidget()))
        return false;
    return QMainWindow::focusNextPrevChild(next);
}

void MainWindow::toggleBranchSwitcher()
{
    if (!m_branchSwitcher)
        return;
    // Anchored to the git segment, which is hidden outside a repository - the
    // popup then falls back to the bottom-left corner and says so.
    m_branchSwitcher->toggle(m_statusline->gitSegment());
}

void MainWindow::openSwitcher()
{
    // m_titleCwd is nvim's cwd as of the last neobeam_cwd notification: it
    // anchors the switcher's path mode, so relative paths and the prefilled
    // starting directory match where the editor actually is.
    m_switcher->open(switcherItems(), m_titleCwd);
}

QList<SwitcherItem> MainWindow::switcherItems() const
{
    QList<SwitcherItem> items;
    QSet<QString> seen;

    for (const ProjectEntry &project : m_cfg.projects) {
        SwitcherItem item;
        item.kind = SwitcherItem::Project;
        item.label = project.name;
        item.path = project.path;
        item.sideCommand = project.sideCommand;
        items.append(item);
        seen.insert(project.path);
    }

    for (const QString &path : m_session->recentFolders()) {
        if (seen.contains(path) || !QDir(path).exists())
            continue;
        seen.insert(path);
        SwitcherItem item;
        item.kind = SwitcherItem::Folder;
        item.label = QFileInfo(path).fileName();
        item.path = path;
        items.append(item);
    }

    for (const QString &path : m_session->recentFiles()) {
        if (seen.contains(path) || !QFileInfo(path).isFile())
            continue;
        seen.insert(path);
        SwitcherItem item;
        item.kind = SwitcherItem::File;
        item.label = QFileInfo(path).fileName();
        item.path = path;
        items.append(item);
    }

    return items;
}

void MainWindow::applySwitcherSelection(const SwitcherItem &item)
{
    const bool isFile = item.kind == SwitcherItem::File;
    const QString dir = isFile ? QFileInfo(item.path).absolutePath() : item.path;

    // nvim: cwd to the project directory, then open the file (or the folder
    // itself, which brings up the configured file browser). nvim_cmd with an
    // args list lets Neovim handle path escaping.
    m_nvim->request("nvim_set_current_dir", {dir});
    QVariantMap cmd;
    cmd["cmd"] = QStringLiteral("edit");
    cmd["args"] = QVariantList{item.path};
    m_nvim->request("nvim_cmd", {cmd, QVariantMap{}});

    // Every terminal, side terminal included, moves to the project directory.
    m_terminal->broadcastCwd(dir);
    applyProjectSideCommand(dir);
    // Pinned projects title the window with their name; folder/file picks
    // fall back to the plain cwd title.
    if (item.kind == SwitcherItem::Project) {
        m_titleProject = item.label;
        m_titleProjectDir = dir;
    } else {
        m_titleProject.clear();
        m_titleProjectDir.clear();
    }
    updateTitleCwd(dir);

    if (m_cfg.switcherMaxRecent > 0) {
        m_session->addRecentFolder(dir, m_cfg.switcherMaxRecent);
        if (isFile)
            m_session->addRecentFile(item.path, m_cfg.switcherMaxRecent);
    }

    m_grid->setFocus();
}

// ~-shorten a path for display in the window title.
static QString tildify(const QString &path)
{
    const QString home = QDir::homePath();
    if (path == home)
        return QStringLiteral("~");
    if (path.startsWith(home + QLatin1Char('/')))
        return QLatin1Char('~') + path.mid(home.size());
    return path;
}

void MainWindow::updateTitleCwd(const QString &cwd)
{
    // Leaving the project directory ends the project context; re-selecting it
    // in the switcher sets m_titleProject again before calling here.
    if (cwd != m_titleProjectDir) {
        m_titleProject.clear();
        m_titleProjectDir.clear();
    }
    if (cwd != m_titleCwd) {
        m_titleCwd = cwd;
        // The git segment follows the same cwd-change choke point as the title.
        if (m_gitStatus)
            m_gitStatus->setWorkingDirectory(cwd);
        if (m_review)
            m_review->setWorkingDirectory(cwd);
    }
    applyWindowTitle();
}

void MainWindow::resizeNvimGrid(int cols, int rows)
{
    if (cols <= 0 || rows <= 0) return;
    if (cols == m_lastGridCols && rows == m_lastGridRows) return;
    m_lastGridCols = cols;
    m_lastGridRows = rows;
    // Fire-and-forget: nothing consumes the response, only the async redraw
    // stream that follows it, so a notification (no reply expected) avoids
    // making nvim format one we'd just discard.
    m_nvim->notify("nvim_ui_try_resize_grid", {1, cols, rows});
}

void MainWindow::toggleMaximizedMode()
{
    if (m_maximized != MaximizedPane::None) {
        leaveMaximized();
        return;
    }
    // The pane that holds focus is the one to maximize; the buffer area is
    // the fallback, which is also what an unfocused window or a click on the
    // toolbar lands on.
    TerminalPanel *panel = terminalPanelFor(QApplication::focusWidget());
    if (panel == m_terminal)
        enterMaximized(MaximizedPane::Terminal);
    else if (panel == m_sideTerminal)
        enterMaximized(MaximizedPane::SideTerminal);
    else
        enterMaximized(MaximizedPane::Editor);
}

void MainWindow::toggleChangeReview()
{
    if (!m_review)
        return;
    if (m_review->isVisible()) {
        m_review->hide();
        return;
    }
    // A maximized pane owns the whole window; the panel would have nowhere to
    // open, so leave that mode first (Ctrl+Shift+X does the same on toggle).
    if (m_maximized != MaximizedPane::None)
        leaveMaximized();
    // nvim's cwd is the repository the panel reports on; it arrives via
    // updateTitleCwd() and is empty only before nvim has reported one.
    if (!m_titleCwd.isEmpty())
        m_review->setWorkingDirectory(m_titleCwd);
    m_review->show();
}

void MainWindow::enterMaximized(MaximizedPane pane)
{
    // Capture what's actually open before touching anything, so a panel the
    // user had already closed doesn't come back on restore. The splitter
    // states go with it: a hidden child's space is handed to its siblings and
    // is not given back on show.
    m_maxEditorWasVisible = m_editorArea->isVisible();
    m_maxTerminalWasVisible = m_terminal->isVisible();
    m_maxSideTerminalWasVisible = m_sideTerminal->isVisible();
    m_maxReviewWasVisible = m_review->isVisible();
    m_maxMessagesWasVisible = m_cfg.extMessages && m_messages->isVisible();
    m_maxVSplitterState = m_vsplitter->saveState();
    m_maxHSplitterState = m_hsplitter->saveState();
    m_maximized = pane;

    switch (pane) {
    case MaximizedPane::Editor:
        // Driven through the same toggle buttons/methods a user click would
        // use, rather than calling hide() directly, so button check-state and
        // session persistence stay exactly as consistent as normal use.
        if (m_maxTerminalWasVisible) m_termBtn->setChecked(false);
        if (m_maxSideTerminalWasVisible) m_sideTermBtn->setChecked(false);
        if (m_maxReviewWasVisible) m_review->hide();
        if (m_maxMessagesWasVisible) m_messages->hidePanel();
        m_grid->setFocus(); // land on the buffer now filling the freed space
        break;
    case MaximizedPane::Terminal:
        // The editor area and the side terminal are the two halves of the
        // horizontal splitter, so hiding it covers both in one step and
        // leaves the whole window to the bottom panel.
        m_hsplitter->hide();
        m_terminal->focusTerminal();
        break;
    case MaximizedPane::SideTerminal:
        m_editorArea->hide();
        if (m_maxTerminalWasVisible) m_termBtn->setChecked(false);
        if (m_maxReviewWasVisible) m_review->hide();
        m_sideTerminal->focusTerminal();
        break;
    case MaximizedPane::None:
        break;
    }
}

void MainWindow::leaveMaximized()
{
    const MaximizedPane pane = m_maximized;
    m_maximized = MaximizedPane::None;

    // Show the containers first: a panel restored inside a hidden parent
    // would report itself visible while nothing is on screen.
    m_hsplitter->show();
    if (m_maxEditorWasVisible) m_editorArea->show();
    if (m_maxTerminalWasVisible) m_termBtn->setChecked(true);
    // Shown directly rather than via m_sideTermBtn: the button's handler
    // re-fetches nvim's cwd and steals focus into the terminal (right for a
    // user opening it fresh, wrong for just restoring what was already there
    // before maximizing). showEvent's own visibilityChanged still syncs the
    // button's checked state, same as session restore does.
    if (m_maxSideTerminalWasVisible) m_sideTerminal->show();
    if (m_maxReviewWasVisible) m_review->show();
    if (m_maxMessagesWasVisible) m_messages->showPanel();
    if (!m_maxVSplitterState.isEmpty()) m_vsplitter->restoreState(m_maxVSplitterState);
    if (!m_maxHSplitterState.isEmpty()) m_hsplitter->restoreState(m_maxHSplitterState);

    // Focus goes back to the pane that was maximized, so the key toggles the
    // view without also moving the cursor somewhere else.
    switch (pane) {
    case MaximizedPane::Terminal:      m_terminal->focusTerminal(); break;
    case MaximizedPane::SideTerminal:  m_sideTerminal->focusTerminal(); break;
    default:                           m_grid->setFocus(); break;
    }
}

void MainWindow::applyWindowTitle()
{
    QString title;
    if (!m_cfg.uiWindowTitle.isEmpty()) {
        const QString &tpl = m_cfg.uiWindowTitle;
        title.reserve(tpl.size());
        for (int i = 0; i < tpl.size(); ++i) {
            const QChar c = tpl.at(i);
            if (c != QLatin1Char('%') || i + 1 == tpl.size()) {
                title += c;
                continue;
            }
            const QChar token = tpl.at(++i);
            switch (token.unicode()) {
            case 'p': title += m_titleProject; break;
            case 'd': title += tildify(m_titleCwd); break;
            case 'D': title += m_titleCwd; break;
            case 'f': title += QFileInfo(m_titleFile).fileName(); break;
            case 'F': title += tildify(m_titleFile); break;
            case '%': title += QLatin1Char('%'); break;
            // Unknown placeholders render literally; config load warns.
            default: title += c; title += token; break;
            }
        }
    } else if (!m_titleProject.isEmpty()) {
        title = m_titleProject + QStringLiteral(" - ") + tildify(m_titleCwd);
    } else if (m_titleCwd.isEmpty() || m_titleCwd == QDir::homePath()) {
        title = QStringLiteral("Neobeam");
    } else {
        title = QStringLiteral("Neobeam - ") + tildify(m_titleCwd);
    }
    if (title != windowTitle()) {
        qCDebug(nbTitle) << "title:" << title;
        setWindowTitle(title);
    }
}

void MainWindow::applyProjectSideCommand(const QString &dir)
{
    QString command;
    for (const ProjectEntry &project : m_cfg.projects) {
        if (project.path == dir) {
            command = project.sideCommand;
            break;
        }
    }
    // Moves the side terminal to dir and applies the command: pending shells
    // are fixed up in place, a running plain shell just cds, and anything
    // else is respawned fresh (see TerminalPanel::adoptProject).
    m_sideTerminal->adoptProject(dir, command);

    // A project that declares a side command wants its CLI tool in view, so the
    // panel is opened here rather than waiting for the toolbar button. This is
    // the switcher's path into a project (also taken after nvim exits with
    // keep_running): no folder was on the command line, so startup could not
    // resolve a side command and left the panel collapsed. Shown after
    // adoptProject, whose pending shells start on the show. Projects without a
    // command leave the panel as it is - hidden stays hidden, and a panel the
    // user opened for a plain shell is kept.
    if (!command.isEmpty() && !m_sideTerminal->isVisible())
        m_sideTerminal->show();
}

TerminalPanel *MainWindow::terminalPanelFor(QWidget *widget) const
{
    if (!widget)
        return nullptr;
    if (m_terminal && (m_terminal == widget || m_terminal->isAncestorOf(widget)))
        return m_terminal;
    if (m_sideTerminal && (m_sideTerminal == widget || m_sideTerminal->isAncestorOf(widget)))
        return m_sideTerminal;
    return nullptr;
}

void MainWindow::cycleGridTerminalFocus(bool forward)
{
    // Fixed cycle order: grid (position 0), then each bottom-terminal split,
    // then each side-terminal split. Hidden panels contribute no positions.
    const int bottomCount = (m_terminal && m_terminal->isVisible())
                                ? m_terminal->splitCount() : 0;
    const int sideCount = (m_sideTerminal && m_sideTerminal->isVisible())
                              ? m_sideTerminal->splitCount() : 0;
    const int total = 1 + bottomCount + sideCount;
    if (total == 1) {
        m_grid->setFocus();
        return;
    }

    QWidget *focused = QApplication::focusWidget();
    int pos = 0;
    if (focused && bottomCount > 0 && m_terminal->isAncestorOf(focused)) {
        const int idx = m_terminal->indexOfFocused(focused);
        pos = (idx >= 0) ? idx + 1 : 1;
    } else if (focused && sideCount > 0 && m_sideTerminal->isAncestorOf(focused)) {
        const int idx = m_sideTerminal->indexOfFocused(focused);
        pos = 1 + bottomCount + ((idx >= 0) ? idx : 0);
    }

    pos = (pos + (forward ? 1 : -1) + total) % total;
    if (pos == 0)
        m_grid->setFocus();
    else if (pos <= bottomCount)
        m_terminal->focusTerminalAt(pos - 1);
    else
        m_sideTerminal->focusTerminalAt(pos - 1 - bottomCount);
}
