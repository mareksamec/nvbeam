#include "uiwidgets.h"
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QPainterPath>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QClipboard>

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
    Q_UNUSED(grid)
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

    move(col * cellWidth, (row + 1) * cellHeight);
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

// --- MessageWidget ---

MessageWidget::MessageWidget(const QString &accent, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 4, 12, 4);
    m_label = new QLabel(this);
    m_label->setStyleSheet("color: #c0caf5; font-family: monospace; font-size: 10pt;");
    m_modeLabel = new QLabel(this);
    m_modeLabel->setStyleSheet(QString("color: %1; font-family: monospace; font-size: 10pt;").arg(accent));
    layout->addWidget(m_label, 1);
    layout->addWidget(m_modeLabel);
    setStyleSheet("background-color: #1a1b26;");
    setFixedHeight(26);
    setVisible(false);
}

void MessageWidget::showMsg(const QString &kind, const QVariantList &content, bool replaceLast)
{
    Q_UNUSED(kind) Q_UNUSED(replaceLast)
    QString text;
    for (const auto &chunk : content) {
        QVariantList c = chunk.toList();
        if (c.size() >= 2) text += c[1].toString();
    }
    m_label->setText(text);
    setVisible(!text.isEmpty());
}

void MessageWidget::clear()
{
    m_label->clear();
    setVisible(false);
}

void MessageWidget::showMode(const QVariantList &content)
{
    QString text;
    for (const auto &chunk : content) {
        QVariantList c = chunk.toList();
        if (c.size() >= 2) text += c[1].toString();
    }
    m_modeLabel->setText(text);
    if (!text.isEmpty()) setVisible(true);
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
        if (!sel.isEmpty())
            QGuiApplication::clipboard()->setText(sel);
    }
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

    // Configure each slot: text, colours, visibility. Order must match the
    // layout slot order built in the constructor.
    struct Slot { QLabel *label; QString text; QColor fg; QColor bg; bool bold; bool visible; };
    const Slot cfg[] = {
        {m_modeLabel,   " " + m_mode + " ",                                    dark,              modeBg, true,  true},
        {m_branchLabel, " " + m_branch + " ",                                  fg,                mid,    false, !m_branch.isEmpty()},
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

void StatuslineWidget::setMode(const QString &mode)
{
    m_mode = mode.toUpper();
    refresh();
}

void StatuslineWidget::updateStatus(const QVariantMap &data)
{
    if (data.contains("mode")) m_mode = data["mode"].toString().toUpper();
    if (data.contains("file")) m_file = data["file"].toString();
    if (data.contains("branch")) m_branch = data["branch"].toString();
    if (data.contains("pos")) m_pos = data["pos"].toString();
    if (data.contains("encoding")) m_encoding = data["encoding"].toString();
    if (data.contains("diagnostics")) m_diag = data["diagnostics"].toString();
    refresh();
}
