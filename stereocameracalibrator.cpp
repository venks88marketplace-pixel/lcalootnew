#include "stereocameracalibrator.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <format>
#include <time.h>   // for time
#include <stdlib.h> // for rand & srand
#include "opencv2/imgproc.hpp"
#include "opencv2/calib3d.hpp"
#include "headers/calibrationStorage.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

StereoCameraCalibrator::StereoCameraCalibrator(cv::Size patternsize, QObject* parent)
    : QObject(parent),
    m_patternSize{patternsize}{
    //hardcoded for now
    for (float z = 1.0; z <= 21.0; z += 1.0) {
        m_assistNames.insert({z,""});
    }
}

std::string StereoCameraCalibrator::currentDateString() {
    auto p = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(p);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::stringstream ss;
    ss << std::put_time(&tm,"%Y_%b_%d_%H_%M_%S");
    //std::cout << ss.str() << std::endl;
    return ss.str();
}

void StereoCameraCalibrator::generatePairs(std::vector<std::pair<int,int>>& Hpairs,
       std::vector<std::pair<int,int>>& Vpairs,
       cv::Size& patternsize) {
    int n = patternsize.width;
    int m = patternsize.height;
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            int k = i * n + j;
            if (j>0) {
                Hpairs.push_back({k, k - 1});
            }
            if (i>0) {
                Vpairs.push_back({k, k - n});
            }
        }
    }
}

std::vector< cv::Point2f >
StereoCameraCalibrator::calibrationPattern2D(const double hx,
          const double hy,
          cv::Size chessboardSize) {
    std::vector< cv::Point2f > pat;
    float y = -(chessboardSize.height - 1) / 2;
    for (int i = 0; i < chessboardSize.height; i++, y += 1) {
        float x = -(chessboardSize.width - 1) / 2;
        for (int j = 0; j < chessboardSize.width; j++, x += 1) {
            pat.push_back(cv::Point2f(x * hx, y * hy));
        }
    }
    return pat;
}

void StereoCameraCalibrator::checkerboardCorners3D(std::vector<calibPlane>& planes,
                            cv::Size& patternSize,
                            float hx,
                            float hy,
                           std::vector<std::vector<cv::Point2f>>& leftPlanes,
                           std::vector<std::vector<cv::Point2f>>& rightPlanes,
                           std::vector<std::vector< cv::Point3f >>& obj3D) {

    leftPlanes.resize(1);
    rightPlanes.resize(1);
    obj3D.resize(1);
    auto &LP = leftPlanes[0];
    auto &RP = rightPlanes[0];
    auto &OP = obj3D[0];

    auto pat = calibrationPattern2D( hx, hy, patternSize );
    float minz = 100000, maxz = -10000;
    for (auto& p : planes) {
        if (p.zSeq < minz) minz = p.zSeq;
        if (p.zSeq > maxz) maxz = p.zSeq;
    }
    std::cout << "Z range = [" << minz << " - " << maxz << "]" << std::endl;
    for (auto& p : planes) {
        p.zSeq = p.zSeq - ((maxz + minz) / 2);
        p.zSeq *= -1.0;
    }
    for (auto& p : planes) {
        std::vector<cv::Point2f> left_corners;
        std::vector<cv::Point2f> right_corners;
        cv::FileStorage fs(p.fname, cv::FileStorage::FORMAT_JSON |  cv::FileStorage::READ );
        fs["left"] >> left_corners;
        fs["right"] >> right_corners;
        fs.release();
        std::copy(left_corners.begin(), left_corners.end(), std::back_inserter(LP));
        std::copy(right_corners.begin(), right_corners.end(), std::back_inserter(RP));
        for (auto& op : pat) {
            OP.push_back({op.x, op.y, p.zSeq});
        }
    }
}

double StereoCameraCalibrator::computeReprojectionErrors( const std::vector<std::vector<cv::Point3f> >& objectPoints,
                                 const std::vector<std::vector<cv::Point2f> >& imagePoints,
                                 const std::vector<cv::Mat>& rvecs,
                                 const std::vector<cv::Mat>& tvecs,
                                 const cv::Mat& cameraMatrix ,
                                 const cv::Mat& distCoeffs,
                                 std::vector<float>& perViewErrors) {
    std::vector<cv::Point2f> imagePointsReprojected;
    int i, totalPoints = 0;
    double totalErr = 0, err;
    perViewErrors.resize(objectPoints.size());

    for( i = 0; i < (int)objectPoints.size(); ++i )
    {
        cv::projectPoints( cv::Mat(objectPoints[i]), rvecs[i], tvecs[i], cameraMatrix,
                          distCoeffs, imagePointsReprojected);
        err = norm(cv::Mat(imagePoints[i]), cv::Mat(imagePointsReprojected), cv::NORM_L2);

        int n = (int)objectPoints[i].size();
        perViewErrors[i] = (float) std::sqrt(err*err/n);
        totalErr        += err*err;
        totalPoints     += n;
    }
    return std::sqrt(totalErr/totalPoints);
}

std::vector< cv::Point3f> StereoCameraCalibrator::estimate3DPts(const calibration::Parameter& param,
                                       const std::vector < cv::Point2f >&left,
                                       const std::vector < cv::Point2f >&right) {
    cv::Mat estOutM(4,left.size(),CV_64F);
    std::vector<cv::Point2f> uLeft, uRight;
    std::vector< cv::Point3f > estOutPts;

    if (param.useRectification == true) {
        cv::undistortPoints(left, uLeft, param.K1, param.D1, param.R1, param.P1);
        cv::undistortPoints(right, uRight, param.K2, param.D2, param.R2, param.P2);
    }
    else {
        cv::undistortPoints(left, uLeft, param.K1, param.D1, cv::Mat{}, param.K1);
        cv::undistortPoints(right, uRight, param.K2, param.D2, cv::Mat{}, param.K2);
    }
    cv::triangulatePoints(param.P1, param.P2, uLeft, uRight, estOutM);

    for (uint i=0; i<left.size(); ++i) {
        auto d = estOutM.col(i).t();
        cv::Mat dd;
        cv::convertPointsFromHomogeneous(d, dd);

        estOutPts.push_back({dd.at<cv::Vec3f>(0)[0], dd.at<cv::Vec3f>(0)[1], dd.at<cv::Vec3f>(0)[2]});
        //P(i << ") l=" << left[i] << ", r=" << right[i] <<", 3d="<< dd  << " OR " << estOutPts.back())
    }
    return estOutPts;
}


StereoCameraCalibrator::Stats StereoCameraCalibrator::computeStats(const std::vector<double>& data) {
    if (data.empty()) {
        throw std::invalid_argument("Data set cannot be empty.");
    }
    double mean = 0.0;
    double M2 = 0.0; // Sum of squares of differences from the mean
    size_t n = 0;
    for (double x : data) {
        n++;
        double delta = x - mean;
        mean += delta / n;
        double delta2 = x - mean;
        M2 += delta * delta2;
    }
    double variance = (n > 1) ? M2 / (n - 1) : 0.0; // Sample variance
    return { mean, std::sqrt(variance) };
}

double StereoCameraCalibrator::calibrateSingleCamera(const std::vector<std::vector<cv::Point2f>>& imagePoints,
                            const std::vector<std::vector<cv::Point3f>>& objectPoints,
                            const cv::Size imageSize,
                            cv::Mat& cameraMatrix,
                            cv::Mat& distCoeffs,
                            std::vector<float>& reprojErrs) {
    std::vector<cv::Mat> rvecs, tvecs;
    cameraMatrix = cv::Mat::eye(3, 3, CV_64F);
    distCoeffs = cv::Mat::zeros(8, 1, CV_64F);
    auto flag = cv::CALIB_USE_INTRINSIC_GUESS |
                cv::CALIB_FIX_ASPECT_RATIO |
                cv::CALIB_FIX_K4 |
                cv::CALIB_FIX_K5 |
                cv::CALIB_ZERO_TANGENT_DIST |
                cv::CALIB_FIX_PRINCIPAL_POINT;
    double f0 = 1714.0;
    cameraMatrix.at<double>(0,0) = f0;
    cameraMatrix.at<double>(1,1) = f0;
    cameraMatrix.at<double>(0,2) = imageSize.width/2;
    cameraMatrix.at<double>(1,2) = imageSize.height/2;
    std::cout << "Single camera Calibrate camera with " << objectPoints.size() << " views." << std::endl;

    double rms = calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix,
                                 distCoeffs, rvecs, tvecs, flag );
    std::cout << "RMS error: " << rms << std::endl;
    //std::cout << "Camera matrix " << cameraMatrix << std::endl;
    //std::cout << "Distortion matrix " << distCoeffs << std::endl;
    bool ok = checkRange(cameraMatrix) && checkRange(distCoeffs);
    std::cout << "Checking sanity of matrix values: " << (int)ok << std::endl;
    double totalAvgErr = computeReprojectionErrors(objectPoints, imagePoints,
                                                   rvecs, tvecs, cameraMatrix, distCoeffs, reprojErrs);
    std::cout << "Average 2D error check (pixels): " << totalAvgErr << std::endl;
    return totalAvgErr;
}

cv::Mat StereoCameraCalibrator::dataCollection(const cv::Mat& calLeft,
                                               const cv::Mat& calRight,
                                               std::string filename,
                                               std::string folder,
                                               float z) {

    std::vector<cv::Point2f> left_corners;
    std::vector<cv::Point2f> right_corners;
    cv::Mat combo = cv::Mat{};
    bool found1 = cv::findChessboardCornersSB(calLeft, m_patternSize, left_corners, 0);
    if (found1) {
        cv::cornerSubPix(calLeft, left_corners, cv::Size(11, 11),
             cv::Size(-1, -1),
             cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.1));
    }
    bool found2 = cv::findChessboardCornersSB(calRight, m_patternSize, right_corners, 0);
    if (found2) {
        cv::cornerSubPix(calRight, right_corners, cv::Size(11, 11),
             cv::Size(-1, -1),
             cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.1));
    }
    if (found1 and found2) {
        int zint = static_cast<int>(z);

        if (zint == 11) {//FOP
            cv::Point2f leftObjCenter, rightObjCenter;
            std::vector<cv::Point> leftCrop, rightCrop;

            findBoundingCorners(left_corners, leftCrop, leftObjCenter);
            findBoundingCorners(right_corners, rightCrop, rightObjCenter);

            StereoCameraCalibrator::CameraCorners leftCamera;
            for( int i = 0; i < leftCrop.size(); i++) {
                switch(i){
                case 0:
                    leftCamera.topLeft.x = leftCrop[i].x;
                    leftCamera.topLeft.y = leftCrop[i].y;
                    break;
                case 1:
                    leftCamera.topRight.x = leftCrop[i].x;
                    leftCamera.topRight.y = leftCrop[i].y;
                    break;
                case 2:
                    leftCamera.bottomRight.x = leftCrop[i].x;
                    leftCamera.bottomRight.y = leftCrop[i].y;
                    break;
                case 3:
                    leftCamera.bottomLeft.x = leftCrop[i].x;
                    leftCamera.bottomLeft.y = leftCrop[i].y;
                    break;
                }
            }
            StereoCameraCalibrator::CameraCorners rightCamera;
            for( int i = 0; i < rightCrop.size(); i++) {
                switch(i){
                case 0:
                    rightCamera.topLeft.x = rightCrop[i].x;
                    rightCamera.topLeft.y = rightCrop[i].y;
                    break;
                case 1:
                    rightCamera.topRight.x = rightCrop[i].x;
                    rightCamera.topRight.y = rightCrop[i].y;
                    break;
                case 2:
                    rightCamera.bottomRight.x = rightCrop[i].x;
                    rightCamera.bottomRight.y = rightCrop[i].y;
                    break;
                case 3:
                    rightCamera.bottomLeft.x = rightCrop[i].x;
                    rightCamera.bottomLeft.y = rightCrop[i].y;
                    break;
                }
            }
            saveTargetCorners("C://calibration//apps//forStereoCBCorners.json",
                                   leftCamera,
                                    rightCamera);
        }

        if (folder.empty() == false) {
            std::string leftFname = folder + "/" + filename + "-left.png";
            std::string rightFname = folder + "/" + filename + "-right.png";
            std::string chkFname = folder + "/" + filename + "-planes.json";

            bool s = cv::imwrite(leftFname, calLeft);
            qDebug() << "storing left image " << s;
            bool ss = cv::imwrite(rightFname, calRight);
            qDebug() << "storing right image " << ss;

            m_assistNames[z] = filename;

            cv::FileStorage fs(chkFname.c_str(), cv::FileStorage::FORMAT_JSON |  cv::FileStorage::WRITE );
            //fs << "Obj3D" << pattern;
            fs << "left" << left_corners;
            fs << "right" << right_corners;
            fs.release();
        }
    }
    else {
        qDebug() << "Checkerboard not detected in at-least one view." ;
    }
    return combo;
}

cv::Mat StereoCameraCalibrator::verification(int zplane, const cv::Mat& calLeft,
                                             const cv::Mat& calRight,
                                             std::string folder) {
    std::vector<cv::Point2f> left_corners;
    std::vector<cv::Point2f> right_corners;
    cv::Mat combo = cv::Mat{};
    //std::cout << "OK here first" << std::endl;
    bool found1 = cv::findChessboardCornersSB(calLeft, m_patternSize, left_corners, 0);
    if (found1) {
        //std::cout << "OK here inside" << std::endl;
        cv::cornerSubPix(calLeft, left_corners, cv::Size(11, 11),
                         cv::Size(-1, -1),
                         cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.1));
    }
    //std::cout << "OK here second" << std::endl;
    bool found2 = cv::findChessboardCornersSB(calRight, m_patternSize, right_corners, 0);
    if (found2) {
        //std::cout << "OK here inside2" << std::endl;
        cv::cornerSubPix(calRight, right_corners, cv::Size(11, 11),
                         cv::Size(-1, -1),
                         cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30, 0.1));

    }
    //std::cout << "OK here inside" << std::endl;
    //combo = showCorners(calLeft, calRight, left_corners, right_corners, m_patternSize);
    if (found1 and found2) {
        calibrationStorage calStore;
        calibration::Parameter paramLocal;
        calStore.getNfovCameraMatrices(paramLocal.D1, paramLocal.K1, paramLocal.D2, paramLocal.K2);
        calStore.getStereoMatrix(paramLocal.F, paramLocal.E, paramLocal.R, paramLocal.T,
                                 paramLocal.R1, paramLocal.R2, paramLocal.P1, paramLocal.P2, paramLocal.Q);
        paramLocal.useRectification = true;
        std::vector<cv::Point3f> pts3D = estimate3DPts(paramLocal, left_corners, right_corners);
        std::vector<std::pair<int, int>> Hpairs, Vpairs;
        generatePairs(Hpairs, Vpairs, m_patternSize);
        std::vector<double> HerrorPerPlane, VerrorPerPlane;
        std::vector<double> hdist, vdist;
        //P("Horizontal diffs")
        for (auto &p : Hpairs) {
            auto d1 = pts3D[p.first] - pts3D[p.second];
            auto n1 = cv::norm(d1);
            hdist.push_back(n1);
            //P("[" << p.first << "," << p.second << "] - " << n1 )
        }

        //P("Vertical diffs")
        for (auto &p : Vpairs) {
            auto d1 = pts3D[p.first] - pts3D[p.second];
            auto n1 = cv::norm(d1);
            vdist.push_back(n1);
            //P("[" << p.first << "," << p.second << "] - "  << n1 )
        }

        Stats stat = computeStats(hdist);
        double hmean = stat.mean;
        double hstd = stat.stddev;

        stat = computeStats(vdist);
        double vmean = stat.mean;
        double vstd = stat.stddev;
        //average z value
        std::vector<double> zvalues(pts3D.size());
        for (int k=0; k<pts3D.size(); ++k) {
            zvalues[k] = pts3D[k].z;
        }

        stat = computeStats(zvalues);
        double zmean = stat.mean;
        double zstd = stat.stddev;

        qDebug() << "Checkerboard with " << m_patternSize.width << "x" << m_patternSize.height << " corners.";
        qDebug() << "Horizontal mean spacing of checkerboard: " << hmean << "mm, (stddev: " << hstd << "mm)";
        qDebug() << "Vertical mean spacing of checkerboard  : " << vmean << "mm, (stddev: " << vstd << "mm)";
        qDebug() << "Average z values of the plane          : " << zmean << "mm, (stddev: " << zstd << "mm)";

        m_results.results[(std::ostringstream() << "HMS_z" << zplane).str()] = hmean;
        m_results.results[(std::ostringstream() << "VMS_z" << zplane).str()] = vmean;
        m_results.results[(std::ostringstream() << "Z_" << std::setfill('0') << std::setw(2) << zplane).str()] = zmean;

        if (folder.empty() == false) {
            std::string filename = folder + "/TEST_" + currentDateString();
            cv::imwrite(filename + "-left.png", calLeft);
            cv::imwrite(filename + "-right.png", calRight);
        }
    }
    else {
        std::cout << "A checkerboard with " << m_patternSize.width << "x" << m_patternSize.height << " corners was not detected by one or two cameras." << std::endl;
    }
    return combo;
}

double StereoCameraCalibrator::computeReprojectionErrors3D( const std::vector<cv::Point3f> & objectPoints,
                                const std::vector<cv::Point2f>& imagePoints,
                                const cv::Mat& rvecs,
                                const cv::Mat& tvecs,
                                const cv::Mat& cameraMatrix ,
                                const cv::Mat& distCoeffs,
                                float& rmsError) {
    std::vector<cv::Point2f> imagePointsReprojected;
    cv::projectPoints( cv::Mat(objectPoints), rvecs, tvecs, cameraMatrix,
                      distCoeffs, imagePointsReprojected);
    double err = norm(cv::Mat(imagePoints), cv::Mat(imagePointsReprojected), cv::NORM_L2);
    int n = (int)objectPoints.size();
    rmsError = (float) std::sqrt(err*err/n);
    return err/n; //avg error
}

double StereoCameraCalibrator::calibrateSingleCamera3D(const std::vector<std::vector<cv::Point2f>>& imagePoints,
                               const std::vector<std::vector<cv::Point3f>>& objectPoints,
                               const cv::Size imageSize,
                               cv::Mat& cameraMatrix,
                               cv::Mat& distCoeffs,
                               cv::Mat& rvec,
                               cv::Mat& tvec,
                               float& rmsError) {
    std::vector<cv::Mat> rvecs, tvecs;
    cameraMatrix = cv::Mat::eye(3, 3, CV_64F);
    distCoeffs = cv::Mat::zeros(8, 1, CV_64F);
    auto flags = cv::CALIB_USE_INTRINSIC_GUESS;

    double f0 = 9124.08; // 25mm focal distance / 2.74 um pixel size
    cameraMatrix.at<double>(0,0) = f0;
    cameraMatrix.at<double>(1,1) = f0;
    cameraMatrix.at<double>(0,2) = imageSize.width/2;
    cameraMatrix.at<double>(1,2) = imageSize.height/2;
    std::cout << "Single camera Calibrate camera with " << objectPoints.size() << " views." << std::endl;

    double rms = calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix,
                                 distCoeffs, rvecs, tvecs, flags );

    rvec = rvecs[0];
    tvec = tvecs[0];

    std::cout << "RMS error: " << rms << std::endl;
    //std::cout << "Camera matrix " << cameraMatrix << std::endl;
    //std::cout << "Distortion matrix " << distCoeffs << std::endl;
    bool ok = checkRange(cameraMatrix) && checkRange(distCoeffs);
    std::cout << "Checking sanity of matrix values: " << (int)ok << std::endl;
    double avgError = computeReprojectionErrors3D(objectPoints[0], imagePoints[0],
                                                  rvec, tvec, cameraMatrix, distCoeffs, rmsError);
    std::cout << "Average 2D error check (pixels): " << avgError << std::endl;
    return 0;
}

double StereoCameraCalibrator::StereoAnalysis3D( calibration::Parameter& param,
                        const std::vector<cv::Point2f>& leftPlanes,
                        const std::vector<cv::Point2f>& rightPlanes,
                        const std::vector<cv::Point3f>& objPoints,
                        std::vector < cv::Point3f>& estOutPts) {
    cv::Mat R1, R1inv, R2, R2inv;
    cv::Rodrigues(param.rvec1, R1);
    cv::Rodrigues(param.rvec2, R2);
    cv::Mat X1, X2;
    cv::hconcat(R1, param.tvec1, X1);
    cv::hconcat(R2, param.tvec2, X2);
    param.Pex1 = param.K1 * X1;
    param.Pex2 = param.K2 * X2;
    cv::Size imgSize {2016, 1452};
    cv::Mat perViewReprojectionError;
    double reprojError = cv::stereoCalibrate(std::vector<std::vector<cv::Point3f>>{objPoints},
                                             std::vector<std::vector<cv::Point2f>>{leftPlanes},
                                             std::vector<std::vector<cv::Point2f>>{rightPlanes},
                                             param.K1, param.D1, param.K2, param.D2, imgSize,
                                             param.R, param.T, param.E, param.F,
                                             perViewReprojectionError, cv::CALIB_FIX_INTRINSIC,
                                             cv::TermCriteria( cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 100, 1e-8)
                                             );
    //std::cout << "Total reprojection error: " << reprojError << std::endl;
    cv::stereoRectify(param.K1, param.D1, param.K2, param.D2, imgSize, param.R, param.T,
                      param.R1, param.R2, param.P1, param.P2, param.Q, cv::CALIB_ZERO_DISPARITY);
    cv::Mat estOutM(4,1,CV_64F);
    std::vector<cv::Point2f> uLeft, uRight;
    cv::Mat estOutMr(4,1,CV_64F);
    std::vector<cv::Point2f> uLeftr, uRightr;
    //std::vector < cv::Point3f> estOutPts;
    estOutPts.reserve(leftPlanes.size());

    cv::undistortPoints(leftPlanes, uLeft, param.K1, param.D1, cv::Mat{}, param.K1);
    cv::undistortPoints(rightPlanes, uRight, param.K2, param.D2, cv::Mat{}, param.K2);
    cv::triangulatePoints(param.Pex1, param.Pex2, uLeft, uRight, estOutM);

    cv::undistortPoints(leftPlanes, uLeftr, param.K1, param.D1, param.R1, param.P1);
    cv::undistortPoints(rightPlanes, uRightr, param.K2, param.D2, param.R2, param.P2);
    cv::triangulatePoints(param.P1, param.P2, uLeftr, uRightr, estOutMr);

    double errorMM  = 0;
    for (uint i=0; i<leftPlanes.size(); ++i) {
        //P(leftPlanes[i] << uLeft[i] << rightPlanes[i] << uRight[i])
        //cv::Mat d, dd;
        cv::Mat d;
        d = estOutM.col(i).t();
        //cv::convertPointsFromHomogeneous(d, dd);
        cv::Mat dd = d.clone();
        for (int i = 0; i < d.total(); i++)
            dd.at<float>(i) = d.at<float>(i) / d.at<float>(d.total()-1);

        estOutPts.push_back({dd.at<float>(0), dd.at<float>(1), dd.at<float>(2)});
        //std::cout << i << ") l=" << leftPlanes[i] << ", r=" << rightPlanes[i]
        //          << '\t' << estOutPts.back() << " for " << objPoints[i] << std::endl;
        errorMM += cv::norm(estOutPts.back() - objPoints[i]);
    }
    return errorMM / leftPlanes.size();
}

void StereoCameraCalibrator::loadDataFiles(std::vector<calibPlane>& calibData,
                            std::string fname,
                            std::string base_folder,
                            cv::Size& chessboardSize) {
    // 1.0,XXX-left.png
    if (base_folder.back() != '/') {
        base_folder.append("/");
    }
    if (fs::exists(fname) == false) {
        std::cout << "CSV file with list of z-mapping not found (" << fname << ")" << std::endl;
        return;
    }
    std::ifstream fid(fname);
    while (fid.good()) {
        std::string line;
        getline(fid,line);
        if (line.size() < 10 || line[0] == '#') continue;
        size_t n = 0, n1 = 0;
        calibPlane plane;
        n = line.find_first_of(',',n1);
        if (n != std::string::npos) {
            plane.label = line.substr(n1, n-n1); n1 = n + 1;
            std::istringstream(plane.label) >> plane.zSeq ;
        }
        else {
            continue;
        }
        plane.fname = base_folder + line.substr(n1, n-n1) + "-planes.json";
       // auto lfname = base_folder + line.substr(n1, n-n1) + "-left.png";
       // auto rfname = base_folder + line.substr(n1, n-n1) + "-right.png";
        if (fs::exists(plane.fname.c_str())== false) {
            std::cout << "Z = " << plane.label << "Missing file: "
                      << plane.fname << std::endl;
        }
        else {
            std::cout << "Z = " << plane.zSeq << ", files:"
                      << plane.fname << " loaded." << std::endl;
            calibData.push_back(plane);
        }
    }
    std::cout << "Checkerboard corners from " << calibData.size()
              << " planes loaded." << std::endl;
    fid.close();
}

void StereoCameraCalibrator::train3D(const double hx,
                                     const double hy,
                                     const std::string& folder) {

    std::string inpFname = folder + "/ImageList3D.csv";
    std::cout << "Reading " << inpFname << std::endl;
    std::vector<calibPlane> planes;
    loadDataFiles(planes, inpFname, folder, m_patternSize);
    std::vector < std::vector < cv::Point2f >> leftPlanes, rightPlanes;
    std::vector < std::vector< cv::Point3f >> objPoints;
    checkerboardCorners3D(planes, m_patternSize, hx, hy, leftPlanes, rightPlanes, objPoints);
    double rightError = 0.0, leftError = 0.0;
    double reprojError = 0.0;
    cv::Size size {2016, 1452};
    float reprojErrsLeft, reprojErrsRight;
    leftError = calibrateSingleCamera3D(leftPlanes, objPoints, size, m_param.K1, m_param.D1, m_param.rvec1, m_param.tvec1,reprojErrsLeft);
    rightError = calibrateSingleCamera3D(rightPlanes, objPoints, size, m_param.K2, m_param.D2, m_param.rvec2, m_param.tvec2, reprojErrsRight);
    std::vector<cv::Point3f> est3Dpoints;
    double error3d = StereoAnalysis3D(m_param, leftPlanes[0], rightPlanes[0], objPoints[0], est3Dpoints);
    std::cout << "Average reprojection error(mm): " << error3d << std::endl;
    m_results.results["AverageReprojectionError"] = error3d;
    std::string resultFile = folder + "/" + "stereoCalibration3D.json";
    cv::FileStorage fr(resultFile.c_str(), cv::FileStorage::FORMAT_JSON |  cv::FileStorage::WRITE );
    fr << "hx" << hx;
    fr << "hy" << hy;
    fr << "NumPts" << (int)est3Dpoints.size();
    fr << "leftError" << leftError;
    fr << "rightError" << rightError;
    fr << "TotalReprojectionError" << reprojError;
    fr << "LeftIntrinsic" << m_param.K1;
    fr << "LeftDistortion" << m_param.D1;
    fr << "RightIntrinsic" << m_param.K2;
    fr << "RightDistortion" << m_param.D2;
    fr << "R" << m_param.R;
    fr << "T" << m_param.T;
    fr << "E" << m_param.E;
    fr << "F" << m_param.F;
    fr << "R1" << m_param.R1;
    fr << "R2" << m_param.R2;
    fr << "P1" << m_param.P1;
    fr << "P2" << m_param.P2;
    fr << "Q" << m_param.Q;
    fr << "leftReprojErrs" << reprojErrsLeft;
    fr << "rightReprojErrs" << reprojErrsRight;
    fr << "Reprojection Error3D" << error3d;
    //fr << "perviewReconstructionError" << perViewReconstructionError;
    //fr << "perviewFProjectionError" << perViewFprojectionError;
    fr.release();
}

void StereoCameraCalibrator::findBoundingCorners(
    std::vector<cv::Point2f>& corners,
    std::vector<cv::Point>& crop,
    cv::Point2f& cm) {

    cm.x = cm.y = 0.0; float cnt = 0.0;
    for (auto& c : corners ) {
        cm.x += c.x; cm.y += c.y; cnt += 1.0;
    }
    if (cnt > 0) {
        cm.x /= cnt; cm.y /= cnt;
    }
    cv::Point2f c00, c01, c10, c11;
    float m00 = 10000, m01 = 0, m10 = 0, m11 = 0;
    for (auto &c : corners) {
        float m1 = c.x - c.y;
        float m2 = c.x + c.y;
        if (m2 < m00) { m00 = m2; c00 = c;}
        if (m2 > m11) { m11 = m2; c11 = c;}
        if (m1 > m01) { m01 = m1; c01 = c;}
        if (m1 < m10) { m10 = m1; c10 = c;}
    }
    crop.push_back(c00);
    crop.push_back(c01);
    crop.push_back(c11);
    crop.push_back(c10);
}


void StereoCameraCalibrator::saveTargetCorners(const std::string& fileName,
                            StereoCameraCalibrator::CameraCorners& leftCamera,
                            StereoCameraCalibrator::CameraCorners& rightCamera)
{
    json j;

    auto addCamera = [](json& camera, StereoCameraCalibrator::CameraCorners& corners){
        camera["top_left"] =
        {
            {"x", corners.topLeft.x},
            {"y", corners.topLeft.y}
        };

        camera["bottom_left"] =
        {
            {"x", corners.bottomLeft.x},
            {"y", corners.bottomLeft.y}
        };

        camera["top_right"] =
        {
            {"x", corners.topRight.x},
            {"y", corners.topRight.y}
        };

        camera["bottom_right"] =
        {
            {"x", corners.bottomRight.x},
            {"y", corners.bottomRight.y}
        };
    };

    addCamera(j["target_corners"]["leftCamera"], leftCamera);
    addCamera(j["target_corners"]["rightCamera"], rightCamera);

    std::ofstream file(fileName);
    file << j.dump(4);
}


