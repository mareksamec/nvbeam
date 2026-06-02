#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QPixmap>

class QTimer;

/// Minimap data for a single line: per-character highlight colors
struct MinimapLine {
    QVector<QColor> colors; // one color per character
    bool operator==(const MinimapLine &o) const { return colors == o.colors; }
};

/// Native VSCode-like minimap widget showing zoomed-out buffer content.
/// Uses Kate/KTextEditor's approach: pixmap rebuilt asynchronously,
/// paintEvent only blits + draws overlay. No rebuild during paint.
class MinimapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MinimapWidget(QWidget *parent = nullptr);

    /// Full content + viewport update (schedules pixmap rebuild)
    void updateContent(const QVector<MinimapLine> &lines, int totalLines,
                       int topLine, int visibleLines, int cursorLine);

    /// Lightweight viewport-only update (overlay repaint only)
    void setViewport(int topLine, int visibleLines, int cursorLine);

    QSize sizeHint() const override;

signals:
    void lineClicked(int line);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void rebuildPixmap();

private:
    int lineAtY(int y) const;

    QVector<MinimapLine> m_lines;
    int m_totalLines = 0;
    int m_topLine = 0;
    int m_visibleLines = 0;
    int m_cursorLine = 0;

    QPixmap m_pixmap;
    QTimer *m_rebuildTimer = nullptr;

    static constexpr int kCharWidth = 2;
    static constexpr int kLineHeight = 2;
    static constexpr int kMinWidth = 80;
};
