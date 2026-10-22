#pragma once

#include "opencv2/core.hpp"

class BlobFinder
{
public:
    BlobFinder()
    {}

    static std::vector<cv::Point2f> findBlobs(const cv::Mat& image);

};


