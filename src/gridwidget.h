#pragma once

#include <QWidget>
#include <QColor>
#include <QFont>
#include <QVector>
#include <QHash>
#include <QTimer>
#include <QVariant>

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

/// A single Neovim grid (one per window in ext_multigrid)
struct Grid {
    int id = 0;
    int rows = 0;
    int cols = 0;
    int x = 0;       // position in cell coords on the screen
    int y = 0;
    int zindex = 0;
    bool hidden = false;
    bool isFloat = false;
    QVector<QVector<GridCell>> cells;

    void resize(int r, int c) {
        rows = r;
        cols = c;
        cells.resize(r);
        for (auto &row : cells) row.resize(c);
    }

    void clear() {
        for (auto &row : cells)
            for (auto &cell : row) { cell.text = " "; cell.hlId = 0; }
    }
};

/// Widget that renders multiple Neovim grids using QPainter
class GridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GridWidget(const QFont &font, QWidget *parent = nullptr);

    int cellWidth() const { return m_cellWidth; }
    int cellHeight() const { return m_cellHeight; }

public slots:
    void handleRedraw(const QVariantList &events);

signals:
    void gridSizeReady(int rows, int cols);
    void inputKey(const QString &keys);
    void inputMouse(const QString &button, const QString &action,
                    const QString &modifier, int grid, int row, int col);

    // Externalized UI events
    void cmdlineShow(const QVariantList &content, int pos, const QString &firstc,
                     const QString &prompt, int indent, int level);
    void cmdlineHide(int level);
    void cmdlinePos(int pos, int level);
    void popupmenuShow(const QVariantList &items, int selected, int row, int col, int grid);
    void popupmenuSelect(int selected);
    void popupmenuHide();
    void tablineUpdate(int curtab, const QVariantList &tabs);
    void msgShow(const QString &kind, const QVariantList &content, bool replaceLast);
    void msgClear();
    void msgShowmode(const QVariantList &content);
    void msgRuler(const QVariantList &content);

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
    void handleGridDestroy(const QVariantList &args);
    void handleHlAttrDefine(const QVariantList &args);
    void handleDefaultColorsSet(const QVariantList &args);
    void handleModeInfoSet(const QVariantList &args);
    void handleModeChange(const QVariantList &args);
    void handleWinPos(const QVariantList &args);
    void handleWinFloatPos(const QVariantList &args);
    void handleWinHide(const QVariantList &args);
    void handleWinClose(const QVariantList &args);
    void handleMsgSetPos(const QVariantList &args);
    void handleCmdlineShow(const QVariantList &args);
    void handleCmdlineHide(const QVariantList &args);
    void handleCmdlinePos(const QVariantList &args);
    void handlePopupmenuShow(const QVariantList &args);
    void handlePopupmenuSelect(const QVariantList &args);
    void handlePopupmenuHide(const QVariantList &args);
    void handleTablineUpdate(const QVariantList &args);
    void handleMsgShow(const QVariantList &args);
    void handleMsgClear(const QVariantList &args);
    void handleMsgShowmode(const QVariantList &args);
    void handleMsgRuler(const QVariantList &args);
    void recalculateGrid();
    QString keyToNvim(QKeyEvent *event);
    QString modifierPrefix(Qt::KeyboardModifiers mods, bool isSpecial);

    struct GridHit {
        int gridId = 0;
        int row = 0;
        int col = 0;
    };
    GridHit gridAt(const QPoint &pos);

    QFont m_font;
    int m_cellWidth = 0;
    int m_cellHeight = 0;

    // Cursor
    int m_cursorGrid = 1;
    int m_cursorRow = 0;
    int m_cursorCol = 0;

    struct ModeInfo {
        QString cursorShape = "block";
        int cellPercentage = 100;
    };
    QVector<ModeInfo> m_modeInfoList;
    int m_currentMode = 0;
    bool m_busy = false;

    bool m_mousePressed = false;
    int m_mouseGrid = 0;

    // Multi-grid state
    QHash<int, Grid> m_grids;
    QHash<int, HlAttr> m_hlAttrs;
    QColor m_defaultFg = Qt::white;
    QColor m_defaultBg = QColor(30, 30, 30);
};
