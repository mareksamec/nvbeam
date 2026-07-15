#include "gridwidget.h"
#include <QPainter>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QWheelEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QGuiApplication>
#include <QStyleHints>
#include <QClipboard>
#include <QDebug>
#include "logging.h"
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
    setAcceptDrops(true); // accept file drops onto the buffer area

    // Holds a dragless Shift+click for the double-click window so a
    // Shift+double-click becomes a word selection instead of reaching nvim.
    m_deferredClickTimer.setSingleShot(true);
    m_deferredClickTimer.setInterval(QGuiApplication::styleHints()->mouseDoubleClickInterval());
    connect(&m_deferredClickTimer, &QTimer::timeout, this, [this]() {
        // isActive() is already false inside timeout; forward directly.
        emit inputMouse("left", "press", m_deferredMod,
                        m_pendingGrid, m_pendingRow, m_pendingCol);
        emit inputMouse("left", "release", m_deferredMod,
                        m_deferredReleaseGrid, m_deferredReleaseRow, m_deferredReleaseCol);
    });
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

void GridWidget::resetGrids()
{
    m_grids.clear();
    m_cursorGrid = 1;
    m_cursorRow = 0;
    m_cursorCol = 0;
    m_selecting = false;
    m_pendingSelectPress = false;
    m_skipNextRelease = false;
    m_deferredClickTimer.stop();
    m_selStartRow = m_selEndRow = 0;
    m_selStartCol = m_selEndCol = 0;
    // Highlight table and default colors are redefined by the new instance on
    // attach (hl_attr_define / default_colors_set), so they can stay until then.
    update();
}

void GridWidget::processEvent(const QString &name, const QVariantList &args)
{
    qCDebug(nbRedraw) << name;
    if (name == "flush") {
        // nvim re-emits win_viewport_margins + tabline_update + flush after
        // every RPC request it serves. Emitting flushed() for those batches
        // would re-arm the flush-driven pollers (statusline/bufferline/
        // minimap), whose next request triggers the same batch again: an
        // infinite RPC loop that burns CPU while idle. Only batches carrying
        // real content wake the pollers.
        if (m_flushDirty) {
            m_flushDirty = false;
            emit flushed();
        }
        return;
    }
    if (name != QLatin1String("win_viewport_margins") &&
        name != QLatin1String("tabline_update"))
        m_flushDirty = true;

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
    else if (name == "popupmenu_show") handlePopupmenuShow(args);
    else if (name == "popupmenu_select") handlePopupmenuSelect(args);
    else if (name == "popupmenu_hide") handlePopupmenuHide(args);
    else if (name == "tabline_update") handleTablineUpdate(args);
    else if (name == "cmdline_show") handleCmdlineShow(args);
    else if (name == "cmdline_pos") handleCmdlinePos(args);
    else if (name == "cmdline_hide") emit cmdlineHide();
    else if (name == "cmdline_special_char") handleCmdlineSpecialChar(args);
    else if (name == "msg_show") handleMsgShow(args);
    else if (name == "msg_clear") handleMsgClear(args);
    else if (name == "msg_showmode") handleMsgShowmode(args);
    else if (name == "msg_ruler") handleMsgRuler(args);
    else if (name == "busy_start") m_busy = true;
    else if (name == "busy_stop") m_busy = false;
    else if (name == "option_set") handleOptionSet(args);
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

    // Keep any selection glued to the text it covers: shift it with the
    // scroll, drop it once the selected rows leave the scrolled region.
    if ((hasSelection() || m_selecting) && gridId == m_selStartGrid) {
        bool startIn = m_selStartRow >= top && m_selStartRow < bot;
        bool endIn = m_selEndRow >= top && m_selEndRow < bot;
        if (startIn || endIn) {
            int newStart = m_selStartRow - rows;
            int newEnd = m_selEndRow - rows;
            if (newStart >= top && newStart < bot && newEnd >= top && newEnd < bot) {
                m_selStartRow = newStart;
                m_selEndRow = newEnd;
            } else if (m_selecting) {
                // Mid-drag: clamp instead of cancelling so the drag survives
                m_selStartRow = qBound(top, newStart, bot - 1);
                m_selEndRow = qBound(top, newEnd, bot - 1);
            } else {
                m_selStartRow = m_selEndRow = 0;
                m_selStartCol = m_selEndCol = 0;
            }
        }
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
    int win = args[1].toInt();
    int startRow = args[2].toInt();
    int startCol = args[3].toInt();

    if (m_grids.contains(gridId)) {
        m_grids[gridId].win = win;
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
    int win = args[1].toInt();
    int anchorGrid = args[3].toInt();
    double anchorRow = args[4].toDouble();
    double anchorCol = args[5].toDouble();

    if (!m_grids.contains(gridId)) return;
    Grid &grid = m_grids[gridId];
    grid.win = win;
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
        m_grids[gridId].isMsg = true;
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

void GridWidget::handleCmdlineShow(const QVariantList &args)
{
    if (args.size() < 6) return;
    // [content, pos, firstc, prompt, indent, level]
    emit cmdlineShow(args[0].toList(), args[1].toInt(), args[2].toString(),
                     args[3].toString(), args[4].toInt(), args[5].toInt());
}

void GridWidget::handleCmdlinePos(const QVariantList &args)
{
    if (args.size() < 2) return;
    emit cmdlinePos(args[0].toInt(), args[1].toInt());
}

void GridWidget::handleCmdlineSpecialChar(const QVariantList &args)
{
    if (args.size() < 2) return;
    emit cmdlineSpecialChar(args[0].toString(), args[1].toBool());
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

    // Collect visible grids sorted by compositor layer, mirroring nvim's own
    // compositor: the global grid 1 at the bottom, tiled window grids above
    // it, the message grid above those, floats on top (by zindex). Ties fall
    // back to the grid id so the order is deterministic.
    QVector<int> gridOrder;
    for (auto it = m_grids.begin(); it != m_grids.end(); ++it) {
        if (!it->hidden) gridOrder.append(it.key());
    }
    auto layerOf = [this](int id) {
        const Grid &g = m_grids[id];
        if (g.isFloat) return 3;
        if (g.isMsg) return 2;
        return id == 1 ? 0 : 1;
    };
    std::sort(gridOrder.begin(), gridOrder.end(), [this, &layerOf](int a, int b) {
        const int la = layerOf(a), lb = layerOf(b);
        if (la != lb) return la < lb;
        const Grid &ga = m_grids[a];
        const Grid &gb = m_grids[b];
        if (ga.zindex != gb.zindex) return ga.zindex < gb.zindex;
        return a < b;
    });

    for (int gridId : gridOrder) {
        const Grid &grid = m_grids[gridId];
        int ox = grid.x * m_cellWidth;
        int oy = grid.y * m_cellHeight;

        // Grids are opaque: grid 1 keeps stale content underneath windows by
        // design (old separators, closed splits), so every grid must cover
        // its whole rectangle including cells with the default background.
        painter.fillRect(ox, oy, grid.cols * m_cellWidth, grid.rows * m_cellHeight,
                         m_defaultBg);

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

    // Draw selection overlay
    paintSelection(painter);

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

/// Intercept Tab/Backtab before Qt uses them for focus navigation
bool GridWidget::event(QEvent *ev)
{
    if (ev->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(ev);
        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) {
            keyPressEvent(ke);
            return true;
        }
    }
    return QWidget::event(ev);
}

void GridWidget::keyPressEvent(QKeyEvent *event)
{
    flushDeferredClick(); // keep input ordering: held Shift+click goes first
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

    // Check non-float grids. Grid 1 is the backdrop: it spans the whole
    // window but holds no buffer text under ext_multigrid, so it must never
    // shadow a window grid (QHash iteration order is arbitrary); it serves
    // as the fallback below instead.
    for (auto it = m_grids.begin(); it != m_grids.end(); ++it) {
        if (it.key() == 1 || it->hidden || it->isFloat) continue;
        const Grid &g = *it;
        int localCol = pixelCol - g.x;
        int localRow = pixelRow - g.y;
        if (localCol >= 0 && localCol < g.cols && localRow >= 0 && localRow < g.rows) {
            return {it.key(), localRow, localCol};
        }
    }

    // Fallback to grid 1
    auto g1 = m_grids.constFind(1);
    if (g1 != m_grids.constEnd() && g1->rows > 0 && g1->cols > 0)
        return {1, qBound(0, pixelRow, g1->rows - 1), qBound(0, pixelCol, g1->cols - 1)};
    return {1, 0, 0};
}

GridWidget::GridHit GridWidget::hitInGrid(const QPoint &pos, int gridId) const
{
    // Pointer position in gridId's local cell coords, clamped to its bounds.
    // Used while a selection is in progress so it stays anchored to the grid
    // it started in regardless of what the pointer is currently over.
    auto it = m_grids.constFind(gridId);
    if (it == m_grids.constEnd() || it->rows <= 0 || it->cols <= 0)
        return {gridId, 0, 0};
    int row = pos.y() / m_cellHeight - it->y;
    int col = pos.x() / m_cellWidth - it->x;
    return {gridId, qBound(0, row, it->rows - 1), qBound(0, col, it->cols - 1)};
}

void GridWidget::mousePressEvent(QMouseEvent *event)
{
    // A Shift+click held back for the double-click window must reach nvim
    // before any newer input.
    flushDeferredClick();

    auto hit = gridAt(event->pos());
    m_mousePressed = true;
    m_mouseGrid = hit.gridId;

    // Terminal-style selection: explicit selection mode, or left-click when
    // nvim mouse is empty
    bool useTerminalSelection = event->button() == Qt::LeftButton &&
                                (m_selectionModeActive ||
                                 (m_terminalSelectionEnabled &&
                                  m_nvimMouseOpt.isEmpty()));

    if (useTerminalSelection) {
        m_selecting = true;
        m_selectionExplicit = m_selectionModeActive;
        m_selStartGrid = hit.gridId;
        m_selStartRow = hit.row;
        m_selStartCol = hit.col;
        m_selEndRow = hit.row;
        m_selEndCol = hit.col;
        update();
        return;
    }

    // Shift+left outside selection mode: candidate for a one-shot terminal
    // selection, but only once the pointer actually drags. The press is
    // deferred so a plain Shift+click still reaches nvim (<S-LeftMouse>).
    if (event->button() == Qt::LeftButton &&
        event->modifiers().testFlag(Qt::ShiftModifier)) {
        m_pendingSelectPress = true;
        m_pendingGrid = hit.gridId;
        m_pendingRow = hit.row;
        m_pendingCol = hit.col;
        return;
    }

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

    if (m_skipNextRelease && event->button() == Qt::LeftButton) {
        // Release that ends a double-click word selection: already handled.
        m_skipNextRelease = false;
        return;
    }

    if (m_pendingSelectPress && event->button() == Qt::LeftButton) {
        // Shift+click without a drag: hold the deferred press/release for the
        // double-click window. A Shift+double-click must become a word
        // selection, not two <S-LeftMouse> clicks in nvim; a single click is
        // forwarded unchanged once the window expires.
        m_pendingSelectPress = false;
        m_deferredMod = modifierPrefix(event->modifiers(), false);
        m_deferredReleaseGrid = hit.gridId;
        m_deferredReleaseRow = hit.row;
        m_deferredReleaseCol = hit.col;
        m_deferredClickTimer.start();
        return;
    }

    if (m_selecting && event->button() == Qt::LeftButton) {
        m_selecting = false;
        auto end = hitInGrid(event->pos(), m_selStartGrid);
        m_selEndRow = end.row;
        m_selEndCol = end.col;

        // Copy to CLIPBOARD + PRIMARY (same targets as the terminal panels).
        // Explicit selections (mode / Shift+drag) always copy; the implicit
        // mouse= path honors the config toggle.
        if (m_selectionExplicit || m_mouseAutoCopy)
            copySelection();
        if (!m_selectionModeActive)
            unsetCursor();  // end of a one-shot Shift+drag selection
        update();
        return;
    }

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

    if (m_pendingSelectPress) {
        // Deferred Shift+press: a move of at least one cell promotes it to a
        // one-shot terminal-style selection.
        auto hit = hitInGrid(event->pos(), m_pendingGrid);
        if (hit.row != m_pendingRow || hit.col != m_pendingCol) {
            m_pendingSelectPress = false;
            m_selecting = true;
            m_selectionExplicit = true;
            m_selStartGrid = m_pendingGrid;
            m_selStartRow = m_pendingRow;
            m_selStartCol = m_pendingCol;
            m_selEndRow = hit.row;
            m_selEndCol = hit.col;
            setCursor(Qt::IBeamCursor);
            update();
        }
        return;
    }

    if (m_selecting) {
        // Keep the selection anchored to the grid it started in, even when
        // the pointer crosses into another split or float.
        auto hit = hitInGrid(event->pos(), m_selStartGrid);
        m_selEndRow = hit.row;
        m_selEndCol = hit.col;
        update();
        return;
    }

    auto hit = gridAt(event->pos());
    QString mod = modifierPrefix(event->modifiers(), false);
    emit inputMouse("left", "drag", mod, m_mouseGrid, hit.row, hit.col);
}

void GridWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Terminal-style word selection: selection mode, Shift held, or the
    // implicit mouse= path. Nothing is forwarded to nvim in this case.
    bool wantSelection = event->button() == Qt::LeftButton &&
                         (m_selectionModeActive ||
                          event->modifiers().testFlag(Qt::ShiftModifier) ||
                          (m_terminalSelectionEnabled && m_nvimMouseOpt.isEmpty()));
    if (wantSelection) {
        // Drop the Shift+click held back on the first release: it is part of
        // this double-click, not a standalone <S-LeftMouse>.
        m_deferredClickTimer.stop();
        m_pendingSelectPress = false;
        m_selecting = false;
        selectWordAt(gridAt(event->pos()));
        m_skipNextRelease = true;
        return;
    }

    // Same as QWidget's default: treat the second press like a press so nvim
    // can do its own multi-click handling.
    mousePressEvent(event);
}

void GridWidget::selectWordAt(const GridHit &hit)
{
    auto it = m_grids.constFind(hit.gridId);
    if (it == m_grids.constEnd()) return;
    const Grid &grid = *it;
    if (hit.row < 0 || hit.row >= grid.rows ||
        hit.col < 0 || hit.col >= grid.cols) return;

    // Three character classes, xterm-style: whitespace, word (alnum + _),
    // other symbols. The selection expands over the run of the clicked class.
    auto charClass = [](const QString &t) -> int {
        if (t.isEmpty()) return 0; // double-width continuation cell
        const QChar ch = t.at(0);
        if (ch.isSpace()) return 0;
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_')) return 1;
        return 2;
    };

    const auto &row = grid.cells[hit.row];
    int cls = charClass(row[hit.col].text);
    if (cls == 0) return; // double-clicked whitespace: nothing to select

    int c1 = hit.col, c2 = hit.col;
    while (c1 > 0 && charClass(row[c1 - 1].text) == cls) c1--;
    while (c2 + 1 < grid.cols && charClass(row[c2 + 1].text) == cls) c2++;

    m_selectionExplicit = true;
    m_selStartGrid = hit.gridId;
    m_selStartRow = m_selEndRow = hit.row;
    m_selStartCol = c1;
    m_selEndCol = c2;
    copySelection();
    update();
}

void GridWidget::copySelection()
{
    const QString text = extractSelectedText();
    if (text.isEmpty()) return;
    QClipboard *cb = QGuiApplication::clipboard();
    cb->setText(text);
    if (cb->supportsSelection())
        cb->setText(text, QClipboard::Selection);
}

void GridWidget::flushDeferredClick()
{
    if (!m_deferredClickTimer.isActive()) return;
    m_deferredClickTimer.stop();
    emit inputMouse("left", "press", m_deferredMod,
                    m_pendingGrid, m_pendingRow, m_pendingCol);
    emit inputMouse("left", "release", m_deferredMod,
                    m_deferredReleaseGrid, m_deferredReleaseRow, m_deferredReleaseCol);
}

void GridWidget::wheelEvent(QWheelEvent *event)
{
    auto hit = gridAt(event->position().toPoint());
    QString mod = modifierPrefix(event->modifiers(), false);
    QString dir = event->angleDelta().y() > 0 ? "up" : "down";
    emit inputMouse("wheel", dir, mod, hit.gridId, hit.row, hit.col);
}

void GridWidget::dragEnterEvent(QDragEnterEvent *event)
{
    // Only accept drags that carry at least one local file path (e.g. from a
    // file manager). Non-file drags are left for other widgets / ignored.
    if (!event->mimeData()->hasUrls())
        return;

    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            event->acceptProposedAction();
            return;
        }
    }
}

void GridWidget::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls())
        return;

    // Collect local file paths; remote URLs are ignored here.
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }

    if (!paths.isEmpty()) {
        event->acceptProposedAction();

        // Resolve which window sits under the drop position so the file can be
        // opened in that specific window (multi-grid targeting). Falls back to
        // -1 (caller uses the current window) when the grid has no known handle.
        const QPoint pos = event->position().toPoint();
        GridHit hit = gridAt(pos);
        int win = -1;
        if (m_grids.contains(hit.gridId))
            win = m_grids[hit.gridId].win;

        emit filesDropped(paths, win, event->modifiers());
    }
}

void GridWidget::setMouseConfig(bool terminalSelection, bool autoCopy, const QColor &selColor)
{
    m_terminalSelectionEnabled = terminalSelection;
    m_mouseAutoCopy = autoCopy;
    m_selectionColor = selColor;
}

void GridWidget::setSelectionModeActive(bool on)
{
    if (m_selectionModeActive == on) return;
    m_selectionModeActive = on;
    if (on) {
        setCursor(Qt::IBeamCursor);
    } else {
        unsetCursor();
        m_selecting = false;
        m_pendingSelectPress = false;
        m_skipNextRelease = false;
        m_selStartRow = m_selEndRow = 0;
        m_selStartCol = m_selEndCol = 0;
    }
    update();
    emit selectionModeChanged(on);
}

void GridWidget::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    emit focusChanged(true);
}

void GridWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    emit focusChanged(false);
}

void GridWidget::handleOptionSet(const QVariantList &args)
{
    // option_set events come as: [name, value] pairs
    if (args.size() < 2) return;
    QString name = args[0].toString();
    if (name == "mouse")
        m_nvimMouseOpt = args[1].toString();
}

QString GridWidget::extractSelectedText() const
{
    if (!m_grids.contains(m_selStartGrid)) return {};
    const Grid &grid = m_grids[m_selStartGrid];

    // Normalize selection bounds (start before end)
    int r1 = m_selStartRow, c1 = m_selStartCol;
    int r2 = m_selEndRow, c2 = m_selEndCol;
    if (r1 > r2 || (r1 == r2 && c1 > c2)) {
        std::swap(r1, r2);
        std::swap(c1, c2);
    }

    QString result;
    for (int r = r1; r <= r2; ++r) {
        if (r < 0 || r >= grid.rows) continue;
        int startCol = (r == r1) ? c1 : 0;
        int endCol = (r == r2) ? c2 : grid.cols - 1;
        startCol = qBound(0, startCol, grid.cols - 1);
        endCol = qBound(0, endCol, grid.cols - 1);

        QString line;
        for (int c = startCol; c <= endCol; ++c)
            line += grid.cells[r][c].text;

        // Trim trailing spaces from each line
        while (line.endsWith(' ')) line.chop(1);
        if (r > r1) result += '\n';
        result += line;
    }
    return result;
}

void GridWidget::paintSelection(QPainter &p) const
{
    if (!hasSelection()) return;
    if (!m_grids.contains(m_selStartGrid)) return;
    const Grid &grid = m_grids[m_selStartGrid];

    int r1 = m_selStartRow, c1 = m_selStartCol;
    int r2 = m_selEndRow, c2 = m_selEndCol;
    if (r1 > r2 || (r1 == r2 && c1 > c2)) {
        std::swap(r1, r2);
        std::swap(c1, c2);
    }

    // Grid pixel offset
    int gx = grid.x * m_cellWidth;
    int gy = grid.y * m_cellHeight;

    p.save();
    p.setBrush(m_selectionColor);
    p.setPen(Qt::NoPen);
    p.setOpacity(0.5);

    for (int r = r1; r <= r2; ++r) {
        if (r < 0 || r >= grid.rows) continue;
        int startCol = (r == r1) ? c1 : 0;
        int endCol = (r == r2) ? c2 : grid.cols - 1;
        startCol = qBound(0, startCol, grid.cols - 1);
        endCol = qBound(0, endCol, grid.cols - 1);

        int x = gx + startCol * m_cellWidth;
        int y = gy + r * m_cellHeight;
        int w = (endCol - startCol + 1) * m_cellWidth;
        p.drawRect(x, y, w, m_cellHeight);
    }
    p.restore();
}
