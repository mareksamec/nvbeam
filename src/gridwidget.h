#pragma once

#include <QWidget>
#include <QColor>
#include <QFont>
#include <QVector>
#include <QHash>

/// A single cell in the character grid
struct GridCell {
    QString text = " ";
    int hlId = 0;
};

/// Highlight attributes from Neovim
struct HlAttr {
    QColor foreground;
    QColor background;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool reverse = false;
};

/// Widget that renders the Neovim grid using QPainter
class GridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GridWidget(QWidget *parent = nullptr);

    /// Returns grid dimensions in cells
    int gridRows() const { return m_rows; }
    int gridCols() const { return m_cols; }
    int cellWidth() const { return m_cellWidth; }
    int cellHeight() const { return m_cellHeight; }

public slots:
    /// Process redraw events from Neovim
    void handleRedraw(const QVariantList &events);

signals:
    /// Emitted when the widget knows its grid size (after first paint)
    void gridSizeReady(int rows, int cols);

    /// Emitted when a key should be sent to Neovim
    void inputKey(const QString &keys);

    /// Emitted on mouse events
    void inputMouse(const QString &button, const QString &action,
                    const QString &modifier, int grid, int row, int col);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void processEvent(const QString &name, const QVariantList &args);
    void handleGridResize(const QVariantList &args);
    void handleGridLine(const QVariantList &args);
    void handleGridClear(const QVariantList &args);
    void handleGridCursorGoto(const QVariantList &args);
    void handleGridScroll(const QVariantList &args);
    void handleHlAttrDefine(const QVariantList &args);
    void handleDefaultColorsSet(const QVariantList &args);
    void handleModeInfoSet(const QVariantList &args);
    void handleModeChange(const QVariantList &args);
    void recalculateGrid();
    QString keyToNvim(QKeyEvent *event);
    QString modifierPrefix(Qt::KeyboardModifiers mods, bool isSpecial);
    QPair<int,int> cellAt(const QPoint &pos);

    QFont m_font;
    int m_cellWidth = 0;
    int m_cellHeight = 0;
    int m_rows = 0;
    int m_cols = 0;
    int m_cursorRow = 0;
    int m_cursorCol = 0;

    // Cursor mode info
    struct ModeInfo {
        QString cursorShape = "block"; // block, horizontal, vertical
        int cellPercentage = 0;
    };
    QVector<ModeInfo> m_modeInfoList;
    int m_currentMode = 0;
    bool m_busy = false;

    // Mouse state
    bool m_mousePressed = false;

    QVector<QVector<GridCell>> m_grid;
    QHash<int, HlAttr> m_hlAttrs;
    QColor m_defaultFg = Qt::white;
    QColor m_defaultBg = QColor(30, 30, 30);
};
