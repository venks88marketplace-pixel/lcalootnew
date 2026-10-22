#include "photometricanalyzer.h"
#include <iostream>
#include <tuple>
#include <QPixmap>
#include <QColor>
#include <QDebug>
#include "opencv2/imgproc.hpp"
#include <opencv2/highgui.hpp>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>

using namespace cv;

cv::Mat PhotometricAnalyzer::computeHistogram(cv::Mat& img, std::string label, int& maxIndex) {
    cv::Mat histsm;
    histsm = smoothHistogram(img);
    std::tuple<cv::Mat, int> nhresult = normalizeHistogram(histsm, label, 2016, 1452);
    //std::tuple<cv::Mat, int> nhresult = normalizeHistogram(histsm, label, 1160, 1152); //for mock testing
    maxIndex = std::get<1>(nhresult);
    return std::get<0>(nhresult);
}

cv::Mat PhotometricAnalyzer::evaluateIntensitySimilarity( const cv::Mat& img, ImageSide side ){
    //std::tuple<cv::Mat, std::pair<int,int>> PhotometricAnalyzer::evaluateIntensitySimilarity( const QPixmap& image, ImageSide side ){
    m_imageSide = side;
    cv::Mat histogramImage;
    std::pair<unsigned int, unsigned int> topBotPeaks; //peaks
    try {
        //Mat img = QImageToCvMatP(image.toImage());
        cv::Rect topCrop = cv::Rect{0,0,img.cols, img.rows/2};
        cv::Rect botCrop = cv::Rect{0,img.rows/2,img.cols, img.rows/2};
        std::vector<std::string> labels {"Top", "Bottom"};

        cv::Mat cropT = img(topCrop);
        int peakIntensity{0};
        cv::Mat histTop = computeHistogram(cropT,labels[0], peakIntensity);
        std::cout << "top peak:  " << peakIntensity << std::endl;
        topBotPeaks.first = (unsigned int)peakIntensity;
        peakIntensity = 0;

        cv::Mat cropB = img(botCrop);
        cv::Mat histBot = computeHistogram(cropB,
                                       labels[1],
                                       peakIntensity);
        std::cout << "left-bot peak:  " << peakIntensity << std::endl;
        topBotPeaks.second = (unsigned int)peakIntensity;

        cv::vconcat(histTop, histBot, histogramImage);

        //store results
        std::string filename = side == ImageSide::Left
                                   ? "tb-left.png"
                                   : "tb-right.png";
        std::string pathTB =  m_calDataRootPhotometric + filename;
        imwrite(pathTB, histogramImage);

        if (side == ImageSide::Left ) {
            m_topBotPeaksLeft = topBotPeaks;
        }
        else {
            m_topBotPeaksRight = topBotPeaks;
        }
    }
    catch( std::exception& e ) {
        std::string imageside = side == ImageSide::Left
                                    ? "left "
                                    : "right ";
        qDebug() << "exception occurred: "
                  << imageside
                  << "image"
                 << e.what();
    }
    return histogramImage;
}

cv::Mat PhotometricAnalyzer::smoothHistogram(cv::Mat& img) {
    int histSize = 256;
    float range[] = { 0, 256 } ;
    const float* histRange = { range };
    int channels[] = {0};
    bool uniform = true; bool accumulate = false;
    cv::Mat hist, histsm;
    cv::calcHist( &img, 1, channels, cv::Mat(), hist, 1, &histSize, &histRange, uniform, accumulate );

    std::vector<float> histDebug;
    histDebug.assign(hist.ptr<float>(), hist.ptr<float>() + hist.total());

    hist.copyTo(histsm);
    for (int i = 2; i < histSize-3; ++i) {
        histsm.at<float>(i) = hist.at<float>(i-2) * 0.13 +
                              hist.at<float>(i-1) * 0.22 +
                              hist.at<float>(i) * 0.3 +
                              hist.at<float>(i+1) * 0.22 +
                              hist.at<float>(i+2) * 0.13;
    }

    return histsm;
}

std::tuple<cv::Mat, int> PhotometricAnalyzer::normalizeHistogram(
            cv::Mat& histsm,
            std::string label,
            const int targetWidth,
            const int targetHeight) {
    int hist_w = targetWidth;
    int hist_h = targetHeight/2 - 50;
    std::cout << "normalizeHistogram "
              << "targetWidth: "
              << hist_w
              << "targetHeight: "
              << hist_h << std::endl;

    int bin_w = cvRound( (double) hist_w/histsm.rows);
    cv::Mat histNorm;
    cv::Mat histImage = cv::Mat(
                hist_h + 50,
                hist_w,
                CV_8UC3,
                cv::Scalar( 20,20,20) );
    cv::normalize(histsm,
                  histNorm,
                  0,
                  hist_h,
                  cv::NORM_MINMAX,
                  -1,
                  cv::Mat() );
    std::vector<std::pair<int, float>> peaks;
    // //vertical grid
    int gridStrand = 50;
    for (int i = gridStrand; i < hist_w; i+=gridStrand) {
        cv::line(histImage,
                 cv::Point( i, 0),
                 cv::Point(i,hist_h),
                 cv::Scalar{75, 75, 75},
                 1,
                 8,
                 0);
    }
    //horizontal grid
    for (int i = gridStrand; i < hist_h; i+=gridStrand) {
        cv::line(histImage,
                 cv::Point( 0, i),
                 cv::Point(hist_w, i),
                 cv::Scalar{75, 75, 75},
                 1,
                 8,
                 0);
    }

    float dmax = 0;
    int maxIndex = -1;
    QVector<QPointF> polyline;
    for( int i = 1; i < histsm.rows; i++ ) {
        cv::Point p = cv::Point(bin_w * (i - 1),
                hist_h - cvRound(histNorm.at<float>(i - 1)));
        polyline.append(QPointF(p.x,p.y));
        cv::line(histImage,
                 p,
                 cv::Point(bin_w * (i),
                           hist_h - cvRound(histNorm.at<float>(i))),
                 cv::Scalar(234, 152, 150), 2, 8, 0);
        float d = histNorm.at<float>(i);
        if (d > dmax) {
            dmax = d;
            maxIndex = i;
        }
    }
    //polylines for overlay display
    if (m_imageSide == ImageSide::Left) {
        if (label.compare("Top") == 0)
            m_polylineLT = polyline;
        else {
            for (const auto& p : polyline) {
                m_polylineLB.append(
                    QPointF(p.x(),
                    p.y() + targetHeight/2));
            }
        }
    }
    else {
        if (label.compare("Top") == 0) {
             m_polylineRT = polyline;
        }else {
            for (const auto& p : polyline) {
                m_polylineRB.append(
                    QPointF(p.x(),
                    p.y() + targetHeight/2));
            }
        }
    }
    for (auto p : peaks) {
        cv::putText(histImage,
                    std::to_string(p.first),
                    cv::Point{bin_w*(p.first-1) ,
                    hist_h/2},
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(255,155,240));
        std::cout << "Peak at :"
                  << p.first << "(" << p.second << ")" << std::endl;
    }
    return std::make_tuple(histImage, maxIndex);
}

cv::Mat PhotometricAnalyzer::QImageToCvMatP(const QImage &image)
{
    QImage converted = image.convertToFormat(QImage::Format_Grayscale8);
    return cv::Mat(converted.height(), converted.width(), CV_8UC1,
                   const_cast<uchar*>(converted.bits()), converted.bytesPerLine()).clone();
}

std::string PhotometricAnalyzer::currentDateString() {
    auto p = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(p);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::stringstream ss;
    ss << std::put_time(&tm,"%Y_%b_%d_%H_%M_%S");
    //std::cout << ss.str() << std::endl;
    return ss.str();
}
