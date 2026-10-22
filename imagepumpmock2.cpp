#include "imagepumpmock2.h"
#include <QtDebug>
#include "opencv2/opencv.hpp"


ImagePumpMock2::ImagePumpMock2() {

}

void ImagePumpMock2::newImageReady()
{
    int width = 640;
    int height = 480;
    QColor colorL = QColor(200,0,0);
    QColor colorR = QColor(0,0,200);

    if ( m_imageIndex % 4 == 0 ) {
        colorL = QColor(0,200,200);
        colorR = QColor(0,100,100);
    } else if (m_imageIndex % 4 == 1) {
        colorL = QColor(0,0,200);
        colorR = QColor(0,0,100);
    }else if (m_imageIndex % 4 == 2) {
        colorL = QColor(200,200,0);
        colorR = QColor(100,100,0);

    }else if (m_imageIndex % 4 == 3) {
        colorL = QColor(0, 20, 200);
        colorR = QColor(0, 20, 100);
    }
    m_pixmapL = createImage(width, height, colorL);
    m_pixmapR = createImage(width, height, colorR);
    m_imageIndex++;

    qDebug("sending signal imageChanged");

    //emit imageChanged(&m_pixmapL, &m_pixmapR);
}

void ImagePumpMock2::getImagePair()
{
    //int width = 640;
    //int height = 480;
    //QColor colorL = QColor(200,0,100);
    //QColor colorR = QColor(0,100,100);

    cv::Mat imageL = cv::imread("../../assets/uvspottest1.png",cv::IMREAD_GRAYSCALE);
    QImage l = QImage(imageL.data, imageL.cols, imageL.rows, imageL.step, QImage::Format_Grayscale8).copy();
    cv::Mat imageR = cv::imread("../../assets/uvspottestR.png",cv::IMREAD_GRAYSCALE);
    QImage r = QImage(imageR.data, imageR.cols, imageR.rows, imageR.step, QImage::Format_Grayscale8).copy();

    m_pixmapL = QPixmap::fromImage(l);
    m_pixmapR = QPixmap::fromImage(r);
    //m_pixmapL = createImage(width, height, colorL);
    //m_pixmapR = createImage(width, height, colorR);
    //m_imageIndex++;

    qDebug("sending signal imageChanged");

    //emit imageChanged(&m_pixmapL, &m_pixmapR);
}

QPixmap ImagePumpMock2::createImage(int w, int h, QColor &color)
{
    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(color);
    return QPixmap::fromImage(image);
}
void ImagePumpMock2::startVideo() {
    if (!m_timer.isActive())
        m_timer.start();
}
void ImagePumpMock2::stopVideo() {
    if (m_timer.isActive())
        m_timer.stop();
}
