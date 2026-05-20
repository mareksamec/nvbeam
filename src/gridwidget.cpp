#include "gridwidget.h"
#include <QPainter>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QDebug>

GridWidget::GridWidget(QWidget *parent)
    : QWidget(parent)
{
    m_font = QFont("Monospace", 12);
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);

    QFontMetrics fm(m_font);
    m_cellWidth = fm.horizontalAdvance('M');
    m_cellHeight = fm.height();

    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void GridWidget::handleRedraw(const QVariantList &events)
{
    for (const auto &event : events) {
        QVariantList eventData = event.toList();
        if (eventData.isEmpty()) continue;

        QString name = eventData[0].toString();
        // Each event can have multiple argument sets
        for (int i = 1; i < eventData.size(); i++) {
            processEvent(name, eventData[i].toList());
        }
    }
    update();
}

void GridWidget::processEvent(const QString &name, const QVariantList &args)
{
    if (name == "grid_resize") {
        handleGridResize(args);
    } else if (name == "grid_line") {
        handleGridLine(args);
    } else if (name == "grid_clear") {
        handleGridClear(args);
    } else if (name == "grid_cursor_goto") {
        handleGridCursorGoto(args);
    } else if (name == "grid_scroll") {
        handleGridScroll(args);
    } else if (name == "hl_attr_define") {
        handleHlAttrDefine(args);
    } else if (name == "default_colors_set") {
        handleDefaultColorsSet(args);
    }
}

void GridWidget::handleGridResize(const QVariantList &args)
{
    if (args.size() < 3) return;
    // [grid, width, height]
    int cols = args[1].toInt();
    int rows = args[2].toInt();

    m_rows = rows;
    m_cols = cols;
    m_grid.resize(rows);
    for (int r = 0; r < rows; r++) {
        m_grid[r].resize(cols);
    }
}

void GridWidget::handleGridLine(const QVariantList &args)
{
    if (args.size() < 4) return;
    // [grid, row, col_start, cells...]
    int row = args[1].toInt();
    int col = args[2].toInt();

    if (row < 0 || row >= m_rows) return;

    int hlId = 0;
    for (int i = 3; i < args.size(); i++) {
        QVariantList cell = args[i].toList();
        if (cell.isEmpty()) continue;

        QString text = cell[0].toString();
        if (cell.size() >= 2) {
            hlId = cell[1].toInt();
        }
        int repeat = 1;
        if (cell.size() >= 3) {
            repeat = cell[2].toInt();
        }

        for (int r = 0; r < repeat; r++) {
            if (col < m_cols) {
                m_grid[row][col].text = text;
                m_grid[row][col].hlId = hlId;
                col++;
            }
        }
    }
}

void GridWidget::handleGridClear(const QVariantList &args)
{
    Q_UNUSED(args)
    for (auto &row : m_grid) {
        for (auto &cell : row) {
            cell.text = " ";
            cell.hlId = 0;
        }
    }
}

void GridWidget::handleGridCursorGoto(const QVariantList &args)
{
    if (args.size() < 3) return;
    // [grid, row, col]
    m_cursorRow = args[1].toInt();
    m_cursorCol = args[2].toInt();
}

void GridWidget::handleGridScroll(const QVariantList &args)
{
    if (args.size() < 7) return;
    // [grid, top, bot, left, right, rows, cols]
    int top = args[1].toInt();
    int bot = args[2].toInt();
    int left = args[3].toInt();
    int right = args[4].toInt();
    int rows = args[5].toInt();

    if (rows > 0) {
        // Scroll up
        for (int r = top; r < bot - rows; r++) {
            for (int c = left; c < right; c++) {
                m_grid[r][c] = m_grid[r + rows][c];
            }
        }
        for (int r = bot - rows; r < bot; r++) {
            for (int c = left; c < right; c++) {
                m_grid[r][c] = GridCell{};
            }
        }
    } else if (rows < 0) {
        // Scroll down
        for (int r = bot - 1; r >= top - rows; r--) {
            for (int c = left; c < right; c++) {
                m_grid[r][c] = m_grid[r + rows][c];
            }
        }
        for (int r = top; r < top - rows; r++) {
            for (int c = left; c < right; c++) {
                m_grid[r][c] = GridCell{};
            }
        }
    }
}

void GridWidget::handleHlAttrDefine(const QVariantList &args)
{
    if (args.size() < 2) return;
    // [id, rgb_attrs, ...]
    int id = args[0].toInt();
    QVariantMap attrs = args[1].toMap();

    HlAttr hl;
    hl.foreground = m_defaultFg;
    hl.background = m_defaultBg;

    if (attrs.contains("foreground")) {
        hl.foreground = QColor::fromRgb(attrs["foreground"].toUInt());
    }
    if (attrs.contains("background")) {
        hl.background = QColor::fromRgb(attrs["background"].toUInt());
    }
    hl.bold = attrs.value("bold", false).toBool();
    hl.italic = attrs.value("italic", false).toBool();
    hl.underline = attrs.value("underline", false).toBool();
    hl.reverse = attrs.value("reverse", false).toBool();

    m_hlAttrs[id] = hl;
}

void GridWidget::handleDefaultColorsSet(const QVariantList &args)
{
    if (args.size() < 2) return;
    // [fg, bg, sp, ...]
    if (args[0].toLongLong() >= 0) {
        m_defaultFg = QColor::fromRgb(args[0].toUInt());
    }
    if (args[1].toLongLong() >= 0) {
        m_defaultBg = QColor::fromRgb(args[1].toUInt());
    }
    // Update default highlight
    HlAttr defaultHl;
    defaultHl.foreground = m_defaultFg;
    defaultHl.background = m_defaultBg;
    m_hlAttrs[0] = defaultHl;
}

void GridWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), m_defaultBg);
    painter.setFont(m_font);

    for (int row = 0; row < m_rows; row++) {
        for (int col = 0; col < m_cols; col++) {
            const GridCell &cell = m_grid[row][col];
            const HlAttr &hl = m_hlAttrs.value(cell.hlId, m_hlAttrs.value(0));

            QColor fg = hl.reverse ? hl.background : hl.foreground;
            QColor bg = hl.reverse ? hl.foreground : hl.background;

            int x = col * m_cellWidth;
            int y = row * m_cellHeight;

            // Draw background
            if (bg != m_defaultBg) {
                painter.fillRect(x, y, m_cellWidth, m_cellHeight, bg);
            }

            // Draw text
            if (!cell.text.isEmpty() && cell.text != " ") {
                QFont cellFont = m_font;
                cellFont.setBold(hl.bold);
                cellFont.setItalic(hl.italic);
                painter.setFont(cellFont);
                painter.setPen(fg);
                painter.drawText(x, y + m_cellHeight - painter.fontMetrics().descent(), cell.text);
            }

            // Draw underline
            if (hl.underline) {
                painter.setPen(fg);
                int lineY = y + m_cellHeight - 1;
                painter.drawLine(x, lineY, x + m_cellWidth, lineY);
            }
        }
    }

    // Draw cursor
    if (m_cursorRow < m_rows && m_cursorCol < m_cols) {
        int x = m_cursorCol * m_cellWidth;
        int y = m_cursorRow * m_cellHeight;
        painter.fillRect(x, y, m_cellWidth, m_cellHeight, QColor(255, 255, 255, 128));
    }
}

void GridWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    recalculateGrid();
}

void GridWidget::recalculateGrid()
{
    int newCols = width() / m_cellWidth;
    int newRows = height() / m_cellHeight;
    if (newCols > 0 && newRows > 0 && (newCols != m_cols || newRows != m_rows)) {
        emit gridSizeReady(newRows, newCols);
    }
}
