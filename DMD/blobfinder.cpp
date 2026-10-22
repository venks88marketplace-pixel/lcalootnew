#include <QDebug>
#include "blobfinder.h"
#include <vector>
#include "PointsToGrid.h"

using namespace cv;

std::vector<cv::Point2f> BlobFinder::findBlobs(const cv::Mat& image)
{
    cv::Mat grey = image.clone();

    SimpleBlobDetector::Params params;

    params.thresholdStep = 10;
    params.minThreshold = 80;
    params.maxThreshold = 254;

    params.filterByArea = true;
    params.minArea = 15;
    params.maxArea = 200;

    params.filterByCircularity = true;
    params.minCircularity = 0.6;

    params.filterByConvexity = false;
    params.filterByInertia = false;
    params.filterByColor = false;

    cv::Mat gImage;

    std::vector<KeyPoint> keypoints;

    Ptr<SimpleBlobDetector> detector = SimpleBlobDetector::create(params);

    detector->detect(grey, CV_OUT keypoints);
    qDebug() << "keypoints found: " <<  keypoints.size();

    std::vector<cv::Point2f> blobCenters;
    //KeyPoints to Point2f
    blobCenters.reserve(keypoints.size());
    blobCenters.resize(keypoints.size());
    std::transform(keypoints.begin(), keypoints.end(), blobCenters.begin(), [](const cv::KeyPoint& kp) { return kp.pt; });
    return blobCenters;
}

