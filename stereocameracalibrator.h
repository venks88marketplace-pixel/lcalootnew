#pragma once
#include <QObject>
#include <string>
#include <map>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <services/StepperKst101ZfsService.h>
#include "StereoCalibrationResults.h"
#include "utils.h"

class StereoCameraCalibrator : public QObject
{
    Q_OBJECT

public:
    explicit StereoCameraCalibrator(cv::Size patternsize, QObject *parent = nullptr);

    struct calibPlane {
        std::string label;
        std::string fname;
        float zSeq;
        cv::Mat left, right;
    };



    struct Stats {
        double mean;
        double stddev;
    };

    struct Corner
    {
        int x;
        int y;
    };

    struct CameraCorners
    {
        Corner topLeft;
        Corner bottomLeft;
        Corner topRight;
        Corner bottomRight;
    };

    cv::Mat dataCollection(const cv::Mat& calLeft,
               const cv::Mat& calRight,
               std::string filename,
               std::string folder,
               float z);
    void train3D(const double hx,
                 const double hy,
                 const std::string& folder);

    cv::Mat verification(int zPlane,
                         const cv::Mat& calLeft,
                         const cv::Mat& calRight,
                         std::string folder);

    std::string currentDateString();

    std::string& savedImageFolder() { return m_savedImageFolder; }
    std::map<float, std::string>& assistNames() { return  m_assistNames; }
    calibration::Parameter getParams() { return m_param;}

    StereoCalibrationResults getResults() const {return m_results; }

private slots:

private:
    void generatePairs(std::vector<std::pair<int,int>>& Hpairs,
                       std::vector<std::pair<int,int>>& Vpairs,
                       cv::Size& patternsize);

    std::vector< cv::Point2f > calibrationPattern2D(const double hx,
                        const double hy,
                        cv::Size chessboardSize);

    void checkerboardCorners3D(std::vector<calibPlane>& planes,cv::Size& patternSize,
                        float hx,
                        float hy,
                        std::vector<std::vector<cv::Point2f>>& leftPlanes,
                        std::vector<std::vector<cv::Point2f>>& rightPlanes,
                        std::vector<std::vector< cv::Point3f >>& obj3D);

    double computeReprojectionErrors( const std::vector<std::vector<cv::Point3f> >& objectPoints,
                         const std::vector<std::vector<cv::Point2f> >& imagePoints,
                         const std::vector<cv::Mat>& rvecs,
                         const std::vector<cv::Mat>& tvecs,
                         const cv::Mat& cameraMatrix ,
                         const cv::Mat& distCoeffs,
                         std::vector<float>& perViewErrors);

    std::vector< cv::Point3f> estimate3DPts(const calibration::Parameter& param,
                        const std::vector<cv::Point2f>&left,
                        const std::vector<cv::Point2f>&right);

    //this is not accurate
    // void computeMeanStd(std::vector<double>& v, double& mean, double& stdev) {
    //     double sum = std::accumulate(v.begin(), v.end(), 0.0);
    //     mean = sum / v.size();
    //     std::vector<double> diff(v.size());
    //     std::transform(v.begin(), v.end(), diff.begin(), [mean](double x) { return x - mean; });
    //     double sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
    //     stdev = std::sqrt(sq_sum / v.size());
    // }

    // Welford's algorithm for mean and standard deviation - use this one
    Stats computeStats(const std::vector<double>& data);

    double calibrateSingleCamera(const std::vector<std::vector<cv::Point2f>>& imagePoints,
                         const std::vector<std::vector<cv::Point3f>>& objectPoints,
                         const cv::Size imageSize,
                         cv::Mat& cameraMatrix,
                         cv::Mat& distCoeffs,
                         std::vector<float>& reprojErrs);



    double computeReprojectionErrors3D( const std::vector<cv::Point3f> & objectPoints,
                        const std::vector<cv::Point2f>& imagePoints,
                        const cv::Mat& rvecs,
                        const cv::Mat& tvecs,
                        const cv::Mat& cameraMatrix,
                        const cv::Mat& distCoeffs,
                        float& rmsError);

    double calibrateSingleCamera3D(const std::vector<std::vector<cv::Point2f >>& imagePoints,
                        const std::vector<std::vector<cv::Point3f>>& objectPoints,
                        const cv::Size imageSize,
                        cv::Mat& cameraMatrix,
                        cv::Mat& distCoeffs,
                        cv::Mat& rvec,
                        cv::Mat& tvec,
                        float& rmsError);


    double StereoAnalysis3D( calibration::Parameter& param,
                            const std::vector<cv::Point2f>& leftPlanes,
                            const std::vector<cv::Point2f>& rightPlanes,
                            const std::vector<cv::Point3f>& objPoints,
                            std::vector<cv::Point3f>& estOutPts);

    void loadDataFiles(std::vector<calibPlane>& calibData,
                       std::string fname,
                       std::string base_folder,
                       cv::Size& chessboardSize);

    void findBoundingCorners(
            std::vector<cv::Point2f>& corners,
            std::vector<cv::Point>& crop,
            cv::Point2f& cm);

    void saveTargetCorners(const std::string& fileName,
                           CameraCorners& left,
                           CameraCorners& right);
private:
    std::map<float, std::string> m_assistNames;
    std::string m_savedImageFolder="c:/calibration/apps/stereoCalibrationAnalysis";
    calibration::Parameter m_param;
    cv::Size m_patternSize;
    StereoCalibrationResults m_results;
};

