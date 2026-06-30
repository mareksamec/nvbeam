#pragma once

#include <QWidget>
#include <QColor>
#include <QFont>
#include <QVector>
#include <QHash>
#include <QTimer>
#include <QVariant>
#include <vector>

/// A single cell in the character grid
struct GridCell {
    QString text; // empty string represents a space (avoids allocating " " for every cell)
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
    int win = -1;    // Neovim window handle (from win_pos), -1 if unknown
    int rows = 0;
    int cols = 0;
    int x = 0;       // position in cell coords on the screen
    int y = 0;
    int zindex = 0;
    bool hidden = false;
    bool isFloat = false;
    std::vector<GridCell> cells; // flat row-major: index = row * cols + col

    void resize(int r, int c) {
        rows = r;
        cols = c;
        cells.assign(static_cast<std::size_t>(r * c), GridCell{});
    }

    void clear() {
        std::fill(cells.begin(), cells.end(), GridCell{});
    }

    GridCell &at(int row, int col)             { return cells[row * cols + col]; }
    const GridCell &at(int row, int col) const { return cells[row * cols + col]; }
};

/// Widget that renders multiple Neovim grids using QPainter
class GridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GridWidget(const QFont &font, QWidget *parent = nullptr);

    int cellWidth() const { return m_cellWidth; }
    int cellHeight() const { return m_cellHeight; }

    /// Configure terminal-style mouse selection behavior
    void setMouseConfig(bool terminalSelection, bool autoCopy, const QColor &selColor);

public slots:
    void handleRedraw(const QVariantList &events);

signals:
    void gridSizeReady(int rows, int cols);
    void inputKey(const QString &keys);
    void inputMouse(const QString &button, const QString &action,
                    const QString &modifier, int grid, int row, int col);

    // Externalized UI events
    void popupmenuShow(const QVariantList &items, int selected, int row, int col, int grid);
    void popupmenuSelect(int selected);
    void popupmenuHide();
    void tablineUpdate(int curtab, const QVariantList &tabs);
    void msgShow(const QString &kind, const QVariantList &content, bool replaceLast);
    void msgClear();
    void msgShowmode(const QVariantList &content);
    void msgRuler(const QVariantList &content);
    void flushed();

    /// Emitted when local files are dropped onto the buffer area.
    /// \param paths     dropped local file paths
    /// \param win       target Neovim window handle under the cursor (-1 if unknown)
    /// \param modifiers keyboard modifiers held during the drop
    void filesDropped(const QStringList &paths, int win, Qt::KeyboardModifiers modifiers);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

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
    void handlePopupmenuShow(const QVariantList &args);
    void handlePopupmenuSelect(const QVariantList &args);
    void handlePopupmenuHide(const QVariantList &args);
    void handleTablineUpdate(const QVariantList &args);
    void handleMsgShow(const QVariantList &args);
    void handleMsgClear(const QVariantList &args);
    void handleMsgShowmode(const QVariantList &args);
    void handleMsgRuler(const QVariantList &args);
    void handleOptionSet(const QVariantList &args);
    void recalculateGrid();
    void rebuildFontCache();
    void rebuildGridOrder();
    QString extractSelectedText() const;
    void paintSelection(QPainter &p) const;
    bool hasSelection() const {
        return m_selStartRow != m_selEndRow || m_selStartCol != m_selEndCol;
    }
    QString keyToNvim(QKeyEvent *event);
    QString modifierPrefix(Qt::KeyboardModifiers mods, bool isSpecial);

    struct GridHit {
        int gridId = 0;
        int row = 0;
        int col = 0;
    };
    GridHit gridAt(const QPoint &pos);

    QFont m_font;
    QFont m_fontCache[4]; // [0]=normal [1]=bold [2]=italic [3]=bold+italic
    int m_fontDescent[4] = {}; // per-variant descent, indexed same as m_fontCache
    int m_cellWidth = 0;
    int m_cellHeight = 0;

    // Grids sorted for paint order (non-float first, then float by ascending zindex).
    // Rebuilt lazily when m_gridOrderDirty is set.
    QVector<int> m_gridOrder;
    bool m_gridOrderDirty = true;

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

    // Neovim mouse option tracking
    QString m_nvimMouseOpt = "a"; // tracks vim's 'mouse' option value

    // Terminal-style mouse selection state
    bool m_terminalSelectionEnabled = false;
    bool m_mouseAutoCopy = true;
    QColor m_selectionColor = QColor("#44475a");
    bool m_selecting = false;
    int m_selStartGrid = 0;
    int m_selStartRow = 0;
    int m_selStartCol = 0;
    int m_selEndRow = 0;
    int m_selEndCol = 0;

    // Multi-grid state
    QHash<int, Grid> m_grids;
    QHash<int, HlAttr> m_hlAttrs;
    QColor m_defaultFg = Qt::white;
    QColor m_defaultBg = QColor(30, 30, 30);
};
