#pragma once
#include <QObject>
#include <opencv2/core.hpp>
#include "utils.h"

class DMDChecker : public QObject
{
    Q_OBJECT
public:
    explicit DMDChecker(cv::Size patternSize, QObject *parent = nullptr);
    std::vector<cv::Point3f> findChckerboardAndCreateCheckPoints(const cv::Mat& calLeft,
                                                                 const cv::Mat& calRight);

    QImage createImage(std::vector< cv::Point2f> points, int radius);
    std::vector<cv::Point3f> detectUVSpots(const cv::Mat& calLeft, const cv::Mat& calRight);
    std::vector< cv::Point2f> convertPointsToDA(std::vector< cv::Point3f> points);
    std::vector< cv::Point3f> convertPointsToIA(const calibration::Parameter& param,
                                               const std::vector < cv::Point2f >&left,
                                               const std::vector < cv::Point2f >&right);
signals:

private:
    cv::Size m_patternSize;
    cv::Mat m_dK, m_dD, m_dR, m_dT;
    cv::Mat m_IA;
    double m_dx, m_dy, m_dz;
    calibration::Parameter m_cameraParams;
};

