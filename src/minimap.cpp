#include "minimap.h"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QSize>

static constexpr QRgb kBgColor = qRgb(0x1a, 0x1b, 0x26);

MinimapWidget::MinimapWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(kMinWidth);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    m_rebuildTimer = new QTimer(this);
    m_rebuildTimer->setSingleShot(true);
    m_rebuildTimer->setInterval(0);
    connect(m_rebuildTimer, &QTimer::timeout, this, &MinimapWidget::rebuildPixmap);
}

QSize MinimapWidget::sizeHint() const
{
    return QSize(kMinWidth, 400);
}

void MinimapWidget::updateContent(const QVector<MinimapLine> &lines, int totalLines,
                                  int topLine, int visibleLines, int cursorLine)
{
    bool contentChanged = (m_lines != lines || m_totalLines != totalLines);
    bool viewportChanged = (m_topLine != topLine || m_visibleLines != visibleLines || m_cursorLine != cursorLine);

    if (!contentChanged && !viewportChanged)
        return;

    m_topLine = topLine;
    m_visibleLines = visibleLines;
    m_cursorLine = cursorLine;

    if (contentChanged) {
        m_lines = lines;
        m_totalLines = totalLines;
        rebuildPixmap();
    } else {
        update(); // overlay only
    }
}

void MinimapWidget::setViewport(int topLine, int visibleLines, int cursorLine)
{
    if (m_topLine == topLine && m_visibleLines == visibleLines && m_cursorLine == cursorLine)
        return;
    m_topLine = topLine;
    m_visibleLines = visibleLines;
    m_cursorLine = cursorLine;
    update(); // only overlay changes — no pixmap rebuild
}

void MinimapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_rebuildTimer->isActive())
        m_rebuildTimer->start();
}

void MinimapWidget::rebuildPixmap()
{
    m_rebuildTimer->stop();

    const int w = width();
    const int h = height();
    if (w <= 0 || h <= 0) return;

    const qreal dpr = devicePixelRatioF();
    QPixmap pix(w * dpr, h * dpr);
    pix.setDevicePixelRatio(dpr);
    pix.fill(QColor::fromRgb(kBgColor));

    const int srcCount = m_lines.size(); // actual lines available (may be capped, e.g. 2000)
    // virtualLines is what we spread across the widget — the full file length so the overlay
    // coordinate system (which uses m_totalLines) stays consistent with the pixmap.
    const int virtualLines = m_totalLines > 0 ? m_totalLines : srcCount;

    if (srcCount > 0 && virtualLines > 0) {
        const int maxLinesInWidget = h / kLineHeight;
        const int lineIncrement = virtualLines > maxLinesInWidget
            ? (virtualLines + maxLinesInWidget - 1) / maxLinesInWidget
            : 1;
        const int drawnLineCount = qMin(maxLinesInWidget, (virtualLines + lineIncrement - 1) / lineIncrement);

        QPainter pp(&pix);
        for (int i = 0; i < drawnLineCount; ++i) {
            const int srcLine = i * lineIncrement;
            const int y = i * kLineHeight;
            if (srcLine >= srcCount) {
                // No highlight data for this region — draw a subtle placeholder so it's not black.
                pp.fillRect(0, y, w, kLineHeight, QColor(0x2a, 0x2b, 0x3a));
                continue;
            }
            const auto &line = m_lines[srcLine];
            const int maxChars = qMin(line.colors.size(), w / kCharWidth);
            for (int c = 0; c < maxChars; ++c) {
                if (line.colors[c].isValid())
                    pp.fillRect(c * kCharWidth, y, kCharWidth, kLineHeight, line.colors[c]);
            }
        }
        pp.end();
    }

    m_pixmap = pix;
    update();
}

void MinimapWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    if (!m_pixmap.isNull()) {
        p.drawPixmap(0, 0, m_pixmap);
    } else {
        p.fillRect(rect(), QColor::fromRgb(kBgColor));
    }

    if (m_lines.isEmpty() || m_totalLines <= 0)
        return;

    const int h = height();

    auto lineToY = [&](int line) -> int {
        return (int)((qint64)line * h / m_totalLines);
    };

    // Viewport indicator: height is the fraction of the file visible in the editor
    int vpY = lineToY(m_topLine);
    int vpH = qMax((int)(h * (float)m_visibleLines / m_totalLines), kLineHeight);
    p.fillRect(0, vpY, width(), vpH, QColor(255, 255, 255, 25));

    // Cursor line
    int cursorY = lineToY(m_cursorLine);
    if (cursorY >= 0 && cursorY < h)
        p.fillRect(0, cursorY, width(), qMax(kLineHeight, h / m_totalLines), QColor(255, 255, 255, 50));
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
    if (m_totalLines <= 0)
        return 0;
    const int h = height();
    return qBound(0, (int)((qint64)y * m_totalLines / h), m_totalLines - 1);
}
