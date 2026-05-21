#pragma once

#include <QWidget>
#include <QImage>
#include <QHash>
#include <QString>

/// Image rendering widget for displaying images inline in Neovim buffers.
/// Placeholder for the upcoming vim.ui.img API.
class ImageRenderer : public QWidget
{
    Q_OBJECT

public:
    explicit ImageRenderer(QWidget *parent = nullptr);

    /// Display an image at the given grid position (row, col) with size in cells
    void showImage(const QString &path, int row, int col, int widthCells, int heightCells,
                   int cellWidth, int cellHeight);

    /// Remove an image by its path key
    void removeImage(const QString &path);

    /// Clear all displayed images
    void clearAll();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct ImageEntry {
        QImage image;
        QRect rect; // pixel rect on widget
    };

    QHash<QString, ImageEntry> m_images;
};
