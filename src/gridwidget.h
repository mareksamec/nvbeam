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
    QColor foreground = Qt::white;
    QColor background = Qt::black;
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

public slots:
    /// Process redraw events from Neovim
    void handleRedraw(const QVariantList &events);

signals:
    /// Emitted when the widget knows its grid size (after first paint)
    void gridSizeReady(int rows, int cols);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void processEvent(const QString &name, const QVariantList &args);
    void handleGridResize(const QVariantList &args);
    void handleGridLine(const QVariantList &args);
    void handleGridClear(const QVariantList &args);
    void handleGridCursorGoto(const QVariantList &args);
    void handleGridScroll(const QVariantList &args);
    void handleHlAttrDefine(const QVariantList &args);
    void handleDefaultColorsSet(const QVariantList &args);
    void recalculateGrid();

    QFont m_font;
    int m_cellWidth = 0;
    int m_cellHeight = 0;
    int m_rows = 0;
    int m_cols = 0;
    int m_cursorRow = 0;
    int m_cursorCol = 0;

    QVector<QVector<GridCell>> m_grid;
    QHash<int, HlAttr> m_hlAttrs;
    QColor m_defaultFg = Qt::white;
    QColor m_defaultBg = QColor(30, 30, 30);
};
