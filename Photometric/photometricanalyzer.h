#pragma once
#include <QObject>
#include <string>
#include "ImageSide.h"
#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>

class PhotometricAnalyzer : public QObject
{
    Q_OBJECT
public:
    explicit PhotometricAnalyzer(QObject *parent = nullptr) {}

    Q_INVOKABLE cv::Mat evaluateIntensitySimilarity( const cv::Mat& image,  ImageSide side);

    void calDataRootPhotometric( std::string& dataRoot ){
        m_calDataRootPhotometric = dataRoot;
    }

    std::string& calDataRootPhotometric() { return m_calDataRootPhotometric; }
    std::string currentDateString();

    QVector<QPointF> m_polylineLT;
    QVector<QPointF> m_polylineLB;
    QVector<QPointF> m_polylineRT;
    QVector<QPointF> m_polylineRB;
    std::pair<unsigned int, unsigned int> m_topBotPeaksLeft;
    std::pair<unsigned int, unsigned int> m_topBotPeaksRight;

private:
    enum ImageParts{
        Top,
        Bottom
    };
    cv::Mat computeHistogram(cv::Mat& img,
                             std::string label,
                             int& maxIndex);
    cv::Mat smoothHistogram(cv::Mat& img);

    std::tuple<cv::Mat, int> normalizeHistogram(
                            cv::Mat& histsm,
                            std::string label,
                            const int targetWidth,
                            const int targetHeight);
    cv::Mat QImageToCvMatP(const QImage &image);
    ImageSide m_imageSide;
    std::string m_calDataRootPhotometric;

};

