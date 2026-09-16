#include "changereview.h"

#include <QAction>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QScrollBar>
#include <QTextBlock>
#include <QToolBar>
#include <QVBoxLayout>

namespace {

/// Shortest poll interval accepted, so a mistyped config cannot spawn a git
/// process per frame.
constexpr int kMinRefreshMs = 250;

} // namespace

DiffHighlighter::DiffHighlighter(const ReviewConfig &cfg, QTextDocument *document)
    : QSyntaxHighlighter(document)
{
    m_added.setForeground(cfg.added);
    m_removed.setForeground(cfg.removed);
    m_hunk.setForeground(cfg.hunk);
    m_file.setForeground(cfg.file);
    m_file.setFontWeight(QFont::Bold);
}

void DiffHighlighter::highlightBlock(const QString &text)
{
    if (text.isEmpty())
        return;
    // The file header markers are tested first: "+++" and "---" also start
    // with the added/removed prefixes.
    if (text.startsWith(QLatin1String("+++")) || text.startsWith(QLatin1String("---"))
        || text.startsWith(QLatin1String("diff --git")) || text.startsWith(QLatin1String("index "))
        || text.startsWith(QLatin1String("new file")) || text.startsWith(QLatin1String("deleted file"))
        || text.startsWith(QLatin1String("rename ")) || text.startsWith(QLatin1String("similarity "))
        || text.startsWith(QLatin1String("Untracked files"))) {
        setFormat(0, text.size(), m_file);
    } else if (text.startsWith(QLatin1String("@@"))) {
        setFormat(0, text.size(), m_hunk);
    } else if (text.startsWith(QLatin1Char('+'))) {
        setFormat(0, text.size(), m_added);
    } else if (text.startsWith(QLatin1Char('-'))) {
        setFormat(0, text.size(), m_removed);
    }
}

ChangeReviewPanel::ChangeReviewPanel(const ReviewConfig &cfg, QWidget *parent)
    : QWidget(parent), m_cfg(cfg)
{
    const QString bg = m_cfg.background.name();
    const QString fg = m_cfg.foreground.name();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header: same chrome as the terminal panel's toolbar, so the two panels
    // sitting next to each other look like one piece of furniture.
    auto *toolbar = new QToolBar(this);
    toolbar->setMovable(false);
    toolbar->setStyleSheet(QStringLiteral(
        "QToolBar { background: %1; border: none; spacing: 4px; }"
        "QToolButton { color: #aaa; background: transparent; border: none; padding: 2px 6px; }"
        "QToolButton:hover { background: #333; border-radius: 3px; }").arg(bg));

    m_stats = new QLabel(QStringLiteral("Changes"), toolbar);
    m_stats->setStyleSheet(QStringLiteral("color: #aaa; padding: 2px 6px;"));
    toolbar->addWidget(m_stats);

    auto *spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    spacer->setStyleSheet(QStringLiteral("background: transparent;"));
    toolbar->addWidget(spacer);

    auto *refreshAct = toolbar->addAction(QStringLiteral("⟳"), this, &ChangeReviewPanel::refresh);
    refreshAct->setToolTip(QStringLiteral("Refresh now"));
    auto *closeAct = toolbar->addAction(QStringLiteral("✕"), this, &QWidget::hide);
    closeAct->setToolTip(QStringLiteral("Close review panel"));
    layout->addWidget(toolbar);

    // Plain text, not rich text: selection and Ctrl+C then yield the diff
    // exactly as git wrote it, and the block-based document stays responsive
    // on a diff thousands of lines long.
    m_view = new QPlainTextEdit(this);
    m_view->setReadOnly(true);
    m_view->setUndoRedoEnabled(false);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_view->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_view->setFrameStyle(QFrame::NoFrame);
    QFont font(m_cfg.fontFamily, m_cfg.fontSize);
    font.setStyleHint(QFont::Monospace);
    if (m_cfg.fontFamily.isEmpty())
        font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_view->setFont(font);
    m_view->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; selection-background-color: #44475a; }")
            .arg(bg, fg));
    layout->addWidget(m_view, 1);

    m_highlighter = new DiffHighlighter(m_cfg, m_view->document());

    // The poll is what keeps the view current while an agent writes files:
    // those writes touch neither .git nor an nvim buffer, so no push trigger
    // exists for them. It runs only while the panel is visible (show/hideEvent)
    // and only while the window is active.
    m_poll.setInterval(qMax(m_cfg.refreshMs, kMinRefreshMs));
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (isVisible() && window() && window()->isActiveWindow())
            refresh();
    });
}

void ChangeReviewPanel::setWorkingDirectory(const QString &dir)
{
    if (dir.isEmpty() || dir == m_dir)
        return;
    m_dir = dir;
    m_text.clear(); // force a redraw: the new directory's diff may be identical text
    if (isVisible())
        refresh();
}

void ChangeReviewPanel::refresh()
{
    collect();
}

void ChangeReviewPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
    if (m_cfg.refreshMs > 0)
        m_poll.start();
    emit visibilityChanged(true);
}

void ChangeReviewPanel::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_poll.stop();
    emit visibilityChanged(false);
}

void ChangeReviewPanel::runGit(const QStringList &args,
                               std::function<void(bool, const QString &)> done)
{
    auto *proc = new QProcess(this);
    const QString dir = m_dir;
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [proc, done](int code, QProcess::ExitStatus) {
        proc->deleteLater();
        const QString out = QString::fromUtf8(code == 0 ? proc->readAllStandardOutput()
                                                        : proc->readAllStandardError());
        done(code == 0, out);
    });
    // FailedToStart (git not installed) never reaches finished().
    connect(proc, &QProcess::errorOccurred, this,
            [proc, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        done(false, QStringLiteral("git could not be started"));
    });
    QStringList full{QStringLiteral("-C"), dir, QStringLiteral("--no-pager")};
    full += args;
    proc->start(QStringLiteral("git"), full);
}

void ChangeReviewPanel::collect()
{
    if (m_dir.isEmpty())
        return;
    if (m_running) { // one git run at a time; the burst collapses into one rerun
        m_rerun = true;
        return;
    }
    m_running = true;

    const QString base = m_cfg.base;
    QStringList args{QStringLiteral("diff")};
    if (!base.isEmpty())
        args << base;

    runGit(args, [this, base](bool ok, const QString &output) {
        if (!ok) {
            m_running = false;
            // Only git's first line: outside a repository it answers with its
            // whole `git diff` usage text, which is not what to show a reader.
            const QString first = output.section(QLatin1Char('\n'), 0, 0).trimmed();
            const bool noRepo = first.contains(QLatin1String("not a git repository"),
                                               Qt::CaseInsensitive);
            m_stats->setText(noRepo ? QStringLiteral("Not a repository")
                                    : QStringLiteral("git error"));
            render(noRepo ? QStringLiteral("%1 is not a git repository.").arg(m_dir)
                          : (first.isEmpty() ? QStringLiteral("git failed.") : first));
            if (m_rerun) { m_rerun = false; collect(); }
            return;
        }
        // Untracked files never show up in `git diff`, so they are listed by
        // name underneath. Only meaningful against the worktree: with an
        // explicit base the comparison is between two revisions.
        if (!base.isEmpty()) {
            m_running = false;
            m_stats->setText(statsFor(output, 0));
            render(output.isEmpty() ? QStringLiteral("No changes against %1.").arg(base) : output);
            if (m_rerun) { m_rerun = false; collect(); }
            return;
        }
        runGit({QStringLiteral("ls-files"), QStringLiteral("--others"),
                QStringLiteral("--exclude-standard")},
               [this, diff = output](bool listed, const QString &untracked) {
            m_running = false;
            const QStringList names = listed
                ? untracked.split(QLatin1Char('\n'), Qt::SkipEmptyParts) : QStringList();
            QString text = diff;
            if (!names.isEmpty()) {
                if (!text.isEmpty() && !text.endsWith(QLatin1Char('\n')))
                    text += QLatin1Char('\n');
                text += QStringLiteral("Untracked files (%1):\n").arg(names.size());
                for (const QString &name : names)
                    text += QStringLiteral("+ %1\n").arg(name);
            }
            m_stats->setText(statsFor(diff, names.size()));
            render(text.isEmpty() ? QStringLiteral("No changes against HEAD.") : text);
            if (m_rerun) { m_rerun = false; collect(); }
        });
    });
}

void ChangeReviewPanel::render(const QString &text)
{
    QString shown = text;
    // A very large diff is capped rather than rendered in full: the panel is
    // for reading, and laying out a hundred thousand blocks on every poll is
    // not.
    if (m_cfg.maxLines > 0) {
        const QStringList lines = shown.split(QLatin1Char('\n'));
        if (lines.size() > m_cfg.maxLines) {
            shown = QStringList(lines.mid(0, m_cfg.maxLines)).join(QLatin1Char('\n'));
            shown += QStringLiteral("\n... truncated at %1 lines ([review] max_lines)\n")
                         .arg(m_cfg.maxLines);
        }
    }
    // Unchanged text is left alone: this is what makes an idle poll invisible,
    // keeping scroll position and the current selection intact.
    if (shown == m_text)
        return;
    m_text = shown;

    const int scroll = m_view->verticalScrollBar()->value();
    const int hscroll = m_view->horizontalScrollBar()->value();
    const int cursor = m_view->textCursor().position();
    m_view->setPlainText(shown);
    QTextCursor restored = m_view->textCursor();
    restored.setPosition(qMin(cursor, qMax(0, m_view->document()->characterCount() - 1)));
    m_view->setTextCursor(restored);
    // After the cursor: setTextCursor scrolls to make it visible.
    m_view->verticalScrollBar()->setValue(scroll);
    m_view->horizontalScrollBar()->setValue(hscroll);
}

QString ChangeReviewPanel::statsFor(const QString &diff, int untracked)
{
    int files = 0;
    int added = 0;
    int removed = 0;
    const QStringList lines = diff.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("diff --git")))
            ++files;
        else if (line.startsWith(QLatin1String("+++")) || line.startsWith(QLatin1String("---")))
            continue;
        else if (line.startsWith(QLatin1Char('+')))
            ++added;
        else if (line.startsWith(QLatin1Char('-')))
            ++removed;
    }
    files += untracked;
    if (files == 0)
        return QStringLiteral("No changes");
    QString text = QStringLiteral("%1 file%2, +%3 -%4")
                       .arg(files).arg(files == 1 ? QString() : QStringLiteral("s"))
                       .arg(added).arg(removed);
    if (untracked > 0)
        text += QStringLiteral(" (%1 untracked)").arg(untracked);
    return text;
}
