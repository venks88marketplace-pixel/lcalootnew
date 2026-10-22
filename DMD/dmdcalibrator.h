#pragma once
#include <QObject>
#include <string>
#include <map>
#include <iostream>
#include <filesystem>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include "headers/calibrationStorage.h"

namespace fs = std::filesystem;

class DMDCalibrator : public QObject
{
    Q_OBJECT

public:
    explicit DMDCalibrator(QObject *parent = nullptr);

    struct calibPlane {
        std::string label;
        std::string fname;
        float zSeq;
        cv::Mat left, right;
    };

    struct Parameters {
        int nx = 5;
        int ny = 3;
        int Cx = 1024/2;
        int Cy = 768/2;
        int exposure = 3000;
        double gainLeft = 6;
        double gainRight = 6;
        cv::Mat K1, K2;
        cv::Mat D1, D2;
        cv::Mat R, T, E, F;
        cv::Mat LP, RP;
        cv::Mat R1, R2, P1, P2, Q;
        cv::Mat KL, RL, tL4, rotXL, rotYL, rotZL, tL;
        cv::Mat KR, RR, tR4, rotXR, rotYR, rotZR, tR;
        cv::Mat distL, distR;
        cv::Mat Rst, Tst;
        cv::Mat Tr_IA;
        double wx, wy,wz;
        std::string svru_path, svrv_path, svr_scales;
        double r3d = 0.025; //residual threshold in mm
    };

    cv::Mat collectAndSaveUVDots(std::string& filename,
                                 cv::Mat& left,
                                 cv::Mat& right,
                                 Parameters& params);
    std::filesystem::path getExecutablePath();

    void trainDMDCM(const std::string& filename, Parameters& params);
    Parameters& calParams() {return m_params;}

    std::string currentDateString();

private:
    void loadCalibrationFileComponents();

    //UV dots in left and right images are objectPoints as far as calibrateCamera is concerned.
    //dots with unmached indices are excluded from both left and right images.
    //corrspondent DMD dot(s) for the unmached index(s) are also excluded
    cv::Mat computeUVDots3D(const cv::Mat& leftImage, const cv::Mat& rightImage,
                            Parameters& params,
                            std::vector<cv::Point2f>& UVLeft,
                            std::vector<cv::Point2f>& UVRight,
                            std::vector<cv::Point3f>& UV3D,
                            std::vector<cv::Point2f>& UVDmd);
    void getIndexedImagePoints( const cv::Mat& cameraImage,
                               const cv::Size patternDims,
                               std::vector<cv::Point2f>& dotCenters);
    template <typename T>
    void reverse_section(std::vector<T>& vec,
                                    size_t start,
                                    size_t count) {
        if (start >= vec.size()) {
            throw std::out_of_range("Start index is out of range.");
        }
        if (start + count > vec.size()) {
            throw std::out_of_range("Section exceeds vector size.");
        }
        std::reverse(vec.begin() + start, vec.begin() + start + count);
    };

    template <typename T>
    void reverse_grid_elements(std::vector<T>& vec, cv::Size dims){
        try {
            for (T val : vec) {
                std::cout << val << " ";
            }
            std::cout << "\n";
            size_t start_index = 0; // 0-based index
            size_t section_size = dims.width;
            for (int i = 0; i < dims.height; i++) {
                start_index = i * section_size;
                reverse_section(vec, start_index, section_size);
            }
            // Print the reversed vector
            for (T val : vec) {
                std::cout << val << " ";
            }
            std::cout << "\n";
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    };

    size_t numberOfSavedUVPlanes(const std::string& filename,
                                 std::vector<std::string>& files);

    std::map<float, std::string> m_assistNames;
    cv::Mat leftImage;
    cv::Mat rightImage;
    //std::string m_savedImageFolder="c:/calibration/apps/dmdCalibrationAnalysis/";
    Parameters m_params;
    calibrationStorage  m_calStore;
};
