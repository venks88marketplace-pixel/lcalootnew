#include "dmdcalibrator.h"
#include <QCoreApplication>
#include <QDebug>
#include <qimage.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include "blobfinder.h"
#include "pointstogrid.h"
#include "headers/calibrationStorage.h"

DMDCalibrator::DMDCalibrator(QObject* parent)
    : QObject(parent)  {
    loadCalibrationFileComponents();
}

void DMDCalibrator::loadCalibrationFileComponents(){
    m_calStore.getNfovCameraMatrices(m_params.D1,
                                                   m_params.K1,
                                                   m_params.D2,
                                                   m_params.K2);
    m_calStore.getProjectionMatrix(m_params.LP,
                                   m_params.RP);

    //left camera
    cv::decomposeProjectionMatrix(m_params.LP,
                                  m_params.KL,
                                  m_params.RL,
                                  m_params.tL4,
                                  m_params.rotXL,
                                  m_params.rotYL,
                                  m_params.rotZL);
    m_params.tL = m_params.tL4.rowRange(0, 3) / m_params.tL4.at<double>(3);

    //right camera
    cv::decomposeProjectionMatrix(m_params.RP,
                                  m_params.KR,
                                  m_params.RR,
                                  m_params.tR4,
                                  m_params.rotXR,
                                  m_params.rotYR,
                                  m_params. rotZR);
    m_params.tR = m_params.tR4.rowRange(0, 3) / m_params.tR4.at<double>(3);

    m_calStore.getStereoMatrix(m_params.F,
                              m_params.E,
                              m_params.Rst,
                              m_params.Tst,
                              m_params.R1,
                              m_params.R2,
                              m_params.P1,
                              m_params.P2,
                              m_params.Q);
    double x,
            y,
            z;
    m_calStore.getInstrumentAxis(m_params.Tr_IA, x, y, z);

    m_calStore.getNfovCameraMatrices(m_params.D1, //dist
                               m_params.KL, //intrinsic Left
                               m_params.D2, //dist
                               m_params.KR ); //intrinsic Right
}

fs::path DMDCalibrator::getExecutablePath(){
    return fs::path(
        QCoreApplication::applicationDirPath().toStdWString());
}

std::string DMDCalibrator::currentDateString() {
    auto p = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(p);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::stringstream ss;
    ss << std::put_time(&tm,"%Y_%b_%d_%H_%M_%S");
    return ss.str();
}
//DMD points are in pixels. The same for all views/poses of the cameras.
//They, dmd points, are imagePoints as far as calibrateCamera function is concerned.
//This function is called on every new pose of the stage.
cv::Mat DMDCalibrator::collectAndSaveUVDots(std::string& filename,
                                            cv::Mat& left,
                                            cv::Mat& right,
                                            Parameters& params) {
    std::vector<cv::Point3f> UV3D;
    std::vector<cv::Point2f> UVLeft, UVRight, UVDmd;
    qDebug() << "Left image, channels: " << left.channels();
    qDebug() << "Left image, type: " << left.type();

    cv::Mat disp = computeUVDots3D(left,
                                   right,
                                   params,
                                   UVLeft,
                                   UVRight,
                                   UV3D,
                                   UVDmd);
    if ( disp.empty()){
        qDebug() << "Grid detection FAILED!";
        return disp;
    }
    qDebug() << "points vector size: " << UVLeft.size();

    if (UV3D.empty() == false) {
        std::string fname = filename + "/" + currentDateString();
        qDebug() << "!!!!! filename: " << fname;
        qDebug() << "stored dmddots: " << UVDmd.size();

        cv::FileStorage fs(fname + "-UVdot.json", cv::FileStorage::FORMAT_JSON |  cv::FileStorage::WRITE );
        fs << "left" << UVLeft;
        fs << "right" << UVRight;
        fs << "stereo3D" << UV3D;
        fs << "dmddots" << UVDmd;
        fs.release();
        cv::imwrite(fname + "-left.png", left);
        cv::imwrite(fname + "-right.png", right);
        std::ofstream ofid(fname + "-UVdot.csv");
        for (int i=0; i<UV3D.size();++i) {
            ofid << UV3D[i] << "," << UVDmd[i] <<  std::endl;
        }
        ofid.close();
        qDebug() << "Saved to disk: " << fname;
    }
    else {
        qDebug() << "Failed to find grid of UV dots.";
    }
    return disp;
}


cv::Mat qImageToCvMatGrayscale(const QImage& qimage) {

    // Construct cv::Mat sharing the same memory buffer
    return cv::Mat(qimage.height(), qimage.width(), CV_8UC1,
                   const_cast<uchar*>(qimage.bits()),
                   qimage.bytesPerLine());
}

//UV dots in left and right images are objectPoints as far as calibrateCamera is concerned.
//dots with unmached indices are excluded from both left and right images.
//corrspondent DMD dot(s) for the unmached index(s) are also excluded
cv::Mat DMDCalibrator::computeUVDots3D(const cv::Mat& leftImage,
                        const cv::Mat& rightImage,
                        Parameters& params,
                        std::vector<cv::Point2f>& UVLeft,
                        std::vector<cv::Point2f>& UVRight,
                        std::vector<cv::Point3f>& UV3D,
                        std::vector<cv::Point2f>& UVDmd) {
    auto to2xN = [](const std::vector<cv::Point2f>& pts) {
        cv::Mat m(2, (int)pts.size(), CV_64F);
        for (int i = 0; i < (int)pts.size(); ++i) {
            m.at<double>(0,1) = pts[i].x;
            m.at<double>(1,0) = pts[i].y;
        }
        return m;
    };

    std::vector<cv::Point2f> leftDotCenters,
                            rightDotCenters; //pixel location of dots in the order correspondent to dots in DMD
    cv::Mat disp,
            disp1,
            dispLeft,
            dispRight;

    try{
        getIndexedImagePoints( leftImage,
                              cv::Size(params.nx, params.ny),
                              leftDotCenters);
        getIndexedImagePoints( rightImage,
                              cv::Size(params.nx, params.ny),
                              rightDotCenters);

        if (leftDotCenters.empty() == false && rightDotCenters.empty() == false) {
            cv::Mat estOutM(4, leftDotCenters.size(), CV_64F);
            std::vector<cv::Point2f> uLeft, uRight;
            cv::Mat mL, mR;

            //Reading DMD image, detecting centers
            auto qimg_dmd = QImage(":/images/dotImage_3_5x3.bmp");
            auto img_dmd = qImageToCvMatGrayscale(qimg_dmd);

            std::vector<cv::Point2f> blobCentersDmd = BlobFinder::findBlobs(img_dmd);
            qDebug() << "DMD: blobs found: " << blobCentersDmd.size() << " Expected: " <<
                                                (params.nx * params.ny);
            if (blobCentersDmd.size() < (params.nx * params.ny)){
                return disp;
            }
            //points arranged by rows (2D array)
            LabeledGrid lgDmd = PointsToGrid::labelCircleGrid(blobCentersDmd,
                                                              params.nx,
                                                              params.ny);
            if ( lgDmd.grid.size() != params.ny && lgDmd.grid[0].size() != params.nx) {
                std::cout << "Failed to compute proper grid for dmd image." << std::endl;
                return disp;
            }

            //get ordered dot positions
            int dot=1;
            for (int r = 0; r < lgDmd.grid.size(); r++) {
                for (int c = 0; c < lgDmd.grid[0].size(); c++){
                    qDebug() << "dmd dot: " << dot++ << "   x: "
                              << lgDmd.grid[r][c].x << "    y: "
                              << lgDmd.grid[r][c].y;
                    UVDmd.push_back(cv::Point2f(lgDmd.grid[r][c].x,
                                                lgDmd.grid[r][c].y));
                }
            }
            qDebug() << "undistorting leftDotCenters";
            cv::undistortPoints(leftDotCenters,
                                uLeft,
                                params.KL,
                                params.D1,
                                params.R1,
                                params.P1);
            qDebug() << "undistorting rightDotCenters";
            cv::undistortPoints(rightDotCenters,
                                uRight,
                                params.KR,
                                params.D2,
                                params.R2,
                                params.P2);

            qDebug() << "triangulating ....";
            cv::triangulatePoints(params.P1,
                                params.P2,
                                uLeft,
                                uRight,
                                estOutM);

            qDebug() << "triangulation completed!";

            qDebug() << "compute 3D in non-homogeneous coordinates";
            auto sz = leftDotCenters.size();
            UV3D.reserve(sz);
            UVLeft.reserve(sz);
            UVRight.reserve(sz);

            for (uint i = 0; i < sz; ++i) {
                cv::Mat d = estOutM.col(i).t();
                qDebug() << "d point: " << i <<  " { x,y,z,w ): }"
                          << d.at<float>(0) << ", "
                          << d.at<float>(1) << ", "
                          <<  d.at<float>(2) <<  ","
                          << d.at<float>(3);

                cv::Mat dd = d.clone();
                for (int i = 0; i < d.total(); i++)
                    dd.at<float>(i) = d.at<float>(i) / d.at<float>(d.total()-1);

                cv::Mat uvInIA;
                cv::Mat ddt;
                cv::transpose(dd, ddt);

                cv::Mat Tr_IA_float;
                params.Tr_IA.convertTo(Tr_IA_float, CV_32F);

                uvInIA = Tr_IA_float * ddt;

                UV3D.push_back(cv::Point3f{uvInIA.at<float>(0), uvInIA.at<float>(1), uvInIA.at<float>(2)});
                qDebug() << "{UVC3D: x,y,z ): }"
                         << uvInIA.at<float>(0)
                         << ", "
                         << uvInIA.at<float>(1)
                         << ", " <<  uvInIA.at<float>(2);

                UVLeft.push_back(leftDotCenters[i]);
                UVRight.push_back(rightDotCenters[i]);
            }

            dispLeft = leftImage.clone();
            dispRight = rightImage.clone();
            for (uint i = 0; i < sz; ++i) {
                cv::circle(dispLeft,
                           cv::Point(leftDotCenters[i]),
                           3,
                           cv::Scalar{0,20,234},cv::FILLED );
                cv::circle(dispRight,
                           cv::Point(rightDotCenters[i]),
                           3,
                           cv::Scalar{0,65,234},cv::FILLED );
            }
            cv::hconcat(dispLeft, dispRight, disp1);
            cv::resize(disp1, disp, cv::Size{}, 0.5, 0.5);
            return disp;
        }
        else {
            return disp;
        }
    }
    catch(std::exception& e){
        qDebug() << "exception caught:" << e.what();
        return disp;
    }
}

void DMDCalibrator::getIndexedImagePoints( const cv::Mat& cameraImage,
                                          const cv::Size patternDims,
                                          std::vector<cv::Point2f>& dotCenters){
    try{
        //uv blobs
        std::vector<cv::Point2f> blobCentersLeft = BlobFinder::findBlobs(cameraImage);
        for ( int i = 0; i < blobCentersLeft.size(); i++)
            qDebug() << "dot: " << i << "   x: " << blobCentersLeft[i].x << "    y: " << blobCentersLeft[i].y;
        qDebug() << "left: blobs found: " << blobCentersLeft.size() << " Expected: "
                 << (patternDims.width * patternDims.height);
        if (blobCentersLeft.size() < (patternDims.width * patternDims.height)){
            return;
        }
        //points arranged by rows (2D array)
        LabeledGrid lgL = PointsToGrid::labelCircleGrid(blobCentersLeft,
                                                        patternDims.width,
                                                        patternDims.height);
        if ( lgL.grid.size() != patternDims.height && lgL.grid[0].size() != patternDims.width) {
            qDebug() << "Failed to compute proper grid for left image.";
            return;
        }
        //get ordered dot positions
        int lgRowCount = lgL.grid.size();
        int lgColCount = lgL.grid[0].size();
        for (int r = 0; r < lgRowCount; r++) {
            for (int c = 0; c < lgColCount; c++){
                dotCenters.push_back(cv::Point2f(lgL.grid[r][c].x, lgL.grid[r][c].y));
            }
        }
        //due to mirroring of the image
        reverse_grid_elements(dotCenters, patternDims);
    }
    catch(std::exception& e){
        qDebug() << "exception caught:" << e.what();
        throw std::runtime_error("Insufficient dataset in the image.");
    }
}

//iterate over a list of saved image planes data and accumulate object and image points
void DMDCalibrator::trainDMDCM(const std::string& filename,
                               Parameters& params) {
    qDebug() << "training with data in file: " << filename;

    std::vector<std::string> uvDotFiles;
    auto num = numberOfSavedUVPlanes(filename, uvDotFiles);
    qDebug() << "Number of captured UV-Dot planes for training: " << num;
    if (num < 2) {
        qDebug() << "Need more UV-Dot planes";
        return;
    }
    std::vector<cv::Point3f> objectPoints; objectPoints.reserve(1000);
    std::vector<cv::Point2f> imagePoints; imagePoints.reserve(1000);
    for (auto &lf : uvDotFiles) {
        cv::FileStorage fs(lf.c_str(),cv::FileStorage::FORMAT_JSON |  cv::FileStorage::READ );
        qDebug() << "File: " << lf.c_str();

        std::vector<cv::Point2i> dmddots;
        std::vector<cv::Point3f> uv3D;
        std::vector<cv::Point2f> uvLeft, uvRight;
        fs["left"] >> uvLeft;
        fs["right"] >> uvRight;
        fs["stereo3D"] >> uv3D;
        fs["dmddots"] >> dmddots;
        fs.release();

        qDebug() << "uv3D size: " << uv3D.size() << " , " << "dmddots size: " << dmddots.size();

        objectPoints.insert(objectPoints.end(),
                            uv3D.begin(),
                            uv3D.end());
        imagePoints.insert(imagePoints.end(),
                           dmddots.begin(),
                           dmddots.end());
    }
    std::vector<std::vector<cv::Point3f>> objectPoints2; objectPoints2.push_back(objectPoints);
    std::vector<std::vector<cv::Point2f>> imagePoints2; imagePoints2.push_back(imagePoints);
    std::vector<cv::Mat> rvecs, tvecs;
    cv::Mat cameraMatrix = cv::Mat::eye(3, 3, CV_64F);
    cv::Mat distCoeffs = cv::Mat::zeros(5, 1, CV_64F);
    auto flag = cv::CALIB_USE_INTRINSIC_GUESS ;
    double f0 = 3500.0;
    cv::Size imageSize {1024, 768};
    cameraMatrix.at<double>(0, 0) = f0;
    cameraMatrix.at<double>(1, 1) = f0;
    cameraMatrix.at<double>(0, 2) = params.Cx;
    cameraMatrix.at<double>(1, 2) = params.Cy;

    qDebug() << "ready to run calibrateCamera.";
    qDebug() << "objectPoints size: " << objectPoints2.size();
    qDebug() << "imagePoints size: " << imagePoints2.size();

    double rms = calibrateCamera(objectPoints2,
                                 imagePoints2,
                                 imageSize,
                                 cameraMatrix,
                                 distCoeffs,
                                 rvecs,
                                 tvecs,
                                 flag);
    qDebug() << "calibrateCamera executed";

    bool ok = checkRange(cameraMatrix) && checkRange(distCoeffs);
    qDebug() << "Training complete : " << (int)ok;
    qDebug() << "RMS error: " << rms << " pixels";
    std::vector<cv::Point2f> Yest;
    cv::projectPoints( objectPoints,
                      rvecs[0],
                      tvecs[0],
                      cameraMatrix,
                      distCoeffs,
                      Yest);
    double error = 0;
    std::string perfcsv = filename + "trainingDMDCMPerformance.csv";
    std::ofstream fid(perfcsv);
    for (int i=0; i<imagePoints.size(); ++i) {
        fid << i << ";" << objectPoints[i] << ";"
            << imagePoints[i] << ";"
            << Yest[i] << ";"
            << cv::norm(imagePoints[i] - Yest[i]) << ";"
            << imagePoints[i] - Yest[i] << std::endl;
        error += cv::norm(imagePoints[i] - Yest[i]);
    }
    fid.close();
    qDebug() << "Avg training error: " << error/imagePoints.size() << " pixels";
    calibrationStorage cal;
    cal.updateDMDParameters(cameraMatrix,
                            distCoeffs,
                            rvecs[0],tvecs[0]);
}

size_t DMDCalibrator::numberOfSavedUVPlanes(const std::string& filename, std::vector<std::string>& files) {
    try{
        cv::glob(filename + "/*-UVdot.json", files);
        return files.size();
    }catch(...)
    {
        return 0;
    }
}
