#pragma once

#include <opencv2/core/core.hpp>

namespace neo::calibration {

        struct photometricCalibrationData{
            std::string timestr;
            int exposure;              // camera exposure in microseconds
            double leftCameraGain;                // camera gain in dB
            double rightCameraGain;                // camera gain in dB

            photometricCalibrationData() {
                //default
                timestr = "default";
                exposure = 6000; //microsecond
                leftCameraGain = 8.0;
                rightCameraGain = 8.0;
            }
        };

        struct roiGeometry {
            int roiOffsetX;
            int roiOffsetY;
            roiGeometry() {
                roiOffsetX = (2448 - 2016)/2;
                roiOffsetY = (2048 - 1452)/2;
            }
        };

        struct spatialCalibration{
            cv::Mat intrinsics;         //  3x3 Matrix for Camera Intrinsics
            cv::Mat distortion;         //  1x5 std::vector holding Camera Distortion Model Coefficents
            cv::Mat projectionMatrix;   //  3x4 Projection Matrix Lab to Camera Frame

            //default
            spatialCalibration() {
                intrinsics = cv::Mat::eye(3,3,CV_64F);
                distortion = cv::Mat::zeros(1,5,CV_64F);
                projectionMatrix = cv::Mat::eye(3,4,CV_64F);
            }
        };

        struct NFOVCalibrationData {
            std::string timestr;
            spatialCalibration left, right;             //  Left Camera
            roiGeometry leftOffset, rightOffset;
            cv::Mat F;  //  3x3 Fundamental Matrix
            cv::Mat E;
            cv::Mat R, T;
            cv::Mat R1, R2;
            cv::Mat P1, P2, Q;
            cv::Mat toIA;

            NFOVCalibrationData(){
                timestr = "default";
                left = spatialCalibration();
                right = spatialCalibration();
                leftOffset = roiGeometry();
                rightOffset = roiGeometry();
                F = cv::Mat::eye(3,3,CV_64F);
                E = cv::Mat::eye(3,3,CV_64F);
                R = cv::Mat::eye(3,3,CV_64F);
                T = cv::Mat::eye(3,1,CV_64F);
                R1 = cv::Mat::eye(3,3,CV_64F);
                R2 = cv::Mat::eye(3,3,CV_64F);
                P1 = cv::Mat::eye(3,4,CV_64F);
                P2 = cv::Mat::eye(3,4,CV_64F);
                Q = cv::Mat::eye(4,4,CV_64F);
                cv::hconcat(cv::Mat::eye(3,3,CV_64F),
                            cv::Mat::zeros(3,1,CV_64F),
                            toIA);
            }
        };

        struct WFOVCalibrationData {
            std::string timestr;
            cv::Point3d labCenter;      //Pixel coordinate of lab center
            //cv::Mat extrinsics;
            WFOVCalibrationData() {
                timestr = "default";
                labCenter = {};
            }
        };

        struct DMDCalibrationData {
            std::string timestr;
            cv::Mat gK, gD, gR, gT;
            DMDCalibrationData() {
                timestr = "default";
                gK = cv::Mat::eye(3,3,CV_64F);
                gD = cv::Mat::zeros(1,5,CV_64F);
                gR = cv::Mat::zeros(3,1,CV_64F);
                gT = cv::Mat::zeros(3,1,CV_64F);
            }
        };

        struct PerspectiveTCalData {
            std::string timestr;
            cv::Mat nfovLeft;           //  3x3 Matrix for Left Camera Perspective Transform to on-axis view;
            cv::Mat wfov;               //  3x3 Matrix for Left Camera Perspective Transform to on-axis view;
            PerspectiveTCalData() {
                timestr = "default";
                nfovLeft = cv::Mat::zeros(3,4,CV_64F);
                wfov = cv::Mat::zeros(2,3,CV_64F);
            }
        };

        struct IRArcCalibration {
            std::string timestr;
            int IRTop = 0;
            int IRBot = 0;
        };

    } // namespaces
