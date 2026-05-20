#include "uiwidgets.h"
#include <QHBoxLayout>

// --- CmdlineWidget ---

CmdlineWidget::CmdlineWidget(const QString &accent, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 6, 12, 6);
    m_label = new QLabel(this);
    m_label->setStyleSheet(QString("color: %1; font-family: monospace; font-size: 11pt;").arg(accent));
    layout->addWidget(m_label);
    setStyleSheet(QString("background-color: #1a1b26; border-top: 2px solid %1;").arg(accent));
    setFixedHeight(32);
    setVisible(false);
}

void CmdlineWidget::show(const QVariantList &content, int pos, const QString &firstc,
                          const QString &prompt, int indent, int level)
{
    Q_UNUSED(pos) Q_UNUSED(indent) Q_UNUSED(level)
    QString text = prompt + firstc;
    for (const auto &chunk : content) {
        QVariantList c = chunk.toList();
        if (c.size() >= 2) text += c[1].toString();
    }
    m_label->setText(text);
    setVisible(true);
}

void CmdlineWidget::hide(int level)
{
    Q_UNUSED(level)
    setVisible(false);
}

void CmdlineWidget::setPos(int pos, int level)
{
    Q_UNUSED(pos) Q_UNUSED(level)
}

// --- PopupmenuWidget ---

PopupmenuWidget::PopupmenuWidget(const QString &accent, QWidget *parent)
    : QWidget(parent)
    , m_accent(accent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_list = new QListWidget(this);
    m_list->setStyleSheet(QString(
        "QListWidget { background-color: #1a1b26; color: #c0caf5; border: 1px solid %1; "
        "font-family: monospace; font-size: 10pt; border-radius: 4px; }"
        "QListWidget::item { padding: 2px 8px; }"
        "QListWidget::item:selected { background-color: %1; color: #1a1b26; }"
    ).arg(accent));
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    layout->addWidget(m_list);
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
