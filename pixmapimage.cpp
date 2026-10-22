#include "pixmapimage.h"
#include <QMetaType>
#include <QDebug>

PixmapImage::PixmapImage(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    // Better behavior for frequent updates/video
    setRenderTarget(QQuickPaintedItem::FramebufferObject);

    QImage image(2016, 1452, QImage::Format_Grayscale8);
    //QImage image(1600, 1152, QImage::Format_Grayscale8); //for mock testing
    image.fill(0);
    m_pixmap = QPixmap::fromImage(image);
}

void PixmapImage::paint(QPainter *painter)
{
    if (!m_pixmap.isNull()) {
        painter->drawPixmap(boundingRect().toRect(), m_pixmap);
    }
}

void PixmapImage::setImage(const QPixmap& img)
{
    if (img.isNull()) {
        qWarning() << "[PixmapImage] setImage got null pixmap";
        return;
    }

    const bool sizeChanged =
        img.width() != m_pixmap.width() ||
        img.height() != m_pixmap.height();

    m_pixmap = img;

    if (sizeChanged)
        emit sourceImageSizeChanged();

    update();
}

void PixmapImage::setImageVariant(const QVariant& value)
{
    if (!value.isValid()) {
        qWarning() << "[PixmapImage] setImageVariant invalid QVariant";
        return;
    }

    // Direct QPixmap path
    if (value.canConvert<QPixmap>()) {
        QPixmap px = value.value<QPixmap>();
        setImage(px);
        return;
    }

    // QImage fallback path
    if (value.canConvert<QImage>()) {
        QImage im = value.value<QImage>();
        setImage(QPixmap::fromImage(im));
        return;
    }

    qWarning() << "[PixmapImage] Unsupported QVariant type for image:" << value.typeName();
}