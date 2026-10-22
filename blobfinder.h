#pragma once

//#include <filesystem>
//#include <iostream>
#include "opencv2/core.hpp"
//#include "opencv2/imgcodecs.hpp"
//#include "opencv2/highgui.hpp"
//#include "opencv2/imgproc.hpp"
//#include "opencv2/opencv.hpp"
//#include <numeric>
//#include <string>
//#include <vector>

class BlobFinder
{
public:
    BlobFinder()
    {}

    static std::vector<cv::Point2f> findBlobs(const cv::Mat& image, cv::Size blobGridDimension);

};


