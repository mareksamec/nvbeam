#include "uiwidgets.h"
#include <functional>
#include "logging.h"
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QPainterPath>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QClipboard>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTime>

// --- PopupmenuWidget ---

PopupmenuWidget::PopupmenuWidget(const QString &accent, QWidget *parent)
    : QWidget(parent)
    , m_accent(accent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_list = new QListWidget(this);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setStyleSheet(QString(
        "QListWidget { background-color: #1a1b26; color: #c0caf5; border: 1px solid %1; "
        "font-family: monospace; font-size: 10pt; border-radius: 4px; }"
        "QListWidget::item { padding: 2px 8px; }"
        "QListWidget::item:selected { background-color: %1; color: #1a1b26; }"
    ).arg(accent));
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    layout->addWidget(m_list);
    setFocusPolicy(Qt::NoFocus);
    setVisible(false);
}

void PopupmenuWidget::show(const QVariantList &items, int selected, int row, int col,
                            int grid, int cellWidth, int cellHeight)
{
    m_list->clear();
    for (const auto &item : items) {
        QVariantList entry = item.toList();
        QString word = entry.isEmpty() ? "" : entry[0].toString();
        QString kind = (entry.size() >= 2) ? entry[1].toString() : "";
        QString display = word;
        if (!kind.isEmpty()) display += "  " + kind;
        m_list->addItem(display);
    }

    int maxVisible = qMin(items.size(), 12);
    int itemH = m_list->sizeHintForRow(0);
    if (itemH <= 0) itemH = 22;
    int h = maxVisible * itemH + 6;
    int w = qMin(450, qMax(220, m_list->sizeHintForColumn(0) + 30));

    if (grid == -1) {
        // Wildmenu anchored to the external cmdline (ext_cmdline): col is a
        // byte position in the cmdline. Place the popup above the bottom edge
        // of the grid area, roughly under the completed word.
        const int parentH = parentWidget() ? parentWidget()->height() : 0;
        move(qMax(0, col) * cellWidth, qMax(0, parentH - h));
    } else {
        move(col * cellWidth, (row + 1) * cellHeight);
    }
    setFixedSize(w, h);
    select(selected);
    setVisible(true);
    raise();
}

void PopupmenuWidget::select(int idx)
{
    if (idx >= 0 && idx < m_list->count())
        m_list->setCurrentRow(idx);
    else
        m_list->clearSelection();
}

void PopupmenuWidget::hidePopup()
{
    setVisible(false);
}

// --- TablineWidget ---

TablineWidget::TablineWidget(const QString &accent, QWidget *parent)
    : QTabBar(parent)
{
    setStyleSheet(QString(
        "QTabBar { background: #1a1b26; border-bottom: 1px solid #333; }"
        "QTabBar::tab { background: #1a1b26; color: #888; padding: 6px 16px; "
        "border: none; border-bottom: 2px solid transparent; margin-right: 1px; }"
        "QTabBar::tab:selected { color: %1; border-bottom: 2px solid %1; }"
        "QTabBar::tab:hover { color: #c0caf5; }"
    ).arg(accent));
    setExpanding(false);
    setVisible(false);

    connect(this, &QTabBar::currentChanged, this, [this](int idx) {
        if (idx >= 0 && idx < m_tabIds.size()) {
            if (m_bufferMode)
                emit bufferSelected(m_tabIds[idx]);
            else
                emit tabSelected(m_tabIds[idx]);
        }
    });
}

void TablineWidget::update(int curtab, const QVariantList &tabs)
{
    if (m_bufferMode) return; // ignore tabline events in buffer mode

    blockSignals(true);
    while (count() > 0) removeTab(0);
    m_tabIds.clear();

    int currentIdx = 0;
    for (int i = 0; i < tabs.size(); i++) {
        QVariantMap tab = tabs[i].toMap();
        int id = tab.value("tab").toInt();
        QString name = tab.value("name", "unnamed").toString();
        addTab(name);
        m_tabIds.append(id);
        if (id == curtab) currentIdx = i;
    }

    setCurrentIndex(currentIdx);
    setVisible(true);
    blockSignals(false);
}

void TablineWidget::updateBuffers(const QVariantList &buffers, int currentBuf)
{
    m_bufferMode = true;
    blockSignals(true);
    while (count() > 0) removeTab(0);
    m_tabIds.clear();

    int currentIdx = 0;
    for (int i = 0; i < buffers.size(); i++) {
        QVariantMap buf = buffers[i].toMap();
        int id = buf.value("id").toInt();
        QString name = buf.value("name", "[No Name]").toString();
        if (name.isEmpty()) name = "[No Name]";
        // Show just the filename
        int slash = name.lastIndexOf('/');
        if (slash >= 0) name = name.mid(slash + 1);
        bool modified = buf.value("modified", false).toBool();
        if (modified) name += " ●";
        addTab(name);
        m_tabIds.append(id);
        if (id == currentBuf) currentIdx = i;
    }

    setCurrentIndex(currentIdx);
    setVisible(m_tabIds.size() > 0);
    blockSignals(false);
}

// --- CmdlineWidget ---

CmdlineWidget::CmdlineWidget(const QString &accent, QWidget *parent)
    : QWidget(parent)
    , m_accent(accent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 4, 12, 4);
    m_label = new QLabel(this);
    m_label->setTextFormat(Qt::RichText);
    m_label->setStyleSheet("color: #c0caf5; font-family: monospace; font-size: 10pt;");
    layout->addWidget(m_label, 1);
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet("background-color: #1a1b26;");
    setFixedHeight(26);
    setVisible(false);
}

void CmdlineWidget::showCmdline(const QVariantList &content, int pos, const QString &firstc,
                                const QString &prompt, int indent, int level)
{
    Q_UNUSED(level)
    m_content.clear();
    for (const auto &chunk : content) {
        QVariantList c = chunk.toList();
        if (c.size() >= 2) m_content += c[1].toString();
    }
    m_firstc = firstc;
    m_prompt = prompt;
    m_indent = indent;
    m_bytePos = pos;
    m_special.clear();
    qCDebug(nbMessages).nospace() << "cmdline_show firstc=" << firstc
                                  << " pos=" << pos << " text=" << m_content;
    render();
    setVisible(true);
}

void CmdlineWidget::setPos(int pos, int level)
{
    Q_UNUSED(level)
    m_bytePos = pos;
    m_special.clear();
    render();
}

void CmdlineWidget::hideCmdline()
{
    m_special.clear();
    setVisible(false);
}

void CmdlineWidget::setSpecialChar(const QString &c, bool shift)
{
    Q_UNUSED(shift)
    // Pending-input placeholder (e.g. '"' after Ctrl-R): shown at the cursor
    // until the next cmdline_show/pos replaces it.
    m_special = c;
    render();
}

void CmdlineWidget::render()
{
    // The cursor position is a byte offset into the UTF-8 content; split
    // there, then re-decode so multibyte characters stay intact.
    const QByteArray utf8 = m_content.toUtf8();
    const int pos = qBound(0, m_bytePos, int(utf8.size()));
    const QString left = QString::fromUtf8(utf8.constData(), pos);
    QString rest = QString::fromUtf8(utf8.constData() + pos, utf8.size() - pos);

    // Block cursor: highlight the pending special char if any, else the
    // character under the cursor, else a trailing space.
    QString cursorCh = m_special;
    if (cursorCh.isEmpty()) {
        if (!rest.isEmpty()) {
            cursorCh = rest.left(1);
            rest.remove(0, 1);
        } else {
            cursorCh = QStringLiteral(" ");
        }
    }

    const auto esc = [](const QString &s) {
        QString e = s.toHtmlEscaped();
        e.replace(QStringLiteral(" "), QStringLiteral("&nbsp;"));
        return e;
    };
    QString html;
    html += QStringLiteral("<span style=\"color:%1;font-weight:bold;\">%2</span>")
                .arg(m_accent, esc(m_firstc + m_prompt));
    html += esc(QString(m_indent, QChar(' ')));
    html += esc(left);
    html += QStringLiteral("<span style=\"background-color:%1;color:#1a1b26;\">%2</span>")
                .arg(m_accent, esc(cursorCh));
    html += esc(rest);
    m_label->setText(html);
}

// --- MessageWidget ---

namespace {

/// Read-only log view; selecting text auto-copies it on release, mirroring
/// the statusline's SelectableLabel and the grid's behaviour.
class SelectableLogEdit : public QPlainTextEdit
{
public:
    explicit SelectableLogEdit(QWidget *parent = nullptr)
        : QPlainTextEdit(parent)
    {
        setReadOnly(true);
        setFocusPolicy(Qt::ClickFocus); // selectable, but Tab/typing stays with the grid
        setTextInteractionFlags(Qt::TextSelectableByMouse);
        setLineWrapMode(QPlainTextEdit::NoWrap);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFrameShape(QFrame::NoFrame);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QPlainTextEdit::mouseReleaseEvent(event);
        // QTextCursor encodes block breaks as U+2029; normalize for the clipboard.
        QString sel = textCursor().selectedText();
        sel.replace(QChar(0x2029), QChar('\n'));
        if (!sel.isEmpty())
            QGuiApplication::clipboard()->setText(sel);
    }
};

} // namespace

MessageWidget::MessageWidget(const QString &accent, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header: expand/collapse toggle on the left, mode indicator on the right.
    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(6, 2, 12, 2);
    headerLayout->setSpacing(0);

    m_toggle = new QPushButton(QStringLiteral("▸ Messages"), header);
    m_toggle->setCheckable(true);
    m_toggle->setFlat(true);
    m_toggle->setCursor(Qt::PointingHandCursor);
    m_toggle->setFocusPolicy(Qt::NoFocus);
    m_toggle->setStyleSheet(
        "QPushButton { color: #888; background: transparent; border: none; "
        "font-family: monospace; font-size: 9pt; padding: 0 4px; text-align: left; }"
        "QPushButton:hover { color: #c0caf5; }");
    headerLayout->addWidget(m_toggle);
    headerLayout->addStretch(1);

    m_modeLabel = new QLabel(header);
    m_modeLabel->setStyleSheet(QString("color: %1; font-family: monospace; font-size: 10pt;").arg(accent));
    headerLayout->addWidget(m_modeLabel);

    m_hideButton = new QPushButton(QStringLiteral("✕"), header);
    m_hideButton->setFlat(true);
    m_hideButton->setCursor(Qt::PointingHandCursor);
    m_hideButton->setFocusPolicy(Qt::NoFocus);
    m_hideButton->setToolTip(QStringLiteral("Hide messages panel (Ctrl+Shift+M to restore)"));
    m_hideButton->setStyleSheet(
        "QPushButton { color: #888; background: transparent; border: none; "
        "font-family: monospace; font-size: 9pt; padding: 0 4px; }"
        "QPushButton:hover { color: #f7768e; }");
    connect(m_hideButton, &QPushButton::clicked, this, &MessageWidget::hidePanel);
    headerLayout->addWidget(m_hideButton);
    layout->addWidget(header);

    m_log = new SelectableLogEdit(this);
    QFont logFont("monospace", 10);
    m_log->setFont(logFont);
    // Cap history so a chatty plugin cannot grow the document unbounded.
    m_log->setMaximumBlockCount(1000);
    m_log->setStyleSheet(QString(
        "QPlainTextEdit { background-color: #1a1b26; color: #c0caf5; "
        "selection-background-color: %1; selection-color: #1a1b26; }").arg(accent));
    layout->addWidget(m_log);

    setStyleSheet("background-color: #1a1b26;");

    connect(m_toggle, &QPushButton::toggled, this, [this](bool on) {
        m_expanded = on;
        m_toggle->setText(on ? QStringLiteral("▾ Messages") : QStringLiteral("▸ Messages"));
        emit aboutToResize(targetHeight());
        applyHeight();
    });
    applyHeight();
}

void MessageWidget::focusLog()
{
    if (!m_expanded)
        m_toggle->setChecked(true); // toggled() handler expands the log
    m_log->setFocus();
}

bool MessageWidget::logHasFocus() const
{
    return m_log->hasFocus();
}

void MessageWidget::collapse()
{
    m_toggle->setChecked(false); // toggled() handler collapses the log
}

void MessageWidget::hidePanel()
{
    emit aboutToResize(0);
    hide();
}

void MessageWidget::showPanel()
{
    emit aboutToResize(targetHeight());
    show();
}

int MessageWidget::targetHeight() const
{
    const int lines = m_expanded ? 12 : 2;
    const QFontMetrics fm(m_log->font());
    const int chrome = int(m_log->document()->documentMargin()) * 2;
    const int newLogHeight = fm.lineSpacing() * lines + chrome;
    // Current total minus current log height plus the log height it's about
    // to become: correct whether we're mid-toggle (m_expanded already
    // updated, geometry not yet) or resuming from hidden (both stale, so the
    // stale total passes through unchanged).
    return height() - m_log->height() + newLogHeight;
}

void MessageWidget::applyHeight()
{
    const int lines = m_expanded ? 12 : 2;
    const QFontMetrics fm(m_log->font());
    const int chrome = int(m_log->document()->documentMargin()) * 2;
    m_log->setFixedHeight(fm.lineSpacing() * lines + chrome);
    m_log->setVerticalScrollBarPolicy(m_expanded ? Qt::ScrollBarAsNeeded
                                                 : Qt::ScrollBarAlwaysOff);
    // Keep the newest entries in view after the viewport change.
    QScrollBar *sb = m_log->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void MessageWidget::showMsg(const QString &kind, const QVariantList &content, bool replaceLast)
{
    // Fires on every n/N/incsearch step; pure noise in a persistent log.
    if (kind == QLatin1String("search_count")) return;

    QString text;
    for (const auto &chunk : content) {
        QVariantList c = chunk.toList();
        if (c.size() >= 2) text += c[1].toString();
    }
    text.replace('\r', '\n'); // some vimscript messages separate lines with \r
    if (text.trimmed().isEmpty()) return;

    qCDebug(nbMessages).nospace() << "msg_show kind=" << kind
                                  << " replace_last=" << replaceLast << " text=" << text;

    static const QSet<QString> errorKinds = {
        QStringLiteral("emsg"), QStringLiteral("echoerr"), QStringLiteral("lua_error"),
        QStringLiteral("rpc_error"), QStringLiteral("shell_err")};
    QColor color("#c0caf5");
    if (errorKinds.contains(kind)) color = QColor("#f7768e");
    else if (kind == QLatin1String("wmsg")) color = QColor("#e0af68");

    QScrollBar *sb = m_log->verticalScrollBar();
    const bool atBottom = sb->value() >= sb->maximum();

    QTextCursor cur(m_log->document());
    cur.movePosition(QTextCursor::End);
    if (replaceLast && m_lastMsgPos >= 0 && m_lastMsgPos <= cur.position()) {
        cur.setPosition(m_lastMsgPos, QTextCursor::KeepAnchor);
        cur.removeSelectedText();
    }
    m_lastMsgPos = cur.position();
    if (cur.position() > 0) cur.insertBlock();

    QTextCharFormat stampFmt;
    stampFmt.setForeground(QColor("#565f89"));
    cur.insertText(QStringLiteral("[%1] ").arg(QTime::currentTime().toString("HH:mm:ss")),
                   stampFmt);
    QTextCharFormat msgFmt;
    msgFmt.setForeground(color);
    cur.insertText(text, msgFmt);

    // Follow the tail unless the user scrolled up to read history.
    if (atBottom) sb->setValue(sb->maximum());
}

void MessageWidget::clear()
{
    // msg_clear dismisses the active message in nvim's own UI; the log keeps
    // history, so only drop the replace_last anchor.
    m_lastMsgPos = -1;
}

void MessageWidget::showMode(const QVariantList &content)
{
    QString text;
    for (const auto &chunk : content) {
        QVariantList c = chunk.toList();
        if (c.size() >= 2) text += c[1].toString();
    }
    m_modeLabel->setText(text);
}

void MessageWidget::showRuler(const QVariantList &content)
{
    Q_UNUSED(content)
}

// --- StatuslineWidget ---

namespace {

/// A QLabel whose text can be selected with the mouse; on release the current
/// selection is auto-copied to the clipboard (mirrors the grid's behaviour).
class SelectableLabel : public QLabel
{
public:
    explicit SelectableLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        QFont f("monospace", 10);
        setFont(f);
        setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        setTextInteractionFlags(Qt::TextSelectableByMouse);
        setAutoFillBackground(true);
        // Show a text I-beam cursor to signal the text is selectable.
        setCursor(Qt::IBeamCursor);
        // Reproduce the old " text " padding around each segment.
        setContentsMargins(6, 0, 6, 0);
    }

    /// Make the segment clickable: a left release that selected no text runs
    /// the handler (the git segment opens the branch switcher with it).
    /// Selecting the text still works and takes precedence, so the two
    /// gestures do not collide. Passing an empty handler turns it back off.
    void setClickHandler(std::function<void()> handler)
    {
        m_click = std::move(handler);
        setCursor(m_click ? Qt::PointingHandCursor : Qt::IBeamCursor);
    }

    /// Apply foreground/background colours and boldness without a stylesheet
    /// re-parse (cheaper on the ~100ms refresh cadence).
    void setColors(const QColor &fg, const QColor &bg, bool bold)
    {
        QPalette pal = palette();
        pal.setColor(QPalette::WindowText, fg);
        pal.setColor(QPalette::Window, bg);
        setPalette(pal);
        QFont f = font();
        f.setBold(bold);
        setFont(f);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QLabel::mouseReleaseEvent(event);
        const QString sel = selectedText();
        if (!sel.isEmpty()) {
            QGuiApplication::clipboard()->setText(sel);
            return;
        }
        if (m_click && event->button() == Qt::LeftButton)
            m_click();
    }

private:
    std::function<void()> m_click;
};

/// Custom-painted powerline divider: a right-pointing triangle in the previous
/// segment's colour over the next segment's background.
class PowerlineDivider : public QWidget
{
public:
    explicit PowerlineDivider(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedWidth(12);
    }

    void setColors(const QColor &prevBg, const QColor &nextBg)
    {
        m_prevBg = prevBg;
        m_nextBg = nextBg;
        update();
    }

protected:
    void paintEvent(QPaintEvent * /*event*/) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), m_nextBg);
        QPainterPath tri;
        tri.moveTo(0, 0);
        tri.lineTo(width(), height() / 2.0);
        tri.lineTo(0, height());
        tri.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(m_prevBg);
        p.drawPath(tri);
    }

private:
    QColor m_prevBg{"#1a1b26"};
    QColor m_nextBg{"#1a1b26"};
};

} // namespace

StatuslineWidget::StatuslineWidget(const QString &accent, const QString &dividerStyle,
                                   QWidget *parent)
    : QWidget(parent)
    , m_dividerStyle(dividerStyle)
    , m_accent(accent)
{
    setFixedHeight(28);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Segment slots in display order; the file slot stretches to fill.
    QLabel **labels[] = {&m_modeLabel, &m_branchLabel, &m_fileLabel,
                         &m_diagLabel, &m_encLabel, &m_posLabel};
    const int slotCount = static_cast<int>(sizeof(labels) / sizeof(labels[0]));

    for (int i = 0; i < slotCount; ++i) {
        auto *label = new SelectableLabel(this);
        *labels[i] = label;
        layout->addWidget(label);

        // One trailing divider per slot; the last visible slot's divider is
        // always hidden by refresh().
        QWidget *divider = nullptr;
        if (m_dividerStyle == "powerline") {
            divider = new PowerlineDivider(this);
        } else if (m_dividerStyle == "pipe") {
            auto *pipe = new QLabel(QString(QChar(0x2502)), this);
            pipe->setFont(QFont("monospace", 10, QFont::Bold));
            pipe->setAlignment(Qt::AlignCenter);
            pipe->setContentsMargins(2, 0, 2, 0);
            pipe->setStyleSheet("color: #565f89; background-color: #1a1b26;");
            divider = pipe;
        }
        if (divider) {
            layout->addWidget(divider);
            divider->hide();
        }
        m_dividers.append(divider);
    }
    // The file segment stretches to fill available width.
    layout->setStretchFactor(m_fileLabel, 1);

    // The branch segment mixes two colours (branch name + accent-coloured git
    // counts), so it renders rich text. Explicit, because Qt's auto-detection
    // would not recognise a "&nbsp;"-prefixed string as rich text.
    m_branchLabel->setTextFormat(Qt::RichText);

    refresh();
}

QColor StatuslineWidget::modeColor() const
{
    if (m_mode.startsWith("I", Qt::CaseInsensitive))
        return QColor("#c678dd"); // violet — Insert
    if (m_mode.startsWith("V", Qt::CaseInsensitive) ||
        m_mode.startsWith("S", Qt::CaseInsensitive) ||
        m_mode.contains("BLOCK", Qt::CaseInsensitive))
        return QColor("#56b6c2"); // cyan — Visual/Select
    if (m_mode.startsWith("C", Qt::CaseInsensitive))
        return QColor("#e5c07b"); // yellow — Command
    if (m_mode.startsWith("R", Qt::CaseInsensitive))
        return QColor("#f7768e"); // red — Replace
    return QColor(m_accent); // Normal
}

void StatuslineWidget::refresh()
{
    const QColor modeBg = modeColor();
    const QColor dark("#1a1b26");
    const QColor mid("#24283b");
    const QColor dim("#565f89");
    const QColor fg("#c0caf5");

    // Git segment: the branch name comes from the Qt-side GitStatus provider,
    // which runs `git` itself, so no nvim plugin is required. With
    // [ui] gitsigns_branch the gitsigns name (from statusline.lua) wins
    // instead when it is set. The counts always come from the provider.
    // Rendered as rich text so the counts can use the accent colour while
    // the branch name keeps the palette foreground; &nbsp; because rich
    // text trims plain leading/trailing spaces.
    const QString branchName = !m_branch.isEmpty()
        ? m_branch : (m_git.valid ? m_git.branch : QString());
    QString counts;
    if (m_git.valid && !branchName.isEmpty()) {
        if (m_git.staged)    counts += QString(" +%1").arg(m_git.staged);
        if (m_git.modified)  counts += QString(" ~%1").arg(m_git.modified);
        if (m_git.untracked) counts += QString(" ?%1").arg(m_git.untracked);
        if (m_git.ahead)     counts += QString(" ↑%1").arg(m_git.ahead);
        if (m_git.behind)    counts += QString(" ↓%1").arg(m_git.behind);
    }
    QString branchText;
    if (!branchName.isEmpty()) {
        branchText = QStringLiteral("&nbsp;") + branchName.toHtmlEscaped();
        if (!counts.isEmpty())
            branchText += QStringLiteral("<span style=\"color:%1;\">%2</span>")
                              .arg(m_accent, counts.toHtmlEscaped());
        branchText += QStringLiteral("&nbsp;");
    }

    // Hover detail for the git segment: full ahead/behind wording plus the
    // changed-file list, which has no room in the inline counts. Built here
    // from data GitStatus already fetched (no new git invocation) - cheap
    // string formatting, so no need to gate it behind a "did git status
    // change" check the way the git subprocess itself is.
    if (!branchName.isEmpty() && m_git.valid) {
        static constexpr int kMaxTooltipFiles = 20;
        static const QString kChangedFileColor   = QStringLiteral("#98c379"); // OneDark green
        static const QString kUntrackedFileColor = QStringLiteral("#61afef"); // OneDark blue,
            // same blue already used elsewhere in neobeam (e.g. the focus-border color)

        QString tip = QStringLiteral("<b>%1</b>").arg(branchName.toHtmlEscaped());
        if (m_git.ahead || m_git.behind) {
            QStringList parts;
            if (m_git.ahead)  parts << QStringLiteral("%1 ahead").arg(m_git.ahead);
            if (m_git.behind) parts << QStringLiteral("%1 behind").arg(m_git.behind);
            tip += QStringLiteral("<br>%1").arg(parts.join(QStringLiteral(", ")));
        } else {
            tip += QStringLiteral("<br>up to date with upstream");
        }

        const int totalFiles = m_git.changedFiles.size() + m_git.untrackedFiles.size();
        if (totalFiles > 0) {
            tip += QStringLiteral("<br><br>Changed files (%1):").arg(totalFiles);
            int shown = 0;
            // Tracked (staged/modified) first in green, then untracked in blue,
            // each capped so the combined list never exceeds kMaxTooltipFiles.
            for (const QString &file : m_git.changedFiles) {
                if (shown >= kMaxTooltipFiles) break;
                tip += QStringLiteral("<br><span style=\"color:%1;\">%2</span>")
                           .arg(kChangedFileColor, file.toHtmlEscaped());
                ++shown;
            }
            for (const QString &file : m_git.untrackedFiles) {
                if (shown >= kMaxTooltipFiles) break;
                tip += QStringLiteral("<br><span style=\"color:%1;\">%2</span>")
                           .arg(kUntrackedFileColor, file.toHtmlEscaped());
                ++shown;
            }
            const int overflow = totalFiles - shown;
            if (overflow > 0)
                tip += QStringLiteral("<br>...and %1 more files").arg(overflow);
        }
        if (m_gitClickable)
            tip += QStringLiteral("<br><br><i>Click to switch branches</i>");
        m_branchLabel->setToolTip(tip);
    } else {
        m_branchLabel->setToolTip(QString());
    }

    // Configure each slot: text, colours, visibility. Order must match the
    // layout slot order built in the constructor.
    struct Slot { QLabel *label; QString text; QColor fg; QColor bg; bool bold; bool visible; };
    const Slot cfg[] = {
        {m_modeLabel,   " " + m_mode + " ",                                    dark,              modeBg, true,  true},
        {m_branchLabel, branchText,                                            fg,                mid,    false, !branchText.isEmpty()},
        {m_fileLabel,   " " + (m_file.isEmpty() ? "[No Name]" : m_file) + " ", fg,                dark,   false, true},
        {m_diagLabel,   " " + m_diag + " ",                                    QColor("#f7768e"), mid,    false, !m_diag.isEmpty()},
        {m_encLabel,    " " + m_encoding + " ",                                dim,               mid,    false, !m_encoding.isEmpty()},
        {m_posLabel,    " " + m_pos + " ",                                     dark,              modeBg, false, !m_pos.isEmpty()},
    };
    const int slotCount = static_cast<int>(sizeof(cfg) / sizeof(cfg[0]));

    for (int i = 0; i < slotCount; ++i) {
        cfg[i].label->setText(cfg[i].text);
        static_cast<SelectableLabel *>(cfg[i].label)->setColors(cfg[i].fg, cfg[i].bg, cfg[i].bold);
        cfg[i].label->setVisible(cfg[i].visible);
    }

    // Assign dividers: a slot's trailing divider is shown only between it and
    // the next visible slot. Walk left→right tracking the last visible slot.
    int prev = -1;
    for (int i = 0; i < slotCount; ++i) {
        if (!cfg[i].visible) {
            if (m_dividers[i]) m_dividers[i]->hide();
            continue;
        }
        if (prev >= 0 && m_dividers[prev]) {
            QWidget *pdiv = m_dividers[prev];
            // Only powerline dividers are colour-dependent; pipe dividers are
            // plain labels. m_dividers only ever holds PowerlineDivider when
            // the style is "powerline", so the cast is safe.
            if (m_dividerStyle == "powerline")
                static_cast<PowerlineDivider *>(pdiv)->setColors(cfg[prev].bg, cfg[i].bg);
            pdiv->show();
        }
        prev = i;
    }
    // The last visible slot has no following segment — hide its divider.
    if (prev >= 0 && m_dividers[prev])
        m_dividers[prev]->hide();
}

void StatuslineWidget::updateGit(const GitStatusInfo &info)
{
    m_git = info;
    refresh();
}

void StatuslineWidget::setMode(const QString &mode)
{
    m_mode = mode.toUpper();
    refresh();
}

void StatuslineWidget::setGitSegmentClickable(bool clickable)
{
    if (m_gitClickable == clickable)
        return;
    m_gitClickable = clickable;
    auto *label = static_cast<SelectableLabel *>(m_branchLabel);
    if (clickable)
        label->setClickHandler([this]() { emit gitSegmentClicked(); });
    else
        label->setClickHandler({});
    refresh();   // the tooltip gains/loses its "click to switch branches" hint
}

void StatuslineWidget::setGitsignsBranch(bool enabled)
{
    if (m_gitsignsBranch == enabled)
        return;
    m_gitsignsBranch = enabled;
    if (!enabled)
        m_branch.clear(); // stop a stale gitsigns name from masking the provider
    refresh();
}

void StatuslineWidget::updateStatus(const QVariantMap &data)
{
    if (data.contains("mode")) m_mode = data["mode"].toString().toUpper();
    if (data.contains("file")) m_file = data["file"].toString();
    if (m_gitsignsBranch && data.contains("branch")) m_branch = data["branch"].toString();
    if (data.contains("pos")) m_pos = data["pos"].toString();
    if (data.contains("encoding")) m_encoding = data["encoding"].toString();
    if (data.contains("diagnostics")) m_diag = data["diagnostics"].toString();
    refresh();
}
