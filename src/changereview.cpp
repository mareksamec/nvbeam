#include "changereview.h"

#include "colorscheme.h"
#include "shortcuts.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <memory>

#ifdef NEOBEAM_HAS_SYNTAX
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/Theme>
#endif
#include <QResizeEvent>
#include <QFrame>
#include <QPainter>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QScrollArea>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextBlockUserData>
#include <QToolBar>
#include <QToolButton>
#include <QHBoxLayout>
#include <QBuffer>
#include <QImageReader>
#include <QVBoxLayout>

namespace {

/// Shortest poll interval accepted, so a mistyped config cannot spawn a git
/// process per frame.
constexpr int kMinRefreshMs = 250;

/// How many timeouts a git run has to overrun before the watchdog in collect()
/// stops waiting for it and lets a new run start.
constexpr int kWatchdogFactor = 2;

/// Gap between two file rows, on top of the band each row's header carries.
constexpr int kRowSpacing = 3;

/// Space left around the line numbers in their gutter, in pixels.
constexpr int kGutterPadding = 6;

/// The stripe painted across a changed line. Set directly rather than blended
/// from the diff colors: the stripe has to stay dark enough to read code on,
/// which is a different job from the marker color it would otherwise follow.
const QColor kAddedStripe{"#2a3e29"};
const QColor kRemovedStripe{"#4d3030"};

/// Shortest an expanded body is ever made, so an empty or one-line diff still
/// shows as an open row rather than a sliver.
constexpr int kMinBodyHeight = 24;

/// Bytes of an untracked file read for its preview. Past this the file is
/// almost certainly not something to review line by line.
constexpr qint64 kMaxUntrackedBytes = 1 << 20;

/// Side of one checkerboard square drawn behind a transparent image.
constexpr int kCheckerSize = 8;

/// Padding around an image inside its pane.
constexpr int kPanePadding = 4;

/// Shortest an image pane is made, so a 16x16 icon still has a visible frame.
constexpr int kMinPaneHeight = 48;

/// Below this width the two image panes stack instead of sitting side by side:
/// half of a narrow panel is not enough of an image to compare.
constexpr int kStackWidth = 420;
// Stacking makes the preview taller, which can make the panel's scrollbar
// appear and take the width back below kStackWidth; without a band between the
// two decisions the layout would flip back and forth forever.
constexpr int kStackHysteresis = 48;

/// A byte count as a reader would write it (git's own diff stats round the
/// same way).
QString formatBytes(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 kB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

/// True when this build can decode the suffix. Asked of QImageReader rather
/// than kept as a list, so a build with extra image plugins gains them and one
/// without WEBP does not promise it.
bool decodableImageSuffix(const QString &path)
{
    const QByteArray suffix = QFileInfo(path).suffix().toLower().toUtf8();
    if (suffix.isEmpty())
        return false;
    // SVG is deliberately not here: rendering it needs Qt6::Svg, a dependency
    // the build-time icon rasterization exists to avoid, and an SVG diff reads
    // perfectly well as the text it is.
    if (suffix == "svg" || suffix == "svgz")
        return false;
    return QImageReader::supportedImageFormats().contains(suffix);
}

/// Decode `bytes`, refusing anything past `maxBytes` before the decode rather
/// than after it: QImageReader knows the dimensions without doing the work.
QImage decodeImage(const QByteArray &bytes, qint64 maxBytes, QString *why)
{
    if (bytes.isEmpty())
        return QImage();
    if (maxBytes > 0 && bytes.size() > maxBytes) {
        if (why)
            *why = QStringLiteral("%1, over [review] image_max_bytes")
                       .arg(formatBytes(bytes.size()));
        return QImage();
    }
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull() && why)
        *why = reader.errorString();
    return image;
}

/// The monospace font the diff bodies use.
QFont diffFont(const ReviewConfig &cfg)
{
    if (cfg.fontFamily.isEmpty())
        return QFontDatabase::systemFont(QFontDatabase::FixedFont);
    QFont font(cfg.fontFamily, cfg.fontSize);
    font.setStyleHint(QFont::Monospace);
    return font;
}

#ifdef NEOBEAM_HAS_SYNTAX

/// The lexer state a line ended in, kept so the next line continues it (a
/// string or comment running across lines). QSyntaxHighlighter's own int state
/// cannot hold it.
class DiffBlockState : public QTextBlockUserData
{
public:
    KSyntaxHighlighting::State state;
};

/// One repository for the process: constructing it reads the definition files,
/// which is far too much work to repeat per file row.
KSyntaxHighlighting::Repository &syntaxRepository()
{
    static KSyntaxHighlighting::Repository repo;
    return repo;
}

#endif // NEOBEAM_HAS_SYNTAX

} // namespace

DiffHighlighter::DiffHighlighter(const ReviewConfig &cfg, QTextDocument *document)
    : QSyntaxHighlighter(document), m_cfg(cfg)
{
    m_added.setForeground(cfg.added);
    m_removed.setForeground(cfg.removed);
    m_hunk.setForeground(cfg.hunk);
#ifdef NEOBEAM_HAS_SYNTAX
    // A theme matching the panel, which is the terminal's scheme: a light
    // theme's colors would be unreadable on a dark terminal and the other way
    // round.
    setTheme(syntaxRepository().defaultTheme(
        cfg.background.lightness() < 128 ? KSyntaxHighlighting::Repository::DarkTheme
                                         : KSyntaxHighlighting::Repository::LightTheme));
#endif
}

void DiffHighlighter::setFileName(const QString &path)
{
#ifdef NEOBEAM_HAS_SYNTAX
    if (!m_cfg.syntax)
        return;
    const KSyntaxHighlighting::Definition def =
        syntaxRepository().definitionForFileName(path);
    if (def.name() == definition().name())
        return;
    setDefinition(def);
    rehighlight();
#else
    Q_UNUSED(path);
#endif
}

void DiffHighlighter::highlightBlock(const QString &text)
{
    if (text.isEmpty())
        return;
    // The file header lines are not in the body (the row's own header carries
    // the name), so a leading "+"/"-" here is always a changed line.
    const bool hunk = text.startsWith(QLatin1String("@@"));
    const bool added = !hunk && text.startsWith(QLatin1Char('+'));
    const bool removed = !hunk && text.startsWith(QLatin1Char('-'));
    const bool context = !hunk && !added && !removed && text.startsWith(QLatin1Char(' '));

    if (hunk)
        setFormat(0, text.size(), m_hunk);
    else if (added)
        setFormat(0, text.size(), m_added);
    else if (removed)
        setFormat(0, text.size(), m_removed);

#ifdef NEOBEAM_HAS_SYNTAX
    if (!m_cfg.syntax || !definition().isValid())
        return;

    auto *data = new DiffBlockState;
    // A hunk header is diff syntax, not the file's language, and it also ends
    // whatever the lexer was in the middle of: the next hunk starts at an
    // arbitrary line of the file, so its state begins empty.
    if (hunk || !(added || removed || context)) {
        setCurrentBlockUserData(data);
        return;
    }

    KSyntaxHighlighting::State previous;
    const QTextBlock prev = currentBlock().previous();
    if (prev.isValid()) {
        if (auto *prevData = dynamic_cast<DiffBlockState *>(prev.userData()))
            previous = prevData->state;
    }

    // The marker column is not part of the code, so the language never sees
    // it; applyFormat() shifts the token offsets back over it.
    m_offset = 1;
    data->state = highlightLine(QStringView(text).mid(m_offset), previous);
    setCurrentBlockUserData(data);
#else
    Q_UNUSED(context);
#endif
}

#ifdef NEOBEAM_HAS_SYNTAX
void DiffHighlighter::applyFormat(int offset, int length,
                                  const KSyntaxHighlighting::Format &format)
{
    if (length <= 0)
        return;
    QTextCharFormat out;
    if (format.hasTextColor(theme()))
        out.setForeground(format.textColor(theme()));
    else
        out.setForeground(m_cfg.foreground);
    if (format.isBold(theme()))
        out.setFontWeight(QFont::Bold);
    if (format.isItalic(theme()))
        out.setFontItalic(true);
    setFormat(offset + m_offset, length, out);
}
#endif

// --- DiffBody ---------------------------------------------------------------

namespace {

/// The strip drawn to the left of the diff. It holds no state: painting is the
/// editor's job, which is the only side that knows which lines are on screen.
class Gutter : public QWidget
{
public:
    explicit Gutter(DiffBody *body) : QWidget(body), m_body(body) {}

protected:
    void paintEvent(QPaintEvent *event) override { m_body->paintGutter(event); }

private:
    DiffBody *m_body;
};

} // namespace

DiffBody::DiffBody(const ReviewConfig &cfg, QWidget *parent)
    : QPlainTextEdit(parent), m_cfg(cfg)
{
    m_gutter = new Gutter(this);
    // The gutter follows the viewport: a new line count changes its width, and
    // scrolling or an edited region repaints the part that moved.
    connect(this, &QPlainTextEdit::blockCountChanged, this, [this] { layOutGutter(); });
    connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &rect, int dy) {
        if (dy != 0)
            m_gutter->scroll(0, dy);
        else
            m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());
    });
    layOutGutter();
}

void DiffBody::setLineNumbers(const QList<int> &numbers)
{
    m_numbers = numbers;
    int widest = 0;
    for (int number : numbers)
        widest = qMax(widest, number);
    m_digits = qMax(1, QString::number(widest).size());
    layOutGutter();
    m_gutter->update();
}

int DiffBody::gutterWidth() const
{
    return kGutterPadding * 2 + m_digits * fontMetrics().horizontalAdvance(QLatin1Char('9'));
}

void DiffBody::layOutGutter()
{
    // setViewportMargins() re-lays out the scroll area unconditionally, and
    // that lands back here through resizeEvent(). Reserving the margin only
    // when its width really changed ends that chain instead of recursing
    // until the stack runs out.
    const int width = gutterWidth();
    if (width != m_margin) {
        m_margin = width;
        setViewportMargins(width, 0, 0, 0);
    }
    const QRect area = contentsRect();
    const QRect place(area.left(), area.top(), width, area.height());
    if (m_gutter->geometry() != place)
        m_gutter->setGeometry(place);
}

void DiffBody::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    layOutGutter();
}

void DiffBody::paintGutter(QPaintEvent *event)
{
    QPainter painter(m_gutter);
    painter.fillRect(event->rect(), m_cfg.background);
    painter.setFont(font());

    QTextBlock block = firstVisibleBlock();
    int top = int(blockBoundingGeometry(block).translated(contentOffset()).top());
    while (block.isValid() && top <= event->rect().bottom()) {
        const int height = int(blockBoundingRect(block).height());
        const int index = block.blockNumber();
        const int number = index < m_numbers.size() ? m_numbers.at(index) : 0;
        if (block.isVisible() && top + height >= event->rect().top()) {
            // The line's own text says what it is, so a changed line's stripe
            // and number color are read straight off it: the gutter then
            // continues the stripe behind the diff rather than cutting it off.
            const QString line = block.text();
            const bool added = line.startsWith(QLatin1Char('+'));
            const bool removed = !added && line.startsWith(QLatin1Char('-'));
            if (added || removed)
                painter.fillRect(0, top, m_gutter->width(), height,
                                 added ? kAddedStripe : kRemovedStripe);
            if (number > 0) {
                painter.setPen(added ? m_cfg.added : (removed ? m_cfg.removed : m_cfg.dim));
                // Right-aligned against the text, and on the first line of a
                // wrapped block, which is where the line actually starts.
                painter.drawText(0, top, m_gutter->width() - kGutterPadding,
                                 int(blockBoundingRect(block).height() / block.lineCount()),
                                 Qt::AlignRight | Qt::AlignTop, QString::number(number));
            }
        }
        top += height;
        block = block.next();
    }
}

// --- ImagePane --------------------------------------------------------------

ImagePane::ImagePane(const ReviewConfig &cfg, QWidget *parent)
    : QWidget(parent), m_cfg(cfg)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(kMinPaneHeight);
}

void ImagePane::setImage(const QImage &image, const QString &note)
{
    m_image = image;
    m_note = note;
    update();
}

void ImagePane::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QRect area = rect().adjusted(kPanePadding, kPanePadding, -kPanePadding, -kPanePadding);
    painter.fillRect(rect(), m_cfg.background);

    if (m_image.isNull()) {
        painter.setPen(m_cfg.dim);
        painter.drawText(area, Qt::AlignCenter | Qt::TextWordWrap, m_note);
        return;
    }

    // Never scaled up: a 16x16 icon shown at 200px would be a blurry square
    // that says less about the change than the icon itself does.
    QSize target = m_image.size();
    if (target.width() > area.width() || target.height() > area.height())
        target.scale(area.size(), Qt::KeepAspectRatio);
    const QRect place(area.x() + (area.width() - target.width()) / 2,
                      area.y() + (area.height() - target.height()) / 2,
                      target.width(), target.height());

    // Checkerboard first, so transparency reads as transparency instead of
    // taking the panel's background and looking like part of the image.
    if (m_image.hasAlphaChannel()) {
        const QColor light = m_cfg.rowHeader;
        const QColor dark = m_cfg.rowBorder;
        painter.save();
        painter.setClipRect(place);
        for (int y = place.top(); y < place.bottom() + kCheckerSize; y += kCheckerSize) {
            for (int x = place.left(); x < place.right() + kCheckerSize; x += kCheckerSize) {
                const bool even = ((x - place.left()) / kCheckerSize
                                   + (y - place.top()) / kCheckerSize) % 2 == 0;
                painter.fillRect(x, y, kCheckerSize, kCheckerSize, even ? light : dark);
            }
        }
        painter.restore();
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(place, m_image);
}

// --- ImagePreview -----------------------------------------------------------

ImagePreview::ImagePreview(const ReviewConfig &cfg, QWidget *parent)
    : QWidget(parent), m_cfg(cfg)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPanePadding, kPanePadding, kPanePadding, kPanePadding);
    layout->setSpacing(kPanePadding);

    m_panes = new QBoxLayout(QBoxLayout::LeftToRight);
    m_panes->setSpacing(kPanePadding * 2);

    const QString captionStyle = QStringLiteral("QLabel { color: %1; }").arg(m_cfg.dim.name());
    auto column = [&](ImagePane *&pane, QLabel *&caption) {
        auto *box = new QVBoxLayout;
        box->setSpacing(2);
        pane = new ImagePane(m_cfg, this);
        caption = new QLabel(this);
        caption->setAlignment(Qt::AlignHCenter);
        caption->setStyleSheet(captionStyle);
        box->addWidget(pane, 1);
        box->addWidget(caption);
        m_panes->addLayout(box, 1);
    };
    column(m_oldPane, m_oldCaption);
    column(m_newPane, m_newCaption);
    layout->addLayout(m_panes, 1);

    m_summary = new QLabel(this);
    m_summary->setAlignment(Qt::AlignHCenter);
    m_summary->setStyleSheet(captionStyle);
    m_summary->hide();
    layout->addWidget(m_summary);
}

void ImagePreview::setOld(const QByteArray &bytes, const QString &note)
{
    fill(m_oldPane, m_oldCaption, bytes, note, QStringLiteral("added"));
    updateSummary();
}

void ImagePreview::setNew(const QByteArray &bytes, const QString &note)
{
    fill(m_newPane, m_newCaption, bytes, note, QStringLiteral("deleted"));
    updateSummary();
}

void ImagePreview::fill(ImagePane *pane, QLabel *caption, const QByteArray &bytes,
                        const QString &note, const QString &absent)
{
    if (bytes.isEmpty()) {
        // No such version: one side of an added or deleted file. Saying which
        // beats an empty frame the reader has to interpret.
        pane->setImage(QImage(), note.isEmpty() ? absent : note);
        caption->clear();
        return;
    }
    QString why;
    const QImage image = decodeImage(bytes, m_cfg.imageMaxBytes, &why);
    if (image.isNull()) {
        pane->setImage(QImage(), why.isEmpty() ? QStringLiteral("not decodable") : why);
        caption->setText(formatBytes(bytes.size()));
        return;
    }
    pane->setImage(image);
    caption->setText(QStringLiteral("%1\u00d7%2 \u00b7 %3")
                         .arg(image.width())
                         .arg(image.height())
                         .arg(formatBytes(bytes.size())));
}

void ImagePreview::updateSummary()
{
    if (!m_oldPane->hasImage() || !m_newPane->hasImage()
        || m_oldPane->imageSize() == m_newPane->imageSize()) {
        m_summary->hide();
        return;
    }
    m_summary->setText(QStringLiteral("%1\u00d7%2 \u2192 %3\u00d7%4")
                           .arg(m_oldPane->imageSize().width())
                           .arg(m_oldPane->imageSize().height())
                           .arg(m_newPane->imageSize().width())
                           .arg(m_newPane->imageSize().height()));
    m_summary->show();
}

int ImagePreview::preferredHeight() const
{
    // The panes never scale an image up, so their natural height is the taller
    // image (or the sum of both once they are stacked).
    const int oldHeight = m_oldPane->hasImage() ? m_oldPane->imageSize().height() : kMinPaneHeight;
    const int newHeight = m_newPane->hasImage() ? m_newPane->imageSize().height() : kMinPaneHeight;
    const bool stacked = m_panes->direction() == QBoxLayout::TopToBottom;
    int panes = stacked ? oldHeight + newHeight + m_panes->spacing()
                        : qMax(oldHeight, newHeight);
    panes += 2 * kPanePadding; // the pane's own padding around the image
    int chrome = 2 * kPanePadding + m_oldCaption->sizeHint().height() * (stacked ? 2 : 1);
    if (m_summary->isVisible())
        chrome += m_summary->sizeHint().height() + kPanePadding;
    return qBound(kMinPaneHeight, panes + chrome, m_cfg.fileMaxHeight);
}

void ImagePreview::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Hysteresis: stack below kStackWidth, go back side by side only once the
    // width is clear of it again.
    const bool stacked = m_panes->direction() == QBoxLayout::TopToBottom;
    const bool wantStacked = stacked ? width() < kStackWidth + kStackHysteresis
                                     : width() < kStackWidth;
    if (wantStacked != stacked) {
        m_panes->setDirection(wantStacked ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        emit layoutChanged();
    }
}

// --- FileCard ---------------------------------------------------------------

FileCard::FileCard(const ReviewConfig &cfg, const FileDiff &file, QWidget *parent)
    : QWidget(parent), m_cfg(cfg), m_file(file), m_context(cfg.showContext)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header row: the arrow doubles as the expander, so the whole row is one
    // click target rather than a separate button plus a label. It carries a
    // band across the panel's full width, which is what separates one file
    // from the next once several are expanded.
    auto *row = new QWidget(this);
    row->setObjectName(QStringLiteral("fileCardHeader"));
    // A plain QWidget does not paint a style sheet background on its own.
    row->setAttribute(Qt::WA_StyledBackground, true);
    // The whole band lights up under the pointer, not the name button alone,
    // so the counts at its right end never sit on a different grey.
    row->setAttribute(Qt::WA_Hover, true);
    row->setStyleSheet(QStringLiteral(
        "QWidget#fileCardHeader { background: %1; border-bottom: 1px solid %2; }"
        "QWidget#fileCardHeader:hover { background: %3; }")
            .arg(m_cfg.rowHeader.name(), m_cfg.rowBorder.name(), m_cfg.hover.name()));
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(2, 1, 4, 1);
    rowLayout->setSpacing(4);

    m_header = new QToolButton(row);
    m_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_header->setArrowType(Qt::RightArrow);
    m_header->setAutoRaise(true);
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // Transparent, so the band behind it (and its hover state) shows through.
    m_header->setStyleSheet(QStringLiteral(
        "QToolButton { color: %1; background: transparent; border: none; padding: 1px 2px;"
        " text-align: left; }")
            .arg(m_cfg.file.name()));
    connect(m_header, &QToolButton::clicked, this, [this] { setExpanded(!isExpanded()); });
    rowLayout->addWidget(m_header, 1);

    // Per-file context toggle: the body opens on the changed lines only, and
    // this brings the surrounding hunk lines back (the review pane in the
    // Claude Code client hides them the same way).
    m_contextBtn = new QToolButton(row);
    m_contextBtn->setText(QStringLiteral("⋯"));
    m_contextBtn->setCheckable(true);
    m_contextBtn->setChecked(m_context);
    m_contextBtn->setAutoRaise(true);
    m_contextBtn->setToolTip(QStringLiteral("Show unchanged context lines"));
    m_contextBtn->setStyleSheet(QStringLiteral(
        "QToolButton { color: %1; background: transparent; border: none; padding: 1px 4px; }"
        "QToolButton:checked { color: %2; background: %3; border-radius: 3px; }")
            .arg(m_cfg.dim.name(), m_cfg.foreground.name(), m_cfg.rowBorder.name()));
    connect(m_contextBtn, &QToolButton::toggled, this, [this](bool on) {
        m_context = on;
        applyBody();
    });
    rowLayout->addWidget(m_contextBtn);

    m_stats = new QLabel(row);
    m_stats->setTextFormat(Qt::RichText);
    m_stats->setStyleSheet(QStringLiteral("QLabel { background: transparent; padding: 1px 2px; }"));
    rowLayout->addWidget(m_stats);
    layout->addWidget(row);

    m_body = new DiffBody(m_cfg, this);
    // No document margin: it would leave an unpainted strip between the
    // gutter and the start of a line's stripe.
    m_body->document()->setDocumentMargin(0);
    m_body->setReadOnly(true);
    m_body->setUndoRedoEnabled(false);
    m_body->setFrameStyle(QFrame::NoFrame);
    // Wrapped, so a long line stays readable in a narrow panel; the height is
    // then computed from the wrapped document (fitBody).
    m_body->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_body->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_body->setFont(diffFont(m_cfg));
    m_body->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; selection-background-color: %3; }")
            .arg(m_cfg.background.name(), m_cfg.foreground.name(),
                 m_cfg.selection.name()));
    m_body->hide();
    layout->addWidget(m_body);

    // Closes the file block off, so two files in a row do not run together
    // once both are expanded.
    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::NoFrame);
    separator->setFixedHeight(1);
    separator->setStyleSheet(QStringLiteral("background: %1;").arg(m_cfg.rowBorder.name()));
    layout->addWidget(separator);

    m_highlighter = new DiffHighlighter(m_cfg, m_body->document());
    m_highlighter->setFileName(file.path);

    setFile(file);
}

void FileCard::setFile(const FileDiff &file)
{
    const bool bodyChanged = file.body != m_file.body;
    const bool pathChanged = file.path != m_file.path;
    m_file = file;
    if (pathChanged)
        m_highlighter->setFileName(m_file.path);

    QString label = m_file.name;
    if (!m_file.dir.isEmpty())
        label += QStringLiteral("  %1").arg(m_file.dir);
    m_header->setText(label);
    m_header->setToolTip(m_file.path);

    QString stats;
    if (!m_file.tag.isEmpty())
        stats += QStringLiteral("<span style='color:%1'>%2</span> ")
                     .arg(m_cfg.dim.name(), m_file.tag);
    if (showsImage()) {
        // git counts no lines in a binary file, so "+0 -0" would be all the
        // header had to say. Its size on disk is the number that means
        // something; the panes below carry the dimensions.
        const qint64 size = QFileInfo(m_file.absolutePath).size();
        stats += QStringLiteral("<span style='color:%1'>%2</span>")
                     .arg(m_cfg.dim.name(), size > 0 ? formatBytes(size) : QStringLiteral("image"));
    } else {
        stats += QStringLiteral("<span style='color:%1'>+%2</span> "
                                "<span style='color:%3'>-%4</span>")
                     .arg(m_cfg.added.name()).arg(m_file.added)
                     .arg(m_cfg.removed.name()).arg(m_file.removed);
    }
    m_stats->setText(stats);

    // An untracked file's content is read on demand, so its body has to be
    // re-read only when the row is open.
    if (m_file.untracked && bodyChanged)
        m_loaded = false;
    // The base version is cached for as long as the diff stands still; a diff
    // that moved means the image did, so it is fetched again.
    if (bodyChanged || pathChanged)
        m_oldImageAsked = false;
    // Rebuilding the body is the expensive half of a refresh (the text goes
    // through the document layout and the highlighter), so it waits until the
    // panel asks for it - which it only does for rows on screen.
    if (bodyChanged && isExpanded())
        m_pendingBody = true;
}

void FileCard::renderPending()
{
    if (!m_pendingBody)
        return;
    applyBody();
}

void FileCard::setExpanded(bool expanded)
{
    if (expanded == isExpanded())
        return;
    m_header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    m_body->setVisible(expanded && !showsImage());
    if (m_preview)
        m_preview->setVisible(expanded && showsImage());
    // Same deferral as a refresh: expandAll() on a large diff would otherwise
    // build every body in one go and stall the event loop.
    if (expanded)
        m_pendingBody = true;
    emit expandedChanged(m_file.path, expanded);
}

bool FileCard::isExpanded() const
{
    return showsImage() && m_preview ? !m_preview->isHidden() : !m_body->isHidden();
}

void FileCard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!isExpanded())
        return;
    if (showsImage()) {
        // The panes may have just switched between side by side and stacked,
        // which changes the height they need.
        fitPreview();
        return;
    }
    fitBody();
}

FileCard::BodyText FileCard::bodyText() const
{
    // "@@ -old,count +new,count @@": the two numbers the lines after it are
    // counted from.
    static const QRegularExpression hunkHeader(
        QStringLiteral("^@@ -(\\d+)(?:,\\d+)? \\+(\\d+)"));

    BodyText out;
    QStringList kept;
    int oldNo = 1;
    int newNo = 1;
    const QStringList lines = m_file.body.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const bool hunk = line.startsWith(QLatin1String("@@"));
        const bool added = !hunk && line.startsWith(QLatin1Char('+'));
        const bool removed = !hunk && line.startsWith(QLatin1Char('-'));

        int number = 0;
        if (hunk) {
            const QRegularExpressionMatch match = hunkHeader.match(line);
            if (match.hasMatch()) {
                oldNo = match.captured(1).toInt();
                newNo = match.captured(2).toInt();
            }
        } else if (added) {
            number = newNo++;
        } else if (removed) {
            number = oldNo++;
        } else {
            // A context line exists in both files; its number in the new one is
            // what the reader is looking for. Counted even when it is hidden,
            // which is what keeps the numbers of the lines around it true.
            number = newNo++;
            ++oldNo;
        }

        if (!m_context && !(hunk || added || removed))
            continue;
        kept << line;
        out.numbers << number;
    }
    out.text = kept.join(QLatin1Char('\n'));
    return out;
}

bool FileCard::showsImage() const
{
    // Binary as far as git is concerned (or untracked, which git never diffs),
    // with a suffix this build can decode.
    return m_cfg.imagePreview && (m_file.binary || m_file.untracked)
           && decodableImageSuffix(m_file.path);
}

void FileCard::applyPreview()
{
    if (!m_preview) {
        m_preview = new ImagePreview(m_cfg, this);
        connect(m_preview, &ImagePreview::layoutChanged, this, &FileCard::fitPreview);
        // The same slot the diff body occupies: one of the two is shown at a
        // time, so the row keeps its shape either way.
        auto *box = static_cast<QVBoxLayout *>(layout());
        box->insertWidget(box->indexOf(m_body), m_preview);
    }
    m_body->hide();
    m_preview->show();

    // The new version is on disk, except for a deleted file, which no longer is.
    if (m_file.tag == QLatin1String("deleted")) {
        m_preview->setNew({});
    } else {
        QFile handle(m_file.absolutePath);
        if (!handle.open(QIODevice::ReadOnly))
            m_preview->setNew({}, QStringLiteral("not readable"));
        else if (m_cfg.imageMaxBytes > 0 && handle.size() > m_cfg.imageMaxBytes)
            m_preview->setNew({}, QStringLiteral("%1, over [review] image_max_bytes")
                                      .arg(formatBytes(handle.size())));
        else
            m_preview->setNew(handle.readAll());
    }

    // An untracked file has no base version to fetch; a tracked one is asked
    // for once per change, since git's answer only moves when the diff does.
    if (m_file.untracked || m_file.tag == QLatin1String("new")) {
        m_preview->setOld({});
    } else if (!m_oldImageAsked) {
        m_oldImageAsked = true;
        m_preview->setOld({}, QStringLiteral("loading\u2026"));
        emit oldImageRequested(m_file.path);
    }
    fitPreview();
}

void FileCard::setOldImage(const QByteArray &bytes)
{
    if (!m_preview)
        return;
    m_preview->setOld(bytes);
    fitPreview();
}

void FileCard::fitPreview()
{
    // Same shape as fitBody(): giving the preview a height re-lays the panel
    // out, which resizes this card again while the height is still being
    // applied. Measure once per pass and apply only a height that differs, so
    // the card and the panel settle instead of driving each other.
    if (!m_preview || m_fitting)
        return;
    m_fitting = true;
    const QScopeGuard done([this] { m_fitting = false; });
    const int height = m_preview->preferredHeight();
    if (height != m_fittedPreview) {
        m_fittedPreview = height;
        m_preview->setFixedHeight(height);
    }
}

void FileCard::applyBody()
{
    m_pendingBody = false;
    if (showsImage()) {
        applyPreview();
        return;
    }
    if (m_preview) {
        m_preview->hide();
        m_body->show();
    }
    if (m_file.untracked && !m_loaded)
        loadUntracked();
    const BodyText body = bodyText();
    if (body.text != m_shown) {
        m_shown = body.text;
        const int scroll = m_body->verticalScrollBar()->value();
        m_body->setPlainText(body.text);
        m_body->setLineNumbers(body.numbers);
        m_body->verticalScrollBar()->setValue(scroll);
        paintChangedLines();
    }
    fitBody();
}

void FileCard::paintChangedLines()
{
    // Block formats rather than extra selections: a full-width extra selection
    // only reaches the first visual line of a wrapped block, leaving the rest
    // of a long changed line unmarked.
    QTextCursor cursor(m_body->document());
    cursor.beginEditBlock();
    for (QTextBlock block = m_body->document()->begin(); block.isValid();
         block = block.next()) {
        const QString line = block.text();
        const bool added = line.startsWith(QLatin1Char('+'));
        const bool removed = !added && line.startsWith(QLatin1Char('-'));
        QTextBlockFormat format = block.blockFormat();
        if (added || removed)
            format.setBackground(added ? kAddedStripe : kRemovedStripe);
        else
            format.clearBackground();
        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }
    cursor.endEditBlock();
}

void FileCard::fitBody()
{
    // Sizing the body changes the panel's layout, which can resize this card
    // again while the measurement is still running. Measure once per pass and
    // apply a height only when it differs, so the two sides settle.
    if (m_fitting)
        return;
    m_fitting = true;
    const QScopeGuard done([this] { m_fitting = false; });
    // A QPlainTextEdit's document reports its height in lines, not pixels, so
    // the pixel height has to be summed from the laid-out blocks. The layout
    // wraps at the widget's own width, which the panel's layout has already
    // set, and blockBoundingRect lays a block out on demand; the loop stops at
    // the cap, so a huge diff never lays out more than one screenful.
    const int cap = qMax(kMinBodyHeight, m_cfg.fileMaxHeight);
    QTextDocument *doc = m_body->document();
    QAbstractTextDocumentLayout *layout = doc->documentLayout();
    qreal content = 2 * doc->documentMargin();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        content += layout->blockBoundingRect(block).height();
        if (content >= cap)
            break;
    }
    // A pixel of slack: a fractional line height would otherwise round down
    // and give the body a scrollbar for the last line alone.
    const int height = qBound(kMinBodyHeight, int(content) + 1 + 2 * m_body->frameWidth(), cap);
    if (height != m_fitted) {
        m_fitted = height;
        m_body->setFixedHeight(height);
    }
}

void FileCard::loadUntracked()
{
    m_loaded = true;
    // An absolute path, since git reports the name relative to the repository
    // while the application's own working directory is something else.
    QFile file(m_file.absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_file.body = QStringLiteral("+ (cannot read %1)").arg(m_file.path);
        return;
    }
    if (file.size() > kMaxUntrackedBytes) {
        m_file.body = QStringLiteral("+ (%1 is too large to preview)").arg(m_file.path);
        return;
    }
    const QStringList lines =
        QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    QStringList body;
    const int limit = m_cfg.maxLines > 0 ? m_cfg.maxLines : lines.size();
    for (int i = 0; i < lines.size() && i < limit; ++i)
        body << QLatin1Char('+') + lines.at(i);
    if (lines.size() > limit)
        body << QStringLiteral("... truncated at %1 lines ([review] max_lines)").arg(limit);
    m_file.body = body.join(QLatin1Char('\n'));
}

// --- ChangeReviewPanel ------------------------------------------------------

ChangeReviewPanel::ChangeReviewPanel(const ReviewConfig &cfg, QWidget *parent)
    : QWidget(parent), m_cfg(cfg), m_mode(cfg.mode)
{
    const QString bg = m_cfg.background.name();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header: same chrome as the terminal panel's toolbar, so the two panels
    // sitting next to each other look like one piece of furniture.
    auto *toolbar = new QToolBar(this);
    toolbar->setMovable(false);
    toolbar->setStyleSheet(QStringLiteral(
        "QToolBar { background: %1; border: none; spacing: 4px; }"
        "QToolButton { color: %2; background: transparent; border: none; padding: 2px 6px; }"
        "QToolButton:hover { background: %3; border-radius: 3px; }")
            .arg(bg, m_cfg.dim.name(), m_cfg.hover.name()));

    m_stats = new QLabel(QStringLiteral("Changes"), toolbar);
    m_stats->setTextFormat(Qt::RichText);
    m_stats->setStyleSheet(QStringLiteral("color: %1; padding: 2px 6px;").arg(m_cfg.dim.name()));
    toolbar->addWidget(m_stats);

    auto *spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    spacer->setStyleSheet(QStringLiteral("background: transparent;"));
    toolbar->addWidget(spacer);

    // Worktree/branch toggle. A button rather than two actions: it carries the
    // active comparison as its label, which is where a reader looks to see
    // what the rows below are a diff of.
    m_modeBtn = new QToolButton(toolbar);
    m_modeBtn->setAutoRaise(true);
    m_modeBtn->setStyleSheet(QStringLiteral(
        "QToolButton { color: %1; background: transparent; border: none; padding: 2px 6px; }"
        "QToolButton:hover { background: %2; border-radius: 3px; }"
        "QToolButton:disabled { color: %3; }")
            .arg(m_cfg.dim.name(), m_cfg.hover.name(), m_cfg.rowBorder.name()));
    connect(m_modeBtn, &QToolButton::clicked, this, &ChangeReviewPanel::toggleMode);
    // An explicit base pins the comparison to that revision, so there is no
    // mode left to choose.
    m_modeBtn->setEnabled(m_cfg.base.isEmpty());
    updateModeButton();
    toolbar->addWidget(m_modeBtn);

    auto *expandAct = toolbar->addAction(QStringLiteral("⌄"), this, &ChangeReviewPanel::expandAll);
    expandAct->setToolTip(QStringLiteral("Expand all files"));
    auto *collapseAct = toolbar->addAction(QStringLiteral("⌃"), this,
                                           &ChangeReviewPanel::collapseAll);
    collapseAct->setToolTip(QStringLiteral("Collapse all files"));
    auto *refreshAct = toolbar->addAction(QStringLiteral("⟳"), this, &ChangeReviewPanel::refresh);
    refreshAct->setToolTip(QStringLiteral("Refresh now"));
    auto *closeAct = toolbar->addAction(QStringLiteral("✕"), this, &QWidget::hide);
    closeAct->setToolTip(QStringLiteral("Close review panel"));
    layout->addWidget(toolbar);

    // One scrolling column of file rows. The rows are widgets rather than tree
    // items so each carries a real text editor: selection and Ctrl+C inside a
    // diff then work the way they do anywhere else.
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameStyle(QFrame::NoFrame);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea { background: %1; border: none; }").arg(bg));
    m_list = new QWidget(m_scroll);
    m_list->setStyleSheet(QStringLiteral("background: %1;").arg(bg));
    m_listLayout = new QVBoxLayout(m_list);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(kRowSpacing);

    m_message = new QLabel(m_list);
    m_message->setWordWrap(true);
    m_message->setStyleSheet(QStringLiteral("color: %1; padding: 8px;").arg(m_cfg.dim.name()));
    m_message->hide();
    m_listLayout->addWidget(m_message);
    m_listLayout->addStretch(1);

    m_scroll->setWidget(m_list);
    // A row whose content changed off screen is rebuilt when it scrolls in.
    connect(m_scroll->verticalScrollBar(), &QAbstractSlider::valueChanged, this,
            [this] { renderVisibleRows(); });
    layout->addWidget(m_scroll, 1);

    // The poll is what keeps the view current while an agent writes files:
    // those writes touch neither .git nor an nvim buffer, so no push trigger
    // exists for them. It runs while the panel is visible (see show/hideEvent).
    m_poll.setInterval(qMax(m_cfg.refreshMs, kMinRefreshMs));
    connect(&m_poll, &QTimer::timeout, this, &ChangeReviewPanel::refresh);
}

void ChangeReviewPanel::setWorkingDirectory(const QString &dir)
{
    if (dir.isEmpty() || dir == m_dir)
        return;
    m_dir = dir;
    m_resolvedBranch.clear(); // another repository, another default branch
    if (isVisible())
        refresh();
}

void ChangeReviewPanel::refresh()
{
    collect();
}

void ChangeReviewPanel::repositoryChanged()
{
    // A commit, a branch switch or a fetch can all change which branch is the
    // default one (and a fetch can create origin/HEAD where there was none), so
    // the cached answer is dropped rather than carried into the next diff.
    m_resolvedBranch.clear();
    refresh();
}

void ChangeReviewPanel::toggleMode()
{
    if (!m_cfg.base.isEmpty())
        return;
    m_mode = m_mode == DiffMode::Branch ? DiffMode::Unstaged : DiffMode::Branch;
    updateModeButton();
    // Straight away rather than on the next tick: the user asked for the other
    // comparison, and with refresh_ms = 0 there is no next tick at all.
    refresh();
}

DiffMode ChangeReviewPanel::effectiveMode() const
{
    return m_cfg.base.isEmpty() ? m_mode : DiffMode::Unstaged;
}

void ChangeReviewPanel::updateModeButton()
{
    if (!m_modeBtn)
        return;
    const bool branch = effectiveMode() == DiffMode::Branch;
    m_modeBtn->setText(branch ? QStringLiteral("⎇ branch") : QStringLiteral("⎇ worktree"));
    const QString what = m_cfg.base.isEmpty()
        ? (branch ? QStringLiteral("Comparing the branch against its merge base; "
                                   "switch to the worktree against HEAD")
                  : QStringLiteral("Comparing the worktree against HEAD; "
                                   "switch to the branch against its merge base"))
        : QStringLiteral("Pinned to [review] base = \"%1\"").arg(m_cfg.base);
    m_modeBtn->setToolTip(m_cfg.base.isEmpty()
                              ? Shortcuts::hint(what, Shortcuts::toggleReviewDiffMode())
                              : what);
}

void ChangeReviewPanel::expandAll()
{
    for (FileCard *card : std::as_const(m_cards))
        card->setExpanded(true);
}

void ChangeReviewPanel::collapseAll()
{
    for (FileCard *card : std::as_const(m_cards))
        card->setExpanded(false);
}

void ChangeReviewPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
    if (m_cfg.refreshMs > 0)
        m_poll.start();
    emit visibilityChanged(true);
}

void ChangeReviewPanel::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_poll.stop();
    emit visibilityChanged(false);
}

void ChangeReviewPanel::runGit(const QStringList &args,
                               std::function<void(bool, const QString &)> done)
{
    runGitRaw(args, [done](bool ok, const QByteArray &output) {
        done(ok, QString::fromUtf8(output));
    });
}

void ChangeReviewPanel::runGitRaw(const QStringList &args,
                                  std::function<void(bool, const QByteArray &)> done)
{
    auto *proc = new QProcess(this);
    // The process object outlives whichever signal answered first, so it is
    // deleted by its own signals rather than by the callbacks below.
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            proc, &QObject::deleteLater);
    connect(proc, &QProcess::errorOccurred, proc, &QObject::deleteLater);

    // Exactly one answer per run. A run given up on is killed, which makes
    // finished() fire as well, and a second answer would clear m_running for a
    // run that is no longer the one in flight.
    auto answered = std::make_shared<bool>(false);
    auto answer = [answered, done](bool ok, const QByteArray &output) {
        if (*answered)
            return;
        *answered = true;
        done(ok, output);
    };

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [proc, answer](int code, QProcess::ExitStatus) {
        answer(code == 0, code == 0 ? proc->readAllStandardOutput()
                                    : proc->readAllStandardError());
    });
    // FailedToStart (git not installed) never reaches finished().
    connect(proc, &QProcess::errorOccurred, this,
            [answer](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            answer(false, QByteArrayLiteral("git could not be started"));
    });

    // Timeout: a git that never exits (a repository on a dead network mount, a
    // command waiting on a prompt) would otherwise hold the panel on the last
    // diff it managed to read, silently and for the rest of the session.
    if (m_cfg.timeoutMs > 0) {
        QTimer::singleShot(m_cfg.timeoutMs, proc, [this, proc, answer] {
            if (proc->state() == QProcess::NotRunning)
                return;
            proc->kill();
            answer(false, QStringLiteral("git took longer than %1 ms; giving up on this refresh")
                              .arg(m_cfg.timeoutMs).toUtf8());
        });
    }

    QStringList full{QStringLiteral("-C"), m_dir, QStringLiteral("--no-pager")};
    full += args;
    proc->start(QStringLiteral("git"), full);
}

void ChangeReviewPanel::collect()
{
    if (m_dir.isEmpty())
        return;
    if (m_running) {
        // Watchdog: the in-flight run should always answer, timeout included,
        // but a run that somehow outlives even that must not freeze the panel,
        // so it is disowned and a fresh one started.
        if (m_cfg.timeoutMs > 0 && m_runStarted.isValid()
            && m_runStarted.elapsed() > qint64(m_cfg.timeoutMs) * kWatchdogFactor) {
            qWarning("change review: a git run has been in flight for %lld ms; starting a new one",
                     m_runStarted.elapsed());
        } else { // one git run at a time; the burst collapses into one rerun
            m_rerun = true;
            return;
        }
    }
    m_running = true;
    m_runStarted.start();

    if (effectiveMode() != DiffMode::Branch) {
        // The worktree against HEAD (or against an explicit [review] base,
        // which is the one comparison that does not end at the worktree and so
        // has no untracked files to add).
        collectDiff(m_cfg.base, m_cfg.base.isEmpty() ? QStringLiteral("HEAD") : m_cfg.base,
                    m_cfg.base.isEmpty());
        return;
    }

    // Branch mode needs its base before it can diff, so one collect() becomes a
    // chain of runs. m_running stays held across the whole chain: releasing it
    // between the links would let a poll start a second chain over the first.
    resolveBaseBranch([this](bool ok, const QString &branch) {
        if (!ok) {
            failCollect(QStringLiteral("No base branch"),
                        QStringLiteral("No main or master branch to compare against. "
                                       "Name one with [review] base_branch."));
            return;
        }
        runGit({QStringLiteral("merge-base"), branch, QStringLiteral("HEAD")},
               [this, branch](bool found, const QString &output) {
            const QString mergeBase = output.trimmed();
            if (!found || mergeBase.isEmpty()) {
                // Unrelated histories, or a shallow clone whose depth does not
                // reach the fork point. Both look like "no changes" otherwise.
                failCollect(QStringLiteral("No common ancestor"),
                            QStringLiteral("No common ancestor with %1. A shallow clone may "
                                           "not reach the fork point.").arg(branch));
                return;
            }
            // Against the worktree rather than HEAD, so uncommitted work stays
            // visible when the mode is switched.
            collectDiff(mergeBase, branch, true);
        });
    });
}

void ChangeReviewPanel::collectDiff(const QString &base, const QString &label,
                                    bool includeUntracked)
{
    m_compareLabel = label;
    // What `git show <base>:<path>` in fetchOldImage() asks for. Empty means
    // the worktree comparison, whose base is HEAD.
    m_compareBase = base.isEmpty() ? QStringLiteral("HEAD") : base;
    QStringList args{QStringLiteral("diff")};
    if (!base.isEmpty())
        args << base;

    runGit(args, [this, includeUntracked](bool ok, const QString &output) {
        auto finish = [this] {
            m_running = false;
            if (m_rerun) {
                m_rerun = false;
                collect();
            }
        };
        if (!ok) {
            // Only git's first line: outside a repository it answers with its
            // whole `git diff` usage text, which is not what to show a reader.
            const QString first = output.section(QLatin1Char('\n'), 0, 0).trimmed();
            const bool noRepo = first.contains(QLatin1String("not a git repository"),
                                               Qt::CaseInsensitive);
            // apply() writes its own summary for an empty list, so the failure
            // summary is set after it rather than before.
            apply({});
            m_stats->setText(noRepo ? QStringLiteral("Not a repository")
                                    : QStringLiteral("git error"));
            showMessage(noRepo ? QStringLiteral("%1 is not a git repository.").arg(m_dir)
                               : (first.isEmpty() ? QStringLiteral("git failed.") : first));
            finish();
            return;
        }
        QList<FileDiff> files = parseDiff(output, m_cfg.maxLines);
        // An image row reads the working copy from disk, so every file needs
        // its absolute path, not only the untracked ones.
        for (FileDiff &file : files)
            file.absolutePath = QDir(m_dir).absoluteFilePath(file.path);
        // Untracked files never show up in `git diff`, so they are listed
        // separately. Only meaningful when the comparison ends at the
        // worktree, which an explicit [review] base is the one case that does
        // not: that one compares two revisions.
        if (!includeUntracked) {
            apply(files);
            finish();
            return;
        }
        runGit({QStringLiteral("ls-files"), QStringLiteral("--others"),
                QStringLiteral("--exclude-standard")},
               [this, files, finish](bool listed, const QString &untracked) mutable {
            if (listed) {
                const QStringList names =
                    untracked.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                for (const QString &name : names) {
                    FileDiff file;
                    file.path = name;
                    file.name = QFileInfo(name).fileName();
                    file.dir = QFileInfo(name).path();
                    if (file.dir == QLatin1String("."))
                        file.dir.clear();
                    file.tag = QStringLiteral("untracked");
                    file.untracked = true;
                    file.absolutePath = QDir(m_dir).absoluteFilePath(name);
                    // Counted without reading the file: its size in lines is
                    // what an "added" count would be anyway.
                    QFile handle(file.absolutePath);
                    if (handle.open(QIODevice::ReadOnly) && handle.size() <= kMaxUntrackedBytes)
                        file.added = QString::fromUtf8(handle.readAll())
                                         .count(QLatin1Char('\n'));
                    files << file;
                }
            }
            apply(files);
            finish();
        });
    });
}

void ChangeReviewPanel::resolveBaseBranch(std::function<void(bool, const QString &)> done)
{
    // Configured first: a repository whose trunk is called neither main nor
    // master has no other way to be reviewed.
    if (!m_cfg.baseBranch.isEmpty()) {
        done(true, m_cfg.baseBranch);
        return;
    }
    if (!m_resolvedBranch.isEmpty()) {
        done(true, m_resolvedBranch);
        return;
    }
    // What the remote itself calls its default branch, which beats guessing.
    runGit({QStringLiteral("symbolic-ref"), QStringLiteral("--quiet"), QStringLiteral("--short"),
            QStringLiteral("refs/remotes/origin/HEAD")},
           [this, done](bool ok, const QString &output) {
        const QString branch = output.trimmed();
        if (ok && !branch.isEmpty()) {
            m_resolvedBranch = branch;
            done(true, branch);
            return;
        }
        probeBranch(0, done);
    });
}

void ChangeReviewPanel::probeBranch(int index, std::function<void(bool, const QString &)> done)
{
    // Local names first: a repository with no remote still has a trunk, and a
    // local main is the branch the user actually merges into.
    static const QStringList candidates{QStringLiteral("main"), QStringLiteral("master"),
                                        QStringLiteral("origin/main"),
                                        QStringLiteral("origin/master")};
    if (index >= candidates.size()) {
        done(false, QString());
        return;
    }
    const QString candidate = candidates.at(index);
    runGit({QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("--quiet"),
            candidate + QStringLiteral("^{commit}")},
           [this, index, candidate, done](bool ok, const QString &output) {
        if (ok && !output.trimmed().isEmpty()) {
            m_resolvedBranch = candidate;
            done(true, candidate);
            return;
        }
        probeBranch(index + 1, done);
    });
}

void ChangeReviewPanel::fetchOldImage(const QString &path)
{
    // Outside the diff chain on purpose: it must not take the one-run-at-a-time
    // guard, or opening an image row would stall the poll behind it.
    runGitRaw({QStringLiteral("show"), m_compareBase + QLatin1Char(':') + path},
              [this, path](bool ok, const QByteArray &bytes) {
        if (FileCard *card = m_cards.value(path))
            card->setOldImage(ok ? bytes : QByteArray());
    });
}

void ChangeReviewPanel::failCollect(const QString &stats, const QString &message)
{
    apply({});
    m_stats->setText(stats);
    showMessage(message);
    m_running = false;
    if (m_rerun) {
        m_rerun = false;
        collect();
    }
}

void ChangeReviewPanel::apply(const QList<FileDiff> &files)
{
    int added = 0;
    int removed = 0;
    QStringList order;
    for (const FileDiff &file : files) {
        order << file.path;
        added += file.added;
        removed += file.removed;
    }

    // Drop rows for files that are no longer changed.
    for (auto it = m_cards.begin(); it != m_cards.end();) {
        if (order.contains(it.key())) {
            ++it;
            continue;
        }
        it.value()->deleteLater();
        it = m_cards.erase(it);
    }

    // Create or update the rest. An unchanged file's row is left untouched, so
    // a selection the user is reading inside it survives the refresh.
    for (const FileDiff &file : files) {
        FileCard *card = m_cards.value(file.path);
        if (card) {
            card->setFile(file);
            continue;
        }
        card = new FileCard(m_cfg, file, m_list);
        connect(card, &FileCard::expandedChanged, this,
                [this](const QString &path, bool expanded) {
            if (expanded)
                m_expanded.insert(path);
            else
                m_expanded.remove(path);
            renderVisibleRows(); // a row just opened may need its body built
        });
        connect(card, &FileCard::oldImageRequested, this, &ChangeReviewPanel::fetchOldImage);
        m_cards.insert(file.path, card);
        if (m_expanded.contains(file.path)) // reopened where the user had it open
            card->setExpanded(true);
    }

    // Re-lay the column only when the order actually changed: moving widgets
    // on every poll would fight with scrolling.
    if (order != m_order) {
        m_order = order;
        for (int i = 0; i < order.size(); ++i)
            m_listLayout->insertWidget(i + 1, m_cards.value(order.at(i))); // after m_message
    }

    // Now, and once more after the layout has placed the rows: a row created
    // by this very refresh still has no geometry to test against the viewport.
    renderVisibleRows();
    QTimer::singleShot(0, this, &ChangeReviewPanel::renderVisibleRows);

    if (files.isEmpty()) {
        m_stats->setText(QStringLiteral("No changes"));
        // Named after what is actually being compared, so the mode is readable
        // from the panel without looking at the toggle.
        showMessage(QStringLiteral("No changes against %1.")
                        .arg(m_compareLabel.isEmpty() ? QStringLiteral("HEAD") : m_compareLabel));
        return;
    }
    m_message->hide();
    m_stats->setText(QStringLiteral("%1 file%2, <span style='color:%3'>+%4</span> "
                                    "<span style='color:%5'>-%6</span>")
                         .arg(files.size())
                         .arg(files.size() == 1 ? QString() : QStringLiteral("s"),
                              m_cfg.added.name())
                         .arg(added)
                         .arg(m_cfg.removed.name())
                         .arg(removed));
}

void ChangeReviewPanel::renderVisibleRows()
{
    // Rebuilding a body changes its height, which re-lays the column out and
    // can scroll the viewport - which calls back in here. One pass at a time.
    if (m_rendering || !isVisible())
        return;
    m_rendering = true;
    const QScopeGuard done([this] { m_rendering = false; });

    // A screenful of margin on each side, so a row is ready by the time a
    // short scroll brings it in.
    const QRect viewport = m_scroll->viewport()->rect();
    const int margin = viewport.height();
    QRect wanted(m_list->mapFrom(m_scroll->viewport(), viewport.topLeft()), viewport.size());
    wanted.adjust(0, -margin, 0, margin);

    // One row per pass: several tall rows rebuilt back to back would block the
    // event loop long enough for the compositor to call the window dead, so
    // the rest are picked up on the next turn, after the loop has run.
    for (const QString &path : std::as_const(m_order)) {
        FileCard *card = m_cards.value(path);
        if (!card || !card->needsRender() || !card->geometry().intersects(wanted))
            continue;
        card->renderPending();
        QTimer::singleShot(0, this, &ChangeReviewPanel::renderVisibleRows);
        return;
    }
}

void ChangeReviewPanel::showMessage(const QString &text)
{
    m_message->setText(text);
    m_message->show();
}

QList<FileDiff> ChangeReviewPanel::parseDiff(const QString &diff, int maxLines)
{
    QList<FileDiff> files;
    if (diff.isEmpty())
        return files;

    FileDiff current;
    bool inFile = false;
    QStringList body;
    int kept = 0;
    bool truncated = false;

    auto flush = [&]() {
        if (!inFile)
            return;
        current.body = body.join(QLatin1Char('\n'));
        files << current;
        current = FileDiff{};
        body.clear();
        kept = 0;
        truncated = false;
    };

    const QStringList lines = diff.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("diff --git "))) {
            flush();
            inFile = true;
            // "diff --git a/<old> b/<new>": the b-side is the current path, and
            // is what a rename should be filed under. A path containing " b/"
            // is rare enough to accept the ambiguity.
            const int bIndex = line.indexOf(QLatin1String(" b/"));
            current.path = bIndex >= 0 ? line.mid(bIndex + 3) : line.mid(11);
            current.name = QFileInfo(current.path).fileName();
            current.dir = QFileInfo(current.path).path();
            if (current.dir == QLatin1String("."))
                current.dir.clear();
            continue;
        }
        if (!inFile)
            continue;
        // Header lines carry the file's status but say nothing a reader needs
        // inside the body, where the row's own header already names the file.
        if (line.startsWith(QLatin1String("new file"))) {
            current.tag = QStringLiteral("new");
            continue;
        }
        if (line.startsWith(QLatin1String("deleted file"))) {
            current.tag = QStringLiteral("deleted");
            continue;
        }
        if (line.startsWith(QLatin1String("rename "))) {
            current.tag = QStringLiteral("renamed");
            continue;
        }
        // git's own marker for a file it did not diff as text. Kept as a flag
        // rather than as body text: a row that can show the image has no use
        // for the sentence, and one that cannot puts it back below.
        if (line.startsWith(QLatin1String("Binary files "))
            || line.startsWith(QLatin1String("GIT binary patch"))) {
            current.binary = true;
            body << line;
            ++kept;
            continue;
        }
        if (line.startsWith(QLatin1String("index ")) || line.startsWith(QLatin1String("--- "))
            || line.startsWith(QLatin1String("+++ ")) || line.startsWith(QLatin1String("old mode"))
            || line.startsWith(QLatin1String("new mode"))
            || line.startsWith(QLatin1String("similarity "))
            || line.startsWith(QLatin1String("copy ")))
            continue;

        if (line.startsWith(QLatin1Char('+')))
            ++current.added;
        else if (line.startsWith(QLatin1Char('-')))
            ++current.removed;

        if (maxLines > 0 && kept >= maxLines) {
            if (!truncated) {
                truncated = true;
                body << QStringLiteral("... truncated at %1 lines ([review] max_lines)")
                            .arg(maxLines);
            }
            continue;
        }
        body << line;
        ++kept;
    }
    flush();
    return files;
}
