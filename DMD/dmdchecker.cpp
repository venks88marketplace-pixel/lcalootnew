#include "dmdchecker.h"
#include "opencv2/imgproc.hpp"
#include "opencv2/calib3d.hpp"
#include <opencv2/highgui.hpp>
#include "headers/calibrationStorage.h"
#include "utils.h"
#include <QImage>
#include "blobfinder.h"

DMDChecker::DMDChecker(cv::Size patternSize, QObject *parent)
    : QObject{parent},
    m_patternSize{patternSize}
{
    calibrationStorage calStore;
    calStore.getInstrumentAxis(m_IA, m_dx, m_dy, m_dz);
    calStore.getDMDParameters(m_dK, m_dD, m_dR, m_dT);
    calStore.getNfovCameraMatrices(m_cameraParams.D1, m_cameraParams.K1, m_cameraParams.D2, m_cameraParams.K2);
    calStore.getStereoMatrix(m_cameraParams.F, m_cameraParams.E, m_cameraParams.R, m_cameraParams.T,
                             m_cameraParams.R1, m_cameraParams.R2, m_cameraParams.P1, m_cameraParams.P2, m_cameraParams.Q);
    m_cameraParams.useRectification = true;

}


static std::vector<cv::Point2f> getCheckpoints(std::vector<cv::Point2f> input, cv::Size patternSize){
    std::vector<cv::Point2f> ret;
    if(input.size() == patternSize.area())
    {
        int last = input.size()-1;
        // reverse vector as sometimes checkerboard detection algorithm reverses
        if(input[0].x < input[last].x) {
            std::reverse(input.begin(), input.end());
        }

        // checkerboard points numbered bottom up right to left
        // 80 <---- 70
        // ...........
        // 10 <----->0

        // test pattern
        //    4 5
        //  3  1  2

        int centery = patternSize.height / 2 - 1;
        int centerx = patternSize.width / 2 - 1;

        auto c1 = input[centery * patternSize.width + centerx];
        auto c2 = input[(centery + 1) * patternSize.width + centerx + 1];
        auto c = (c2 + c1) * 0.5f;
        ret.push_back(c);

        auto p11 = input[centery * patternSize.width + centerx - 2];
        auto p12 = input[(centery + 1) * patternSize.width + centerx - 1];
        auto p1 = (p11 + p12) * 0.5f;
        ret.push_back(p1);

        auto p21 = input[centery * patternSize.width + centerx + 2];
        auto p22 = input[(centery + 1) * patternSize.width + centerx + 3];
        auto p2 = (p21 + p22) * 0.5f;
        ret.push_back(p2);

        auto p31 = input[(centery + 2) * patternSize.width + centerx + 1];
        auto p32 = input[(centery + 1) * patternSize.width + centerx + 2];
        auto p3 = (p31 + p32) * 0.5f;
        ret.push_back(p3);

        auto p41 = input[(centery + 2) * patternSize.width + centerx ];
        auto p42 = input[(centery + 1) * patternSize.width + centerx - 1];
        auto p4 = (p41 + p42) * 0.5f;
        ret.push_back(p4);

        //int index = 0;
        //for (auto& p: ret) {
        //    qDebug() << "proj point " << index++ << " [" << p.x << "," << p.y << "] camera";
        //}
    }
    return ret;
}


std::vector<cv::Point3f> DMDChecker::findChckerboardAndCreateCheckPoints( const cv::Mat& calLeft, const cv::Mat& calRight) {
    std::vector<cv::Point2f> left_corners;
    std::vector<cv::Point2f> right_corners;
    cv::Mat combo = cv::Mat{};
    std::vector<cv::Point3f> pts;
    bool found1 = cv::findChessboardCornersSB(calLeft, m_patternSize, left_corners, 0);
    if (found1) {
        cv::cornerSubPix(calLeft, left_corners, cv::Size(11, 11),
                         cv::Size(-1, -1),
                         cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.1));

        //cv::drawChessboardCorners(calLeft, m_patternSize, left_corners, found1);
        //cv::circle(calLeft, left_corners[0], 10, cv::Scalar(0,0,0), -1 );
        //cv::imshow("Chessboard Corners", calLeft);
        //cv::waitKey(0);

    }
    bool found2 = cv::findChessboardCornersSB(calRight, m_patternSize, right_corners, 0);
    if (found2) {
        cv::cornerSubPix(calRight, right_corners, cv::Size(11, 11),
                         cv::Size(-1, -1),
                         cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.1));
        //cv::drawChessboardCorners(calLeft, m_patternSize, left_corners, found1);
        //cv::imshow("Chessboard Corners", calLeft);
    }

    if (found1 and found2) {

        auto lp = getCheckpoints(left_corners, m_patternSize);
        auto rp = getCheckpoints(right_corners, m_patternSize);

        //for(auto& p : lp){
        //    cv::circle(calLeft, p, 5, cv::Scalar(0,0,0), 2 );
        //}
        //cv::imshow("Chessboard Corners", calLeft);
        //cv::waitKey(0);

        pts = convertPointsToIA(m_cameraParams, lp, rp);

        int index = 0;
        for(auto& p : pts){
            qDebug() << "proj point " << index++ << " [" << p.x << "," << p.y << "," << p.z << "] IA";
        }

        auto points = convertPointsToDA(pts);
        //cv::Mat img = cv::Mat::zeros(768, 1024, CV_8UC1);
        //cv::Point2f scr(1024.0, 768.0);
        //for(auto& p: points){
        //    cv::ellipse(img, p, cv::Size(3, 3), 0, 0, 360, cv::Scalar(255), -1);
        //}
        //cv::imshow("DMD", img);
        //cv::waitKey(0);

    }
    else
    {
        qCritical() << "Checkerboard not found";
    }
    return pts;
}

std::vector< cv::Point3f> DMDChecker::convertPointsToIA(const calibration::Parameter& param,
                                                               const std::vector < cv::Point2f >&left,
                                                               const std::vector < cv::Point2f >&right) {
    cv::Mat estOutM(4,left.size(),CV_64F);
    std::vector<cv::Point2f> uLeft, uRight;
    std::vector< cv::Point3f > estOutPts;

    if (param.useRectification == true) {
        cv::undistortPoints(left, uLeft, param.K1, param.D1, param.R1, param.P1);
        cv::undistortPoints(right, uRight, param.K2, param.D2, param.R2, param.P2);
    }
    else {
        cv::undistortPoints(left, uLeft, param.K1, param.D1, cv::Mat{}, param.K1);
        cv::undistortPoints(right, uRight, param.K2, param.D2, cv::Mat{}, param.K2);
    }
    cv::triangulatePoints(param.P1, param.P2, uLeft, uRight, estOutM);

    for (uint i=0; i<left.size(); ++i) {
        auto d = estOutM.col(i).t();
        cv::Mat dd, ia;
        cv::convertPointsFromHomogeneous(d, dd);
        cv::transform(dd, ia, m_IA);
        estOutPts.push_back({ia.at<cv::Vec3f>(0)[0], ia.at<cv::Vec3f>(0)[1], ia.at<cv::Vec3f>(0)[2]});
        //P(i << ") l=" << left[i] << ", r=" << right[i] <<", 3d="<< dd  << " OR " << estOutPts.back())
    }
    return estOutPts;
}

std::vector< cv::Point2f> DMDChecker::convertPointsToDA(std::vector< cv::Point3f> points){
    std::vector<cv::Point2f> dmd_points;
    cv::projectPoints(points, m_dR, m_dT, m_dK, m_dD, dmd_points);
    return dmd_points;
}

 QImage DMDChecker::createImage(std::vector< cv::Point2f> points, int radius){

    cv::Mat img = cv::Mat::zeros(768, 1024, CV_8UC1);
     cv::Point2f scr(1024.0, 768.0);
    for(auto& p: points){
        cv::ellipse(img, p, cv::Size(radius, radius), 0, 0, 360, cv::Scalar(255), -1);
    }
    //cv::imshow("DMD image", img);
    //cv::waitKey(0);
    QImage qimg(img.data, img.cols, img.rows, static_cast<int>(img.step), QImage::Format_Grayscale8);
    return qimg.copy();
 }

 std::vector<cv::Point3f> DMDChecker::detectUVSpots(const cv::Mat& left, const cv::Mat& right){
    auto rightDots = BlobFinder::findBlobs(right);
    auto leftDots = BlobFinder::findBlobs(left);
    if(rightDots.size() == leftDots.size() && rightDots.size() > 0){
        return convertPointsToIA(m_cameraParams, leftDots, rightDots);
    }
    qDebug() << "Differnt number of UV spots detected by left and right cameras";
    return std::vector<cv::Point3f>();
 }
