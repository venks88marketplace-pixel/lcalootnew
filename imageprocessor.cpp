#include "headers/imageprocessor.h"
#include <QQmlEngine>
#include <QThread>
#include <QPixmap>
#include "opencv2/core.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp>
#include <QDebug>
#include <vector>
#include <math.h>

using namespace cv;
using namespace calibration;

//TBD: move to a utility file later
void printMatType(const cv::Mat& mat) {
    int type = mat.type();
    int depth = type & CV_MAT_DEPTH_MASK; // Extract depth
    int channels = 1 + (type >> CV_CN_SHIFT); // Extract number of channels

    std::string depthStr;
    switch (depth) {
    case CV_8U:  depthStr = "8U"; break;
    case CV_8S:  depthStr = "8S"; break;
    case CV_16U: depthStr = "16U"; break;
    case CV_16S: depthStr = "16S"; break;
    case CV_32S: depthStr = "32S"; break;
    case CV_32F: depthStr = "32F"; break;
    case CV_64F: depthStr = "64F"; break;
    default:     depthStr = "Unknown"; break;
    }
    qDebug() << "Type: CV_" << depthStr << "C" << channels;
}

double calculateDistance(const Point& p1, const Point& p2) {
    return sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
}

ImageProcessor::ImageProcessor(QObject *parent)
    : QObject{parent}
{}

bool ImageProcessor::computeOffsets( const QPixmap& leftImage, const QPixmap& rightImage )
{
    //currently debuging saved mocked images
    std::pair<float,float> averageOffset{std::pair<float,float>(0, 0)};
    try {
        qDebug() << "QML call to computeOffsets!";
        Mat imgL = QImageToCvMat(leftImage.toImage());

        //currently debuging via saved images
        if ( imwrite("$(HOME)/images/leftIm.jpg", imgL)) {qDebug() << "imgL saved";}
        if (!findImageOffset(EnumCameraSide::LeftCam, imgL))
            return false;

        Mat imgR = QImageToCvMat(rightImage.toImage());
        //TBD: change to imgR after appropriate image
        if (!findImageOffset(EnumCameraSide::RightCam, imgL))
            return false;

        //TBD: compute average for left and right
    }catch (const std::exception& e){
        qDebug() << "computeOffsets exeption: " << e.what();
        return false;
    }
    return true;
}

// compute center of only UV spot in ROI image
bool ImageProcessor::findImageOffset(EnumCameraSide cam, const cv::Mat& img)
{
    try {
        // find ROI - no black checkers
        cv::Rect searchRoi = findSearchRoi(cam, img);
        if (searchRoi.empty()) {
            qDebug() << calibration::CamToString(cam )<< ": search ROI computation failed";
            return false;
        }
        findUVCenter(cam, img, searchRoi);

    }catch(const cv::Exception& e) {
        qDebug() << "Eerror finding offset: " << e.what();
        return false;
    }
    return true;
}

cv::Mat ImageProcessor::QImageToCvMat(const QImage &image)
{
    QImage converted = image.convertToFormat(QImage::Format_Grayscale8);
    return cv::Mat(converted.height(), converted.width(), CV_8UC1,
                   const_cast<uchar*>(converted.bits()), converted.bytesPerLine()).clone();
}

cv::Rect ImageProcessor::findSearchRoi(EnumCameraSide cam, const Mat& img)
{
    int minX, maxX, minY, maxY;
    try {
        //find area without black checkers within the chckerboard target
        Mat out;
        threshold(img, out, 10,255,cv::THRESH_BINARY); //50,255
        if ( imwrite("/home/vlad/images/binary.jpg", out)) {qDebug() << "thresholed 10, 255 image saved";}

        medianBlur(out,out,5);
        if ( imwrite("/home/vlad/images/binary-blurred.jpg", out)) {qDebug() << "thresholed blurred image saved";}

        int lowThreshold1 = 80;
        const int ratio1 = 2;
        const int kernel_size1 = 3;

        Mat edgeImage;
        Canny( out, edgeImage, lowThreshold1, lowThreshold1*ratio1, kernel_size1);
        if ( imwrite("/home/vlad/images/cannyEdges.jpg", edgeImage)) {qDebug() << "cannyImage saved";}

        Mat kernel = getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
        Mat dilatedImage;
        printMatType(edgeImage);
        if ( !edgeImage.empty()) {
            cv::dilate(edgeImage, dilatedImage, kernel);
            if (dilatedImage.empty()) {
                //TBD: log failure
                qDebug() << "dilatedImage EMPTY !!!!!!!!!!!!!!!!!!!!!!";
                return Rect();
            }
            if ( imwrite("/home/vlad/images/cannyEdgesDilated.jpg", dilatedImage)) {qDebug() << "cannyImage2dilated saved";}
        }
        std::vector<cv::Vec4f> lines;
        std::vector<float> coordX;
        std::vector<float> coordY;

        //TBD: try to make finding extreme points smarter
        cv::HoughLinesP(dilatedImage, lines, 1, CV_PI/2 ,100, 200, 0 ); //only for hor and vert lines
        for (int i=0; i < (int)lines.size(); i++) {
            Point p1(lines[i][0], lines[i][1]);
            coordX.push_back(lines[i][0]);
            coordY.push_back(lines[i][1]);
            Point p2(lines[i][2], lines[i][3]);
            coordX.push_back(lines[i][2]);
            coordY.push_back(lines[i][3]);

            line(out, Point(lines[i][0], lines[i][1]), Point(lines[i][2], lines[i][3]), cv::Scalar(0,0,0), 4);
            qDebug() << "line: " << i << " " << lines[i][0] << "," << lines[i][1] << "  " << lines[i][2] << "," << lines[i][3]
                     << " length: " << std::abs(calculateDistance(p1, p2));
        }
        minX = *min_element(coordX.begin(), coordX.end());
        maxX = *max_element(coordX.begin(), coordX.end());
        minY = *min_element(coordY.begin(), coordY.end());
        maxY = *max_element(coordY.begin(), coordY.end());

        qDebug() << "min-max coordinates or rect: " << "xmin: " << minX << " " << "maxX: " << maxX << " " << "minY: " << minY << " " << "maxY: " << maxY;
    }catch(std::exception& e) {
        qDebug() << "failed to find ROI, " << e.what();
        return Rect();
    }
    return cv::Rect(minX + 20, minY + 20, maxX - minX - 40, maxY - minY - 40); //reduce dimensions to avoid border pixel in edge image,
}

void ImageProcessor::findUVCenter(EnumCameraSide cam, const Mat& img, const Rect searchRoi)
{
    cam == EnumCameraSide::LeftCam ? m_searchRoiLeft = searchRoi : m_searchRoiRight = searchRoi;

    //find UV spot
    //TODO: read from config?
    int lowThreshold = 30;
    const int ratio = 4;
    const int kernel_size = 3;
    Mat blurImage;

    GaussianBlur(img(searchRoi), blurImage, Size(3, 3), 2.0);
    if ( imwrite("$(HOME)/images/gausBlurred-for-spot.jpg", blurImage)) {qDebug() << "blurImage saved";}

    Canny( blurImage, blurImage, lowThreshold, lowThreshold*ratio, kernel_size);
    if ( imwrite("$(HOME)/images/cannyEdges-for-spot.jpg", blurImage)) {qDebug() << "cannyImage saved";}

    qDebug() << "Canny image cols: " << blurImage.cols <<  ", rows: " << blurImage.rows;

    std::vector<Point> edgePoints;
    for (int row = 0; row < blurImage.rows; ++row) {
        for (int col = 0; col < blurImage.cols; ++col) {
            if (blurImage.at<uchar>(row, col) > 150) {
                edgePoints.push_back(Point(col, row));
            }
        }
    }
    qDebug() << "points in edge image: " << edgePoints.size();

    Rect bb = boundingRect(edgePoints);
    qDebug() << "bounding box: " << bb.x << " " << bb.y << " " << bb.width << " " << bb.height;
    std::pair UVCenter = std::pair(searchRoi.x + bb.x + bb.width/2, searchRoi.y + bb.y + bb.height/2);
    cam == EnumCameraSide::LeftCam ? m_leftImageUVCenter = UVCenter
                                   : m_rightImageUVCenter = UVCenter;
    std::string camera = cam == EnumCameraSide::LeftCam ? "left" : "right" ;
    qDebug() << "UV Center found: " << "X: " << UVCenter.first << ", " << "Y: " << " " << UVCenter.second << "camera: " << camera ;
}


