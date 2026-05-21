#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QImage>
#include <QString>

/// Minimap data for a single line: per-character highlight colors
struct MinimapLine {
    QVector<QColor> colors; // one color per character
};

/// Native VSCode-like minimap widget showing zoomed-out buffer content
class MinimapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MinimapWidget(QWidget *parent = nullptr);

    /// Update minimap content from buffer lines and highlight data
    void setContent(const QVector<MinimapLine> &lines, int totalLines);

    /// Set the visible viewport range (top line, visible count)
    void setViewport(int topLine, int visibleLines);

    /// Set the cursor line for indicator
    void setCursorLine(int line);

    QSize sizeHint() const override;

signals:
    /// Emitted when user clicks minimap to scroll to a line
    void lineClicked(int line);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    int lineAtY(int y) const;

    QVector<MinimapLine> m_lines;
    int m_totalLines = 0;
    int m_topLine = 0;
    int m_visibleLines = 0;
    int m_cursorLine = 0;

    static constexpr int kCharWidth = 2;   // pixels per character
    static constexpr int kLineHeight = 2;  // pixels per line
    static constexpr int kMinWidth = 80;   // minimum widget width in pixels
};
