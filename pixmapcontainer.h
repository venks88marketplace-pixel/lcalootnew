#ifndef PIXMAPCONTAINER_H
#define PIXMAPCONTAINER_H

#include <QObject>
#include <QPixmap>
#include <QtQml>


class PixmapContainer : public QObject
{
    Q_OBJECT
    //QML_ELEMENT
public:
    //explicit PixmapContainer(QObject *parent = nullptr) : QObject(parent){}
    explicit PixmapContainer(QObject *parent = nullptr);
    QPixmap m_pixmap;

signals:
};

#endif // PIXMAPCONTAINER_H
