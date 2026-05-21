#include "imagerenderer.h"
#include <QPainter>
#include <QImageReader>

ImageRenderer::ImageRenderer(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background: transparent;");
}

void ImageRenderer::showImage(const QString &path, int row, int col,
                              int widthCells, int heightCells,
                              int cellWidth, int cellHeight)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull())
        return;

    QRect rect(col * cellWidth, row * cellHeight,
               widthCells * cellWidth, heightCells * cellHeight);

    m_images[path] = {img.scaled(rect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation), rect};
    update();
}

void ImageRenderer::removeImage(const QString &path)
{
    m_images.remove(path);
    update();
}

void ImageRenderer::clearAll()
{
    m_images.clear();
    update();
}

void ImageRenderer::paintEvent(QPaintEvent * /*event*/)
{
    if (m_images.isEmpty())
        return;

    QPainter p(this);
    for (auto it = m_images.constBegin(); it != m_images.constEnd(); ++it) {
        const auto &entry = it.value();
        p.drawImage(entry.rect.topLeft(), entry.image);
    }
}
