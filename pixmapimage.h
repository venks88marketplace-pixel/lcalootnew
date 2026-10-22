#pragma once
#include <QQuickPaintedItem>
#include <QPainter>
#include <QObject>
#include <QVariant>
#include <QImage>
#include <QPixmap>
#include <QtQml/qqmlregistration.h>

class PixmapImage : public QQuickPaintedItem
{
    Q_OBJECT

    Q_PROPERTY(int sourceImageWidth READ sourceImageWidth NOTIFY sourceImageSizeChanged)
    Q_PROPERTY(int sourceImageHeight READ sourceImageHeight NOTIFY sourceImageSizeChanged)

public:
    explicit PixmapImage(QQuickItem *parent = nullptr);

    // Keep old API
    Q_INVOKABLE void setImage(const QPixmap& pixmap);

    // Robust QML entrypoint (QVariant from signal handlers)
    Q_INVOKABLE void setImageVariant(const QVariant& value);

    int sourceImageWidth() const { return m_pixmap.width(); }
    int sourceImageHeight() const { return m_pixmap.height(); }

signals:
    void sourceImageSizeChanged();

protected:
    void paint(QPainter *painter) override;

private:
    QPixmap m_pixmap;
};