#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

using  namespace cv;

struct LabeledGrid {
    std::vector<std::vector<cv::Point2f>> grid; // [rows][cols]
    std::vector<cv::Point2i> labels;            // per input point: (col,row)
};

class PointsToGrid
{
public:
    static std::vector<float> kmeans1D(const std::vector<float>& vals, int K);
    static int nearestCenterIdx(const std::vector<float>& centers, float x);
    static LabeledGrid labelCircleGrid(const std::vector<cv::Point2f>& centers,	int cols, int rows);
};


