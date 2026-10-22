#include "pointstogrid.h"
#include <algorithm>
#include <stdexcept>

std::vector<float> PointsToGrid::kmeans1D(const std::vector<float>& vals, int K)
{
    cv::Mat data((int)vals.size(), 1, CV_32F);
    for (int i = 0; i < (int)vals.size(); ++i) data.at<float>(i, 0) = vals[i];

    cv::Mat labels, centers;
    cv::kmeans(data, K, labels,
               cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 100, 1e-5),
               5, cv::KMEANS_PP_CENTERS, centers);

    std::vector<float> c(K);
    for (int k = 0; k < K; ++k) c[k] = centers.at<float>(k, 0);
    std::sort(c.begin(), c.end()); // defines row/col order
    return c;
}

int PointsToGrid::nearestCenterIdx(const std::vector<float>& centers, float x)
{
    int best = 0;
    float bestd = std::abs(x - centers[0]);
    for (int i = 1; i < (int)centers.size(); ++i) {
        float d = std::abs(x - centers[i]);
        if (d < bestd) { bestd = d; best = i; }
    }
    return best;
}

LabeledGrid PointsToGrid::labelCircleGrid(const std::vector<cv::Point2f>& centers,
                                          int cols, int rows)
{
    //PCA to get grid axes
    cv::Mat X((int)centers.size(), 2, CV_32F);
    for (int i = 0; i < (int)centers.size(); ++i) {
        X.at<float>(i, 0) = centers[i].x;
        X.at<float>(i, 1) = centers[i].y;
    }
    cv::PCA pca(X, cv::Mat(), cv::PCA::DATA_AS_ROW);

    cv::Vec2f axisU(pca.eigenvectors.at<float>(0, 0), pca.eigenvectors.at<float>(0, 1));
    cv::Vec2f axisV(pca.eigenvectors.at<float>(1, 0), pca.eigenvectors.at<float>(1, 1));

    //Project points to 1D coordinates along axes
    std::vector<float> u(centers.size()), v(centers.size());
    for (int i = 0; i < (int)centers.size(); ++i) {
        cv::Vec2f p(centers[i].x, centers[i].y);
        u[i] = p.dot(axisU);
        v[i] = p.dot(axisV);
    }

    //Cluster into row/col bands
    auto rowCenters = kmeans1D(v, rows);
    auto colCenters = kmeans1D(u, cols);

    //Assign each point a (row,col)
    std::vector<std::vector<cv::Point2f>> grid(rows, std::vector<cv::Point2f>(cols));
    std::vector<std::vector<bool>> occupied(rows, std::vector<bool>(cols, false));
    std::vector<cv::Point2i> labels(centers.size());

    for (int i = 0; i < (int)centers.size(); ++i) {
        int r = nearestCenterIdx(rowCenters, v[i]);
        int c = nearestCenterIdx(colCenters, u[i]);
        labels[i] = cv::Point2i(c, r);

        if (occupied[r][c]) {
            throw std::runtime_error("Grid labeling collision (two points in same row/col cell).");
        }
        occupied[r][c] = true;
        grid[r][c] = centers[i];
    }

    // enforce "top-to-bottom" row order based on average y of first row vs last row.
    float y0 = 0.f, yN = 0.f;
    for (int c = 0; c < cols; ++c) { y0 += grid[0][c].y; yN += grid[rows - 1][c].y; }
    if (y0 > yN) { // rows are upside down in image coordinates -> flip
        std::reverse(grid.begin(), grid.end());
    }

    //enforce "left-to-right" col order similarly
    float x0 = 0.f, xN = 0.f;
    for (int r = 0; r < rows; ++r) { x0 += grid[r][0].x; xN += grid[r][cols - 1].x; }
    if (x0 > xN) { // cols are reversed -> flip each row
        for (auto& row : grid) std::reverse(row.begin(), row.end());
    }

    return { grid, labels };
}


