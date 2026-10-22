#pragma once

#include <QObject>
#include <QPair>
#include <utility>
#include <opencv2/core.hpp>
#include "utils.h"

using namespace calibration;

class ImageProcessor : public QObject
{
    Q_OBJECT
public:
    explicit ImageProcessor(QObject *parent = nullptr);

    Q_INVOKABLE bool computeOffsets( const QPixmap& leftImage, const QPixmap& rightImage);

public slots:

    cv::Rect& searchRoiLeft() { return m_searchRoiLeft; }
    cv::Rect& searchRoiRight() { return m_searchRoiLeft; }

    std::pair<float,float>& leftImageUVCenter() { return m_leftImageUVCenter; }
    std::pair<float,float>& rightImageUVCenter() { return m_rightImageUVCenter; }

private:
    bool findImageOffset(calibration::EnumCameraSide cam, const cv::Mat& img);
    cv::Mat QImageToCvMat(const QImage& image);
    cv::Rect findSearchRoi(calibration::EnumCameraSide cam, const cv::Mat& img);
    void findUVCenter(calibration::EnumCameraSide cam, const cv::Mat& img, const cv::Rect searchRoi);


    std::pair<float,float> m_leftImageUVCenter;
    std::pair<float,float> m_rightImageUVCenter;

    std::pair<float,float> m_leftImageCenter;
    std::pair<float,float> m_rightImageCenter;

    std::pair<float,float> m_averageOffset{std::pair(0,0)};
    //to be calculate in image of real target
    cv::Rect m_searchRoiLeft{790, 894, 144, 120}; //in the only test image, without painted edge
    cv::Rect m_searchRoiRight{790, 894, 144, 120}; //in the only test image, without painted edge

signals:
    void imageUpdated();

};

