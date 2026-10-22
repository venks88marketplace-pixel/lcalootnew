#pragma once

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QColor>
#include <QQmlContext>
#include <QTimer>
#include <interfaces/iimagepump2.h>

class ImagePumpMock2 : public IImagePump2
{
public:
    //explicit ImagePumpMock2(QObject *parent = nullptr);
    explicit ImagePumpMock2();

    explicit ImagePumpMock2(const ImagePumpMock2& rhs) = delete;
    ImagePumpMock2& operator= (const ImagePumpMock2& rhs) = delete;

    virtual void getImagePair() override;

    virtual  void startVideo() override;
    virtual  void stopVideo() override;

//public slots:
    void newImageReady();

//signals:
//    virtual void imageChanged(QPixmap* imageL, QPixmap* imageR) override;

private:
    QPixmap m_pixmapL;
    QPixmap m_pixmapR;
    QTimer m_timer;

    int m_imageIndex;

    QPixmap createImage(int w, int h, QColor &color);
};
