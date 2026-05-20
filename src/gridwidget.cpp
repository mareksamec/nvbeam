#include "gridwidget.h"
#include <QPainter>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>

GridWidget::GridWidget(QWidget *parent)
    : QWidget(parent)
{
    m_font = QFont("Monospace", 10);
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);

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
    } else if (name == "mode_info_set") {
        handleModeInfoSet(args);
    } else if (name == "mode_change") {
        handleModeChange(args);
    } else if (name == "busy_start") {
        m_busy = true;
    } else if (name == "busy_stop") {
        m_busy = false;
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
    // [grid, row, col_start, cells, wrap]
    int row = args[1].toInt();
    int col = args[2].toInt();
    QVariantList cells = args[3].toList();

    if (row < 0 || row >= m_rows) return;

    int hlId = 0;
    for (const auto &cellVar : cells) {
        QVariantList cell = cellVar.toList();
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
    // Use invalid color as sentinel for "use default"
    hl.foreground = QColor();
    hl.background = QColor();

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
            HlAttr hl;
            hl.foreground = m_defaultFg;
            hl.background = m_defaultBg;
            if (cell.hlId != 0 && m_hlAttrs.contains(cell.hlId)) {
                hl = m_hlAttrs[cell.hlId];
                if (!m_hlAttrs[cell.hlId].foreground.isValid())
                    hl.foreground = m_defaultFg;
                if (!m_hlAttrs[cell.hlId].background.isValid())
                    hl.background = m_defaultBg;
            }

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

    // Draw cursor based on mode
    if (!m_busy && m_cursorRow < m_rows && m_cursorCol < m_cols) {
        int x = m_cursorCol * m_cellWidth;
        int y = m_cursorRow * m_cellHeight;

        QString shape = "block";
        int pct = 100;
        if (m_currentMode < m_modeInfoList.size()) {
            shape = m_modeInfoList[m_currentMode].cursorShape;
            pct = m_modeInfoList[m_currentMode].cellPercentage;
        }

        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        QColor cursorColor(255, 255, 255, 180);

        if (shape == "block") {
            painter.fillRect(x, y, m_cellWidth, m_cellHeight, cursorColor);
            // Draw the character under cursor in inverted color
            if (m_cursorCol < m_cols) {
                painter.setPen(m_defaultBg);
                painter.setFont(m_font);
                painter.drawText(x, y + m_cellHeight - painter.fontMetrics().descent(),
                                 m_grid[m_cursorRow][m_cursorCol].text);
            }
        } else if (shape == "vertical") {
            int w = qMax(1, m_cellWidth * pct / 100);
            painter.fillRect(x, y, w, m_cellHeight, cursorColor);
        } else if (shape == "horizontal") {
            int h = qMax(1, m_cellHeight * pct / 100);
            painter.fillRect(x, y + m_cellHeight - h, m_cellWidth, h, cursorColor);
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
    if (newCols > 0 && newRows > 0 && (newCols != m_cols || newRows != m_rows)) {
        emit gridSizeReady(newRows, newCols);
    }
}

void GridWidget::handleModeInfoSet(const QVariantList &args)
{
    if (args.size() < 2) return;
    // [cursor_style_enabled, mode_info_list]
    QVariantList modeList = args[1].toList();
    m_modeInfoList.clear();
    for (const auto &modeVar : modeList) {
        QVariantMap m = modeVar.toMap();
        ModeInfo info;
        QString shape = m.value("cursor_shape", "block").toString();
        if (shape == "block") info.cursorShape = "block";
        else if (shape == "vertical") info.cursorShape = "vertical";
        else if (shape == "horizontal") info.cursorShape = "horizontal";
        info.cellPercentage = m.value("cell_percentage", 100).toInt();
        m_modeInfoList.append(info);
    }
}

void GridWidget::handleModeChange(const QVariantList &args)
{
    if (args.size() < 2) return;
    // [mode_name, mode_idx]
    m_currentMode = args[1].toInt();
}

// --- Keyboard input ---

static const QHash<int, QString> &specialKeyMap()
{
    static const QHash<int, QString> map = {
        {Qt::Key_Escape, "Esc"},
        {Qt::Key_Return, "CR"},
        {Qt::Key_Enter, "CR"},
        {Qt::Key_Backspace, "BS"},
        {Qt::Key_Tab, "Tab"},
        {Qt::Key_Backtab, "Tab"}, // Shift+Tab handled via modifier
        {Qt::Key_Space, "Space"},
        {Qt::Key_Up, "Up"},
        {Qt::Key_Down, "Down"},
        {Qt::Key_Left, "Left"},
        {Qt::Key_Right, "Right"},
        {Qt::Key_Home, "Home"},
        {Qt::Key_End, "End"},
        {Qt::Key_PageUp, "PageUp"},
        {Qt::Key_PageDown, "PageDown"},
        {Qt::Key_Insert, "Insert"},
        {Qt::Key_Delete, "Del"},
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

    // Check if it's a known special key
    const auto &map = specialKeyMap();
    if (map.contains(key)) {
        QString name = map[key];
        QString mod = modifierPrefix(mods, true);
        if (key == Qt::Key_Backtab) mod += "S-"; // Backtab is Shift+Tab
        return "<" + mod + name + ">";
    }

    // Control+letter
    if ((mods & Qt::ControlModifier) && key >= Qt::Key_A && key <= Qt::Key_Z) {
        QString mod = modifierPrefix(mods, true);
        QString ch = QString(QChar('a' + (key - Qt::Key_A)));
        return "<" + mod + ch + ">";
    }

    // Alt+key
    if ((mods & Qt::AltModifier) && !text.isEmpty()) {
        QString mod = modifierPrefix(mods, true);
        return "<" + mod + text + ">";
    }

    // Regular text
    if (!text.isEmpty() && text[0].isPrint()) {
        if (text == "<") return "<lt>";
        return text;
    }

    return {};
}

void GridWidget::keyPressEvent(QKeyEvent *event)
{
    QString keys = keyToNvim(event);
    if (!keys.isEmpty()) {
        emit inputKey(keys);
    }
}

// --- Mouse input ---

QPair<int,int> GridWidget::cellAt(const QPoint &pos)
{
    int col = pos.x() / m_cellWidth;
    int row = pos.y() / m_cellHeight;
    col = qBound(0, col, m_cols - 1);
    row = qBound(0, row, m_rows - 1);
    return {row, col};
}

void GridWidget::mousePressEvent(QMouseEvent *event)
{
    auto [row, col] = cellAt(event->pos());
    m_mousePressed = true;

    QString button;
    if (event->button() == Qt::LeftButton) button = "left";
    else if (event->button() == Qt::RightButton) button = "right";
    else if (event->button() == Qt::MiddleButton) button = "middle";
    else return;

    QString mod = modifierPrefix(event->modifiers(), false);
    emit inputMouse(button, "press", mod, 0, row, col);
}

void GridWidget::mouseReleaseEvent(QMouseEvent *event)
{
    auto [row, col] = cellAt(event->pos());
    m_mousePressed = false;

    QString button;
    if (event->button() == Qt::LeftButton) button = "left";
    else if (event->button() == Qt::RightButton) button = "right";
    else if (event->button() == Qt::MiddleButton) button = "middle";
    else return;

    QString mod = modifierPrefix(event->modifiers(), false);
    emit inputMouse(button, "release", mod, 0, row, col);
}

void GridWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_mousePressed) return;
    auto [row, col] = cellAt(event->pos());
    QString mod = modifierPrefix(event->modifiers(), false);
    emit inputMouse("left", "drag", mod, 0, row, col);
}

void GridWidget::wheelEvent(QWheelEvent *event)
{
    auto [row, col] = cellAt(event->position().toPoint());
    QString mod = modifierPrefix(event->modifiers(), false);
    QString button = event->angleDelta().y() > 0 ? "wheel_up" : "wheel_down";
    emit inputMouse(button, "press", mod, 0, row, col);
}
