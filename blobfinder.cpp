#include "blobfinder.h"
#include <vector>
#include "opencv2/imgproc.hpp"
#include "opencv2/opencv.hpp"

#include "PointsToGrid.h"

using namespace cv;
using namespace std;

std::vector<cv::Point2f> BlobFinder::findBlobs(const cv::Mat& image, cv::Size blobGridDimension)
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
    //cv::GaussianBlur(grey, gImage, cv::Size(3,3), 2.0);

    // Storage for blobs
    std::vector<KeyPoint> keypoints;

    Ptr<SimpleBlobDetector> detector = SimpleBlobDetector::create(params);

    //detector->detect(gImage, CV_OUT keypoints);
    detector->detect(grey, CV_OUT keypoints);
    cout << "keypoints found: " << keypoints.size() << endl;

    std::vector<cv::Point2f> blobCenters;
    //KeyPoints to Point2f
    blobCenters.reserve(keypoints.size());
    blobCenters.resize(keypoints.size());
    std::transform(keypoints.begin(), keypoints.end(), blobCenters.begin(), [](const cv::KeyPoint& kp) { return kp.pt; });
    return blobCenters;
}

