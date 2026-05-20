#include "gridwidget.h"
#include <QPainter>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <cmath>
#include <algorithm>

GridWidget::GridWidget(const QFont &font, QWidget *parent)
    : QWidget(parent)
{
    m_font = font;

    QFontMetrics fm(m_font);
    m_cellWidth = fm.horizontalAdvance('M');
    m_cellHeight = fm.height();

    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
}

void GridWidget::handleRedraw(const QVariantList &events)
{
    for (const auto &event : events) {
        QVariantList eventData = event.toList();
        if (eventData.isEmpty()) continue;

        QString name = eventData[0].toString();
        for (int i = 1; i < eventData.size(); i++) {
            processEvent(name, eventData[i].toList());
        }
    }
    update();
}

void GridWidget::processEvent(const QString &name, const QVariantList &args)
{
    if (name == "grid_resize") handleGridResize(args);
    else if (name == "grid_line") handleGridLine(args);
    else if (name == "grid_clear") handleGridClear(args);
    else if (name == "grid_cursor_goto") handleGridCursorGoto(args);
    else if (name == "grid_scroll") handleGridScroll(args);
    else if (name == "grid_destroy") handleGridDestroy(args);
    else if (name == "hl_attr_define") handleHlAttrDefine(args);
    else if (name == "default_colors_set") handleDefaultColorsSet(args);
    else if (name == "mode_info_set") handleModeInfoSet(args);
    else if (name == "mode_change") handleModeChange(args);
    else if (name == "win_pos") handleWinPos(args);
    else if (name == "win_float_pos") handleWinFloatPos(args);
    else if (name == "win_hide") handleWinHide(args);
    else if (name == "win_close") handleWinClose(args);
    else if (name == "msg_set_pos") handleMsgSetPos(args);
    else if (name == "cmdline_show") handleCmdlineShow(args);
    else if (name == "cmdline_hide") handleCmdlineHide(args);
    else if (name == "cmdline_pos") handleCmdlinePos(args);
    else if (name == "popupmenu_show") handlePopupmenuShow(args);
    else if (name == "popupmenu_select") handlePopupmenuSelect(args);
    else if (name == "popupmenu_hide") handlePopupmenuHide(args);
    else if (name == "tabline_update") handleTablineUpdate(args);
    else if (name == "msg_show") handleMsgShow(args);
    else if (name == "msg_clear") handleMsgClear(args);
    else if (name == "msg_showmode") handleMsgShowmode(args);
    else if (name == "msg_ruler") handleMsgRuler(args);
    else if (name == "busy_start") m_busy = true;
    else if (name == "busy_stop") m_busy = false;
}

void GridWidget::handleGridResize(const QVariantList &args)
{
    if (args.size() < 3) return;
    int gridId = args[0].toInt();
    int cols = args[1].toInt();
    int rows = args[2].toInt();

    m_grids[gridId].id = gridId;
    m_grids[gridId].resize(rows, cols);
}

void GridWidget::handleGridLine(const QVariantList &args)
{
    if (args.size() < 4) return;
    int gridId = args[0].toInt();
    int row = args[1].toInt();
    int col = args[2].toInt();
    QVariantList cells = args[3].toList();

    if (!m_grids.contains(gridId)) return;
    Grid &grid = m_grids[gridId];
    if (row < 0 || row >= grid.rows) return;

    int hlId = 0;
    for (const auto &cellVar : cells) {
        QVariantList cell = cellVar.toList();
        if (cell.isEmpty()) continue;

        QString text = cell[0].toString();
        if (cell.size() >= 2) hlId = cell[1].toInt();
        int repeat = (cell.size() >= 3) ? cell[2].toInt() : 1;

        for (int r = 0; r < repeat; r++) {
            if (col < grid.cols) {
                grid.cells[row][col].text = text;
                grid.cells[row][col].hlId = hlId;
                col++;
            }
        }
    }
}

void GridWidget::handleGridClear(const QVariantList &args)
{
    if (args.isEmpty()) return;
    int gridId = args[0].toInt();
    if (m_grids.contains(gridId)) m_grids[gridId].clear();
}

void GridWidget::handleGridCursorGoto(const QVariantList &args)
{
    if (args.size() < 3) return;
    m_cursorGrid = args[0].toInt();
    m_cursorRow = args[1].toInt();
    m_cursorCol = args[2].toInt();
}

void GridWidget::handleGridScroll(const QVariantList &args)
{
    if (args.size() < 7) return;
    int gridId = args[0].toInt();
    int top = args[1].toInt();
    int bot = args[2].toInt();
    int left = args[3].toInt();
    int right = args[4].toInt();
    int rows = args[5].toInt();

    if (!m_grids.contains(gridId)) return;
    Grid &grid = m_grids[gridId];

    if (rows > 0) {
        for (int r = top; r < bot - rows; r++)
            for (int c = left; c < right; c++)
                grid.cells[r][c] = grid.cells[r + rows][c];
        for (int r = bot - rows; r < bot; r++)
            for (int c = left; c < right; c++)
                grid.cells[r][c] = GridCell{};
    } else if (rows < 0) {
        for (int r = bot - 1; r >= top - rows; r--)
            for (int c = left; c < right; c++)
                grid.cells[r][c] = grid.cells[r + rows][c];
        for (int r = top; r < top - rows; r++)
            for (int c = left; c < right; c++)
                grid.cells[r][c] = GridCell{};
    }
}

void GridWidget::handleGridDestroy(const QVariantList &args)
{
    if (args.isEmpty()) return;
    m_grids.remove(args[0].toInt());
}

void GridWidget::handleWinPos(const QVariantList &args)
{
    if (args.size() < 6) return;
    // [grid, win, start_row, start_col, width, height]
    int gridId = args[0].toInt();
    int startRow = args[2].toInt();
    int startCol = args[3].toInt();

    if (m_grids.contains(gridId)) {
        m_grids[gridId].x = startCol;
        m_grids[gridId].y = startRow;
        m_grids[gridId].hidden = false;
        m_grids[gridId].isFloat = false;
        m_grids[gridId].zindex = 0;
    }
}

void GridWidget::handleWinFloatPos(const QVariantList &args)
{
    if (args.size() < 7) return;
    // [grid, win, anchor, anchor_grid, anchor_row, anchor_col, mouse_enabled, ...]
    int gridId = args[0].toInt();
    int anchorGrid = args[3].toInt();
    double anchorRow = args[4].toDouble();
    double anchorCol = args[5].toDouble();

    if (!m_grids.contains(gridId)) return;
    Grid &grid = m_grids[gridId];
    grid.isFloat = true;
    grid.hidden = false;

    // Position relative to anchor grid
    int baseX = 0, baseY = 0;
    if (m_grids.contains(anchorGrid)) {
        baseX = m_grids[anchorGrid].x;
        baseY = m_grids[anchorGrid].y;
    }
    grid.x = baseX + static_cast<int>(anchorCol);
    grid.y = baseY + static_cast<int>(anchorRow);

    // zindex from args if available
    if (args.size() >= 8) grid.zindex = args[7].toInt();
}

void GridWidget::handleWinHide(const QVariantList &args)
{
    if (args.isEmpty()) return;
    int gridId = args[0].toInt();
    if (m_grids.contains(gridId)) m_grids[gridId].hidden = true;
}

void GridWidget::handleWinClose(const QVariantList &args)
{
    if (args.isEmpty()) return;
    m_grids.remove(args[0].toInt());
}

void GridWidget::handleMsgSetPos(const QVariantList &args)
{
    if (args.size() < 2) return;
    // [grid, row, ...]
    int gridId = args[0].toInt();
    int row = args[1].toInt();
    if (m_grids.contains(gridId)) {
        m_grids[gridId].y = row;
        m_grids[gridId].x = 0;
        m_grids[gridId].hidden = false;
    }
}

void GridWidget::handleHlAttrDefine(const QVariantList &args)
{
    if (args.size() < 2) return;
    int id = args[0].toInt();
    QVariantMap attrs = args[1].toMap();

    HlAttr hl;
    if (attrs.contains("foreground"))
        hl.foreground = QColor::fromRgb(attrs["foreground"].toUInt());
    if (attrs.contains("background"))
        hl.background = QColor::fromRgb(attrs["background"].toUInt());
    hl.bold = attrs.value("bold", false).toBool();
    hl.italic = attrs.value("italic", false).toBool();
    hl.underline = attrs.value("underline", false).toBool();
    hl.reverse = attrs.value("reverse", false).toBool();

    m_hlAttrs[id] = hl;
}

void GridWidget::handleDefaultColorsSet(const QVariantList &args)
{
    if (args.size() < 2) return;
    if (args[0].toLongLong() >= 0)
        m_defaultFg = QColor::fromRgb(args[0].toUInt());
    if (args[1].toLongLong() >= 0)
        m_defaultBg = QColor::fromRgb(args[1].toUInt());
}

void GridWidget::handleModeInfoSet(const QVariantList &args)
{
    if (args.size() < 2) return;
    QVariantList modeList = args[1].toList();
    m_modeInfoList.clear();
    for (const auto &modeVar : modeList) {
        QVariantMap m = modeVar.toMap();
        ModeInfo info;
        QString shape = m.value("cursor_shape", "block").toString();
        if (shape == "vertical") info.cursorShape = "vertical";
        else if (shape == "horizontal") info.cursorShape = "horizontal";
        else info.cursorShape = "block";
        info.cellPercentage = m.value("cell_percentage", 100).toInt();
        m_modeInfoList.append(info);
    }
}

void GridWidget::handleModeChange(const QVariantList &args)
{
    if (args.size() < 2) return;
    m_currentMode = args[1].toInt();
}

// --- Externalized UI event handlers ---

void GridWidget::handleCmdlineShow(const QVariantList &args)
{
    if (args.size() < 6) return;
    // [content, pos, firstc, prompt, indent, level]
    emit cmdlineShow(args[0].toList(), args[1].toInt(),
                     args[2].toString(), args[3].toString(),
                     args[4].toInt(), args[5].toInt());
}

void GridWidget::handleCmdlineHide(const QVariantList &args)
{
    int level = (args.size() >= 1) ? args[0].toInt() : 0;
    emit cmdlineHide(level);
}

void GridWidget::handleCmdlinePos(const QVariantList &args)
{
    if (args.size() < 2) return;
    emit cmdlinePos(args[0].toInt(), args[1].toInt());
}

void GridWidget::handlePopupmenuShow(const QVariantList &args)
{
    if (args.size() < 5) return;
    // [items, selected, row, col, grid]
    emit popupmenuShow(args[0].toList(), args[1].toInt(),
                       args[2].toInt(), args[3].toInt(), args[4].toInt());
}

void GridWidget::handlePopupmenuSelect(const QVariantList &args)
{
    if (args.isEmpty()) return;
    emit popupmenuSelect(args[0].toInt());
}

void GridWidget::handlePopupmenuHide(const QVariantList &args)
{
    Q_UNUSED(args)
    emit popupmenuHide();
}

void GridWidget::handleTablineUpdate(const QVariantList &args)
{
    if (args.size() < 2) return;
    // [curtab, tabs, ...]
    emit tablineUpdate(args[0].toInt(), args[1].toList());
}

void GridWidget::handleMsgShow(const QVariantList &args)
{
    if (args.size() < 2) return;
    // [kind, content, replace_last]
    bool replaceLast = (args.size() >= 3) ? args[2].toBool() : false;
    emit msgShow(args[0].toString(), args[1].toList(), replaceLast);
}

void GridWidget::handleMsgClear(const QVariantList &args)
{
    Q_UNUSED(args)
    emit msgClear();
}

void GridWidget::handleMsgShowmode(const QVariantList &args)
{
    if (args.isEmpty()) return;
    emit msgShowmode(args[0].toList());
}

void GridWidget::handleMsgRuler(const QVariantList &args)
{
    if (args.isEmpty()) return;
    emit msgRuler(args[0].toList());
}

void GridWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), m_defaultBg);
    painter.setFont(m_font);

    // Collect visible grids sorted by z-order (non-float first, then float by zindex)
    QVector<int> gridOrder;
    for (auto it = m_grids.begin(); it != m_grids.end(); ++it) {
        if (!it->hidden) gridOrder.append(it.key());
    }
    std::sort(gridOrder.begin(), gridOrder.end(), [this](int a, int b) {
        const Grid &ga = m_grids[a];
        const Grid &gb = m_grids[b];
        if (ga.isFloat != gb.isFloat) return !ga.isFloat; // non-float first
        return ga.zindex < gb.zindex;
    });

    for (int gridId : gridOrder) {
        const Grid &grid = m_grids[gridId];
        int ox = grid.x * m_cellWidth;
        int oy = grid.y * m_cellHeight;

        for (int row = 0; row < grid.rows; row++) {
            for (int col = 0; col < grid.cols; col++) {
                const GridCell &cell = grid.cells[row][col];

                QColor fg = m_defaultFg;
                QColor bg = m_defaultBg;
                bool bold = false, italic = false, underline = false;

                if (cell.hlId != 0 && m_hlAttrs.contains(cell.hlId)) {
                    const HlAttr &hl = m_hlAttrs[cell.hlId];
                    if (hl.foreground.isValid()) fg = hl.foreground;
                    if (hl.background.isValid()) bg = hl.background;
                    if (hl.reverse) std::swap(fg, bg);
                    bold = hl.bold;
                    italic = hl.italic;
                    underline = hl.underline;
                }

                int x = ox + col * m_cellWidth;
                int y = oy + row * m_cellHeight;

                if (bg != m_defaultBg || grid.isFloat) {
                    painter.fillRect(x, y, m_cellWidth, m_cellHeight, bg);
                }

                if (!cell.text.isEmpty() && cell.text != " ") {
                    QFont cellFont = m_font;
                    if (bold) cellFont.setBold(true);
                    if (italic) cellFont.setItalic(true);
                    painter.setFont(cellFont);
                    painter.setPen(fg);
                    painter.drawText(x, y + m_cellHeight - painter.fontMetrics().descent(), cell.text);
                    if (bold || italic) painter.setFont(m_font);
                }

                if (underline) {
                    painter.setPen(fg);
                    painter.drawLine(x, y + m_cellHeight - 1, x + m_cellWidth, y + m_cellHeight - 1);
                }
            }
        }
    }

    // Draw cursor
    if (!m_busy && m_grids.contains(m_cursorGrid)) {
        const Grid &cg = m_grids[m_cursorGrid];
        if (!cg.hidden && m_cursorRow < cg.rows && m_cursorCol < cg.cols) {
            int x = (cg.x + m_cursorCol) * m_cellWidth;
            int y = (cg.y + m_cursorRow) * m_cellHeight;

            QString shape = "block";
            int pct = 100;
            if (m_currentMode < m_modeInfoList.size()) {
                shape = m_modeInfoList[m_currentMode].cursorShape;
                pct = m_modeInfoList[m_currentMode].cellPercentage;
            }

            QColor cursorColor(255, 255, 255, 180);
            if (shape == "block") {
                painter.fillRect(x, y, m_cellWidth, m_cellHeight, cursorColor);
                painter.setPen(m_defaultBg);
                painter.setFont(m_font);
                painter.drawText(x, y + m_cellHeight - painter.fontMetrics().descent(),
                                 cg.cells[m_cursorRow][m_cursorCol].text);
            } else if (shape == "vertical") {
                int w = qMax(1, m_cellWidth * pct / 100);
                painter.fillRect(x, y, w, m_cellHeight, cursorColor);
            } else if (shape == "horizontal") {
                int h = qMax(1, m_cellHeight * pct / 100);
                painter.fillRect(x, y + m_cellHeight - h, m_cellWidth, h, cursorColor);
            }
        }
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
    if (newCols > 0 && newRows > 0) {
        emit gridSizeReady(newRows, newCols);
    }
}

// --- Keyboard input ---

static const QHash<int, QString> &specialKeyMap()
{
    static const QHash<int, QString> map = {
        {Qt::Key_Escape, "Esc"},
        {Qt::Key_Return, "CR"}, {Qt::Key_Enter, "CR"},
        {Qt::Key_Backspace, "BS"},
        {Qt::Key_Tab, "Tab"}, {Qt::Key_Backtab, "Tab"},
        {Qt::Key_Space, "Space"},
        {Qt::Key_Up, "Up"}, {Qt::Key_Down, "Down"},
        {Qt::Key_Left, "Left"}, {Qt::Key_Right, "Right"},
        {Qt::Key_Home, "Home"}, {Qt::Key_End, "End"},
        {Qt::Key_PageUp, "PageUp"}, {Qt::Key_PageDown, "PageDown"},
        {Qt::Key_Insert, "Insert"}, {Qt::Key_Delete, "Del"},
        {Qt::Key_F1, "F1"}, {Qt::Key_F2, "F2"}, {Qt::Key_F3, "F3"},
        {Qt::Key_F4, "F4"}, {Qt::Key_F5, "F5"}, {Qt::Key_F6, "F6"},
        {Qt::Key_F7, "F7"}, {Qt::Key_F8, "F8"}, {Qt::Key_F9, "F9"},
        {Qt::Key_F10, "F10"}, {Qt::Key_F11, "F11"}, {Qt::Key_F12, "F12"},
        {Qt::Key_Less, "lt"},
    };
    return map;
}

QString GridWidget::modifierPrefix(Qt::KeyboardModifiers mods, bool isSpecial)
{
    QString prefix;
    if (mods & Qt::ShiftModifier && isSpecial) prefix += "S-";
    if (mods & Qt::ControlModifier) prefix += "C-";
    if (mods & Qt::AltModifier) prefix += "A-";
    if (mods & Qt::MetaModifier) prefix += "D-";
    return prefix;
}

QString GridWidget::keyToNvim(QKeyEvent *event)
{
    int key = event->key();
    Qt::KeyboardModifiers mods = event->modifiers();
    QString text = event->text();

    const auto &map = specialKeyMap();
    if (map.contains(key)) {
        QString name = map[key];
        QString mod = modifierPrefix(mods, true);
        if (key == Qt::Key_Backtab) mod += "S-";
        return "<" + mod + name + ">";
    }

    if ((mods & Qt::ControlModifier) && key >= Qt::Key_A && key <= Qt::Key_Z) {
        QString mod = modifierPrefix(mods, true);
        return "<" + mod + QString(QChar('a' + (key - Qt::Key_A))) + ">";
    }

    if ((mods & Qt::AltModifier) && !text.isEmpty()) {
        QString mod = modifierPrefix(mods, true);
        return "<" + mod + text + ">";
    }

    if (!text.isEmpty() && text[0].isPrint()) {
        if (text == "<") return "<lt>";
        return text;
    }

    return {};
}

void GridWidget::keyPressEvent(QKeyEvent *event)
{
    QString keys = keyToNvim(event);
    if (!keys.isEmpty()) emit inputKey(keys);
}

// --- Mouse input ---

GridWidget::GridHit GridWidget::gridAt(const QPoint &pos)
{
    int pixelCol = pos.x() / m_cellWidth;
    int pixelRow = pos.y() / m_cellHeight;

    // Check floating grids first (reverse z-order)
    QVector<int> floats;
    for (auto it = m_grids.begin(); it != m_grids.end(); ++it) {
        if (!it->hidden && it->isFloat) floats.append(it.key());
    }
    std::sort(floats.begin(), floats.end(), [this](int a, int b) {
        return m_grids[a].zindex > m_grids[b].zindex;
    });

    for (int gid : floats) {
        const Grid &g = m_grids[gid];
        int localCol = pixelCol - g.x;
        int localRow = pixelRow - g.y;
        if (localCol >= 0 && localCol < g.cols && localRow >= 0 && localRow < g.rows) {
            return {gid, localRow, localCol};
        }
    }

    // Check non-float grids
    for (auto it = m_grids.begin(); it != m_grids.end(); ++it) {
        if (it->hidden || it->isFloat) continue;
        const Grid &g = *it;
        int localCol = pixelCol - g.x;
        int localRow = pixelRow - g.y;
        if (localCol >= 0 && localCol < g.cols && localRow >= 0 && localRow < g.rows) {
            return {it.key(), localRow, localCol};
        }
    }

    // Fallback to grid 1
    return {1, qBound(0, pixelRow, 0), qBound(0, pixelCol, 0)};
}

void GridWidget::mousePressEvent(QMouseEvent *event)
{
    auto hit = gridAt(event->pos());
    m_mousePressed = true;
    m_mouseGrid = hit.gridId;

    QString button;
    if (event->button() == Qt::LeftButton) button = "left";
    else if (event->button() == Qt::RightButton) button = "right";
    else if (event->button() == Qt::MiddleButton) button = "middle";
    else return;

    QString mod = modifierPrefix(event->modifiers(), false);
    emit inputMouse(button, "press", mod, hit.gridId, hit.row, hit.col);
}

void GridWidget::mouseReleaseEvent(QMouseEvent *event)
{
    auto hit = gridAt(event->pos());
    m_mousePressed = false;

    QString button;
    if (event->button() == Qt::LeftButton) button = "left";
    else if (event->button() == Qt::RightButton) button = "right";
    else if (event->button() == Qt::MiddleButton) button = "middle";
    else return;

    QString mod = modifierPrefix(event->modifiers(), false);
    emit inputMouse(button, "release", mod, hit.gridId, hit.row, hit.col);
}

void GridWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_mousePressed) return;
    auto hit = gridAt(event->pos());
    QString mod = modifierPrefix(event->modifiers(), false);
    emit inputMouse("left", "drag", mod, m_mouseGrid, hit.row, hit.col);
}

void GridWidget::wheelEvent(QWheelEvent *event)
{
    auto hit = gridAt(event->position().toPoint());
    QString mod = modifierPrefix(event->modifiers(), false);
    QString dir = event->angleDelta().y() > 0 ? "up" : "down";
    emit inputMouse("wheel", dir, mod, hit.gridId, hit.row, hit.col);
}
