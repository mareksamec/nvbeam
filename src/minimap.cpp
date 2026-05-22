#include "minimap.h"
#include <QPainter>
#include <QMouseEvent>

MinimapWidget::MinimapWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(kMinWidth);
    setMouseTracking(true);
    setStyleSheet("background: #1a1b26;");
}

QSize MinimapWidget::sizeHint() const
{
    return QSize(kMinWidth, 400);
}

void MinimapWidget::setContent(const QVector<MinimapLine> &lines, int totalLines)
{
    m_lines = lines;
    m_totalLines = totalLines;
    m_contentDirty = true;
    m_overlayDirty = true;
    update();
}

void MinimapWidget::setViewport(int topLine, int visibleLines)
{
    if (m_topLine == topLine && m_visibleLines == visibleLines)
        return;
    m_topLine = topLine;
    m_visibleLines = visibleLines;
    m_overlayDirty = true;
    update();
}

void MinimapWidget::setCursorLine(int line)
{
    if (m_cursorLine == line)
        return;
    m_cursorLine = line;
    m_overlayDirty = true;
    update();
}

void MinimapWidget::rebuildImage()
{
    int w = width();
    int h = height();
    if (w <= 0 || h <= 0)
        return;

    m_cache = QImage(w, h, QImage::Format_RGB32);
    m_cache.fill(QColor(0x1a, 0x1b, 0x26));

    if (m_lines.isEmpty())
        return;

    int maxVisibleLines = h / kLineHeight;
    int startLine = 0;
    if (m_totalLines > maxVisibleLines) {
        startLine = m_topLine - maxVisibleLines / 4;
        if (startLine < 0) startLine = 0;
        if (startLine + maxVisibleLines > m_totalLines)
            startLine = m_totalLines - maxVisibleLines;
    }

    int endLine = qMin(startLine + maxVisibleLines, m_lines.size());
    for (int i = startLine; i < endLine; ++i) {
        int y = (i - startLine) * kLineHeight;
        const auto &line = m_lines[i];
        int maxChars = qMin(line.colors.size(), w / kCharWidth);
        for (int c = 0; c < maxChars; ++c) {
            if (line.colors[c].isValid()) {
                QRgb rgb = line.colors[c].rgb();
                int x = c * kCharWidth;
                for (int py = y; py < y + kLineHeight; ++py) {
                    auto *scanline = reinterpret_cast<QRgb *>(m_cache.scanLine(py));
                    for (int px = x; px < x + kCharWidth; ++px)
                        scanline[px] = rgb;
                }
            }
        }
    }

    m_contentDirty = false;
}

void MinimapWidget::paintEvent(QPaintEvent * /*event*/)
{
    if (m_contentDirty || m_cache.size() != size())
        rebuildImage();

    QPainter p(this);
    p.drawImage(0, 0, m_cache);

    if (m_lines.isEmpty())
        return;

    // Compute startLine (same logic as rebuildImage)
    int maxVisibleLines = height() / kLineHeight;
    int startLine = 0;
    if (m_totalLines > maxVisibleLines) {
        startLine = m_topLine - maxVisibleLines / 4;
        if (startLine < 0) startLine = 0;
        if (startLine + maxVisibleLines > m_totalLines)
            startLine = m_totalLines - maxVisibleLines;
    }

    // Viewport indicator
    int vpY = (m_topLine - startLine) * kLineHeight;
    int vpH = m_visibleLines * kLineHeight;
    p.fillRect(0, vpY, width(), vpH, QColor(255, 255, 255, 20));

    // Cursor line indicator
    int cursorY = (m_cursorLine - startLine) * kLineHeight;
    if (cursorY >= 0 && cursorY < height())
        p.fillRect(0, cursorY, width(), kLineHeight, QColor(255, 255, 255, 40));

    m_overlayDirty = false;
}

void MinimapWidget::mousePressEvent(QMouseEvent *event)
{
    emit lineClicked(lineAtY(event->pos().y()));
}

void MinimapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        emit lineClicked(lineAtY(event->pos().y()));
}

int MinimapWidget::lineAtY(int y) const
{
    int maxVisibleLines = height() / kLineHeight;
    int startLine = 0;
    if (m_totalLines > maxVisibleLines) {
        startLine = m_topLine - maxVisibleLines / 4;
        if (startLine < 0) startLine = 0;
        if (startLine + maxVisibleLines > m_totalLines)
            startLine = m_totalLines - maxVisibleLines;
    }
    int line = startLine + y / kLineHeight;
    return qBound(0, line, m_totalLines - 1);
}
