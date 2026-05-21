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
    update();
}

void MinimapWidget::setViewport(int topLine, int visibleLines)
{
    m_topLine = topLine;
    m_visibleLines = visibleLines;
    update();
}

void MinimapWidget::setCursorLine(int line)
{
    m_cursorLine = line;
    update();
}

void MinimapWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x1a, 0x1b, 0x26));

    if (m_lines.isEmpty())
        return;

    int maxVisibleLines = height() / kLineHeight;
    int startLine = 0;

    // Center the viewport indicator if buffer is larger than widget
    if (m_totalLines > maxVisibleLines) {
        startLine = m_topLine - maxVisibleLines / 4;
        if (startLine < 0) startLine = 0;
        if (startLine + maxVisibleLines > m_totalLines)
            startLine = m_totalLines - maxVisibleLines;
    }

    // Draw viewport indicator (semi-transparent highlight)
    int vpY = (m_topLine - startLine) * kLineHeight;
    int vpH = m_visibleLines * kLineHeight;
    p.fillRect(0, vpY, width(), vpH, QColor(255, 255, 255, 20));

    // Draw lines as colored pixels
    int endLine = qMin(startLine + maxVisibleLines, m_lines.size());
    for (int i = startLine; i < endLine; ++i) {
        int y = (i - startLine) * kLineHeight;
        const auto &line = m_lines[i];
        int maxChars = qMin(line.colors.size(), width() / kCharWidth);
        for (int c = 0; c < maxChars; ++c) {
            if (line.colors[c].isValid()) {
                p.fillRect(c * kCharWidth, y, kCharWidth, kLineHeight, line.colors[c]);
            }
        }
    }

    // Draw cursor line indicator
    int cursorY = (m_cursorLine - startLine) * kLineHeight;
    if (cursorY >= 0 && cursorY < height()) {
        p.fillRect(0, cursorY, width(), kLineHeight, QColor(255, 255, 255, 40));
    }
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
