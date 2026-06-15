#include "uiwidgets.h"
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QPainterPath>

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

StatuslineWidget::StatuslineWidget(const QString &accent, const QString &dividerStyle,
                                   QWidget *parent)
    : QWidget(parent)
    , m_dividerStyle(dividerStyle)
    , m_accent(accent)
{
    setFixedHeight(28);
    rebuildSegments();
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

void StatuslineWidget::rebuildSegments()
{
    m_segments.clear();
    QColor modeBg = modeColor();
    QColor dark("#1a1b26");
    QColor mid("#24283b");
    QColor dim("#565f89");

    // Mode segment
    m_segments.append({" " + m_mode + " ", dark, modeBg});

    // Branch segment
    if (!m_branch.isEmpty())
        m_segments.append({" " + m_branch + " ", QColor("#c0caf5"), mid});

    // File segment (this one stretches — index tracked below)
    m_stretchIndex = m_segments.size();
    m_segments.append({" " + (m_file.isEmpty() ? "[No Name]" : m_file) + " ",
                       QColor("#c0caf5"), dark});

    // Diagnostics
    if (!m_diag.isEmpty())
        m_segments.append({" " + m_diag + " ", QColor("#f7768e"), mid});

    // Encoding
    if (!m_encoding.isEmpty())
        m_segments.append({" " + m_encoding + " ", dim, mid});

    // Position
    if (!m_pos.isEmpty())
        m_segments.append({" " + m_pos + " ", dark, modeBg});
}

void StatuslineWidget::setMode(const QString &mode)
{
    m_mode = mode.toUpper();
    rebuildSegments();
    update();
}

void StatuslineWidget::updateStatus(const QVariantMap &data)
{
    if (data.contains("mode")) m_mode = data["mode"].toString().toUpper();
    if (data.contains("file")) m_file = data["file"].toString();
    if (data.contains("branch")) m_branch = data["branch"].toString();
    if (data.contains("pos")) m_pos = data["pos"].toString();
    if (data.contains("encoding")) m_encoding = data["encoding"].toString();
    if (data.contains("diagnostics")) m_diag = data["diagnostics"].toString();
    rebuildSegments();
    update();
}

void StatuslineWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QFont font("monospace", 10);
    font.setBold(true);
    p.setFont(font);
    QFontMetrics fm(font);

    QColor bgDefault("#1a1b26");
    p.fillRect(rect(), bgDefault);

    if (m_segments.isEmpty()) return;

    // Calculate segment widths; the file segment (index 2) stretches
    int divW = (m_dividerStyle == "powerline") ? 12 : 0;
    if (m_dividerStyle == "pipe") divW = fm.horizontalAdvance("|") + 4;

    QVector<int> widths(m_segments.size());
    int fixedWidth = 0;
    for (int i = 0; i < m_segments.size(); ++i) {
        widths[i] = fm.horizontalAdvance(m_segments[i].text) + 4;
        if (i != m_stretchIndex) fixedWidth += widths[i];
    }
    fixedWidth += divW * (m_segments.size() - 1);
    widths[m_stretchIndex] = qMax(widths[m_stretchIndex], width() - fixedWidth);

    int x = 0;
    for (int i = 0; i < m_segments.size(); ++i) {
        const auto &seg = m_segments[i];
        int w = widths[i];

        // Draw segment background
        p.fillRect(x, 0, w, height(), seg.bg);

        // Draw text
        p.setPen(seg.fg);
        QFont textFont = font;
        textFont.setBold(i == 0);
        p.setFont(textFont);
        p.drawText(x, 0, w, height(), Qt::AlignVCenter | Qt::AlignLeft, seg.text);
        x += w;

        // Draw divider between segments
        if (i < m_segments.size() - 1 && divW > 0) {
            QColor prevBg = seg.bg;
            QColor nextBg = m_segments[i + 1].bg;

            if (m_dividerStyle == "powerline") {
                // Fill divider area with next segment's bg
                p.fillRect(x, 0, divW, height(), nextBg);
                // Draw triangle pointing right with prev segment's color
                QPainterPath tri;
                tri.moveTo(x, 0);
                tri.lineTo(x + divW, height() / 2.0);
                tri.lineTo(x, height());
                tri.closeSubpath();
                p.setPen(Qt::NoPen);
                p.setBrush(prevBg);
                p.drawPath(tri);
            } else if (m_dividerStyle == "pipe") {
                p.fillRect(x, 0, divW, height(), nextBg);
                p.setPen(QColor("#565f89"));
                p.setFont(font);
                p.drawText(x, 0, divW, height(), Qt::AlignCenter, QStringLiteral("\u2502"));
            }
            x += divW;
        }
    }
}
