//
// Created by amit on 9/22/20.
//

#pragma once

#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc.hpp"
#include "spatialCalData.h"
//#include <time.h>

//using namespace std;
using namespace neo::calibration;
namespace fs = std::filesystem;

class calibrationStorage {
public:
    NFOVCalibrationData nfovCal;
    WFOVCalibrationData wfovCal;
    photometricCalibrationData photoCal;
    DMDCalibrationData dmdCal;
    IRArcCalibration irArcCal;

    std::filesystem::path calDataRoot() const { return calibrationDataRoot; }
private:

    fs::path calibrationDataRoot = "C:\\calibration\\apps\\calibration_data";
    //fs::path calibrationDataRoot = "C:\\calibration\\apps\\";

    bool readOnly = false;
    std::string readOnlyFile = "";

    auto currentDateString() {
        auto p = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(p);
        //std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &t);
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%b-%d %H:%M:%S");
        //std::cout << ss.str() << std::endl;
        return ss.str();
    }

    int filename2version(std::string filename) {
        if (filename.length() < 13) return 0;
        auto version = filename.substr(filename.length() - 9, 4);
        return std::atoi(version.c_str());
    }

    std::string getLatestCalibrationFile() {
        if (readOnly == true) {
            std::cout << "Calibration is in read only mode: " << readOnlyFile << std::endl;
            return readOnlyFile;
        }
        std::string latestFileOnDisk("");
        int latestVersion = -1;
        if (fs::exists(calibrationDataRoot) == false) {
            std::cout << "Calibration calibrationDataRoot not found " << std::endl;
            fs::create_directories(calibrationDataRoot);
        }
        else {
            for (auto &p: fs::directory_iterator(calibrationDataRoot)) {
                if (p.is_regular_file() == false) continue;
                auto ext = p.path().extension().string();
                if (ext.compare(".json") == 0) {
                    auto filename = p.path().filename().string();
                    int ver = filename2version(filename);
                    //std::cout << "JSON: " << filename << " " << ver << std::endl;
                    if (ver > latestVersion) {
                        latestFileOnDisk = filename;
                        latestVersion = ver;
                    }
                }
            }
        }
//        if ( file_found  == false) {
//            latestFileOnDisk = "nxl_calibration_0000.json";
//        }
        std::cout << "Reading Calibration File : " << latestFileOnDisk << std::endl;
        calFileOnDiskUsed = latestFileOnDisk;
        return latestFileOnDisk;
    }

    fs::path nextCalibrationFileName() {
        std::string filename = getLatestCalibrationFile();
        fs::path filepath;
        //std::cout << "Output from file on Disk: " << filename << std::endl;
        if (filename.empty() == true) {
            filepath = calibrationDataRoot / "nxl_calibration_0000.json";
        }
        else {
            auto ver = std::to_string( filename2version(filename)+ 1);
            while(ver.length() < 4) { ver = "0" + ver; }
            filepath = calibrationDataRoot / fs::path("nxl_calibration_" + ver + ".json");
        }
        std::cout << "New FileOnDisk: " << fs::current_path().string() <<"/"<< filepath.string() << std::endl;
        return filepath;
    }


    void write() {
        if (readOnly == true) {
            std::cout << "Calibration is in read only mode: " << readOnlyFile << std::endl;
            return;
        }
        auto nextfile = nextCalibrationFileName();

        auto dateStr = currentDateString();
        cv::FileStorage fs(nextfile.string(), cv::FileStorage::FORMAT_JSON | cv::FileStorage::WRITE);
        //CV_WRAP FileStorage(const String & filename, int flags, const String & encoding = String());
        if (fs.isOpened()) {
            //nfov
            fs.startWriteStruct("NFOVCalibrationData",cv::FileNode::MAP,cv::String());
            fs.write("DateString",nfovCal.timestr);
            fs.write("LeftDistortion",nfovCal.left.distortion);
            fs.write("LeftIntrinsic",nfovCal.left.intrinsics);
            fs.write("LeftProjectionMatrix",nfovCal.left.projectionMatrix);
            fs.write("LeftROI_X",nfovCal.leftOffset.roiOffsetX);
            fs.write("LeftROI_Y",nfovCal.leftOffset.roiOffsetY);
            fs.write("RightDistortion",nfovCal.right.distortion);
            fs.write("RightIntrinsic",nfovCal.right.intrinsics);
            fs.write("RightProjectionMatrix",nfovCal.right.projectionMatrix);
            fs.write("RightROI_X",nfovCal.rightOffset.roiOffsetX);
            fs.write("RightROI_Y",nfovCal.rightOffset.roiOffsetY);
            fs.write("FundamentalMatrix",nfovCal.F);
            fs.write("EssentialMatrix",nfovCal.E);
            fs.write("StereoRotation",nfovCal.R);
            fs.write("StereoTranslation",nfovCal.T);
            fs.write("RectifyLeft",nfovCal.R1);
            fs.write("RectifyRight",nfovCal.R2);
            fs.write("RectifiedProjectionLeft",nfovCal.P1);
            fs.write("RectifiedProjectionRight",nfovCal.P2);
            fs.write("DisparityToZMatrix",nfovCal.Q);
            fs.write("TransformToInstrumentAxis", nfovCal.toIA);
            fs.endWriteStruct();

            //wfov
            fs.startWriteStruct("WFOVCalibrationData",cv::FileNode::MAP,cv::String());
            fs.write("DateString",wfovCal.timestr);
            fs.write("LabCenterX",wfovCal.labCenter.x);
            fs.write("LabCenterY",wfovCal.labCenter.y);
            fs.write("LabCenterZ",wfovCal.labCenter.z);
            fs.endWriteStruct();
            //Photometric
            fs.startWriteStruct("PhotometricCalibration",cv::FileNode::MAP,cv::String());
            fs.write("DateString",photoCal.timestr);
            fs.write("Exposure",(int)photoCal.exposure);
            fs.write("LeftGain",(double)photoCal.leftCameraGain);
            fs.write("RightGain",(double)photoCal.rightCameraGain);
            fs.endWriteStruct();
            //UVCalib
            fs.startWriteStruct("DMDCalibrationData",cv::FileNode::MAP,cv::String());
            fs.write("DateString", dmdCal.timestr);
            fs.write("gK", dmdCal.gK);
            fs.write("gD", dmdCal.gD);
            fs.write("gR", dmdCal.gR);
            fs.write("gT", dmdCal.gT);
            fs.endWriteStruct();
            //IRCalibration
            fs.startWriteStruct("IRCalibrationData",cv::FileNode::MAP,cv::String());
            fs.write("DateString", irArcCal.timestr);
            fs.write("IRTop", irArcCal.IRTop);
            fs.write("IRBottom", irArcCal.IRBot);
            fs.endWriteStruct();
            fs.release();
            std::cout << "Written on disk " << nextfile << std::endl;
        }
        else {
            std::cout << "Failed to update " << nextfile << std::endl;
        }

    }
    void read() {
        //std::cout << "Inside reading " << std::endl;
        fs::path filepath;
        auto filename = getLatestCalibrationFile();
        if (filename.empty() == true) {
            std::cout << "Calibration file not found " << filename << std::endl;
            return;
        }
        try {
            if (readOnly == false) {
                filepath = calibrationDataRoot / filename;
            }
            else {
                filepath = filename;
            }
            //std::cout << "R1 before read: " << nfovCal.R1 << std::endl;
            cv::FileStorage fs(filepath.string(), cv::FileStorage::FORMAT_JSON | cv::FileStorage::READ);
            auto nfov = fs["NFOVCalibrationData"];
            nfov["DateString"] >> nfovCal.timestr;
            nfov["LeftDistortion"] >> nfovCal.left.distortion;
            nfov["LeftIntrinsic"] >> nfovCal.left.intrinsics;
            nfov["LeftProjectionMatrix"] >> nfovCal.left.projectionMatrix;
            nfov["RightProjectionMatrix"] >> nfovCal.right.projectionMatrix;
            nfov["LeftROI_X"] >> nfovCal.leftOffset.roiOffsetX;
            nfov["LeftROI_Y"] >> nfovCal.leftOffset.roiOffsetY;
            nfov["RightDistortion"] >> nfovCal.right.distortion;
            nfov["RightIntrinsic"] >> nfovCal.right.intrinsics;
            nfov["RightROI_X"] >> nfovCal.rightOffset.roiOffsetX;
            nfov["RightROI_Y"] >> nfovCal.rightOffset.roiOffsetY;
            nfov["FundamentalMatrix"] >> nfovCal.F;
            nfov["EssentialMatrix"] >> nfovCal.E;
            nfov["StereoRotation"] >> nfovCal.R;
            nfov["StereoTranslation"] >> nfovCal.T;
            nfov["RectifyLeft"] >> nfovCal.R1;
            nfov["RectifyRight"] >> nfovCal.R2;
            nfov["RectifiedProjectionLeft"] >> nfovCal.P1;
            nfov["RectifiedProjectionRight"] >> nfovCal.P2;
            nfov["DisparityToZMatrix"] >> nfovCal.Q;
            nfov["TransformToInstrumentAxis"] >> nfovCal.toIA;


            auto wfov = fs["WFOVCalibrationData"];
            wfov["DateString"] >> wfovCal.timestr;
            wfov["LabCenterX"] >> wfovCal.labCenter.x;
            wfov["LabCenterY"] >> wfovCal.labCenter.y;
            wfov["LabCenterZ"] >> wfovCal.labCenter.z;

            auto photo = fs["PhotometricCalibration"];
            photo["DateString"] >> photoCal.timestr;
            photo["Exposure"] >> photoCal.exposure;
            photo["LeftGain"] >> photoCal.leftCameraGain;
            photo["RightGain"] >> photoCal.rightCameraGain;

            auto dmd = fs["DMDCalibrationData"];
            dmd["DateString"] >> dmdCal.timestr;
            dmd["gK"] >> dmdCal.gK;
            dmd["gD"] >> dmdCal.gD;
            dmd["gR"] >> dmdCal.gR;
            dmd["gT"] >> dmdCal.gT;

            auto ir = fs["IRCalibrationData"];
            ir["DateString"] >> irArcCal.timestr;
            ir["IRTop"] >> irArcCal.IRTop;
            ir["IRBottom"] >> irArcCal.IRBot;
            fs.release();
        }
        catch (std::exception &e) {
            std::cout << "Exception in reading json file: " << filepath <<std::endl;
            std::cout << e.what() << std::endl;
        }
    }

public:
    calibrationStorage () {
        if (fs::exists(calibrationDataRoot) == false) {
            fs::create_directories(calibrationDataRoot);
        }
        readOnly = false;
        readOnlyFile = "";
        //calibrationDataRoot = fs::canonical(absPath);
        //getLatestCalibrationFile();
    }

    calibrationStorage (std::string calFile) {
        readOnly = true;
        readOnlyFile = calFile;
    }

    void updateNfovStereoRectify(const cv::Mat& R1, const cv::Mat& P1,
                          const cv::Mat& R2, const cv::Mat& P2,
                          const cv::Mat& Q ) {
        read();
        nfovCal.R1 = R1;
        nfovCal.R2 = R2;
        nfovCal.P1 = P1;
        nfovCal.P2 = P2;
        nfovCal.Q = Q;
        write();
    }

    void updateNfovStereo(const cv::Mat& leftIntrinsic, const cv::Mat& leftDistortion,
                          const cv::Mat& rightIntrinsic, const cv::Mat& rightDistortion,
                          const cv::Mat& fundamental, const cv::Mat& essential,
                          const cv::Mat& R, const cv::Mat& T,
                          const cv::Mat& R1, const cv::Mat& P1,
                          const cv::Mat& R2, const cv::Mat& P2,
                          const cv::Mat& Q ) {
        read();
        nfovCal.timestr = currentDateString();
        nfovCal.left.distortion = leftDistortion;
        nfovCal.left.intrinsics = leftIntrinsic;
        nfovCal.right.distortion = rightDistortion;
        nfovCal.right.intrinsics = rightIntrinsic;
        nfovCal.F = fundamental;
        nfovCal.E = essential;
        nfovCal.R = R;
        nfovCal.T = T;
        nfovCal.R1 = R1;
        nfovCal.R2 = R2;
        nfovCal.P1 = P1;
        nfovCal.P2 = P2;
        nfovCal.Q = Q;
        write();
    }

    void updateNfovProjectionMatrices (const cv::Mat& leftProjection, const cv::Mat& rightProjection) {
        read();
        nfovCal.left.projectionMatrix = leftProjection;
        nfovCal.right.projectionMatrix = rightProjection;
        write();
    }

    void updateNfovOffsets(const int leftOffsetX, const int leftOffsetY, const int rightOffsetX, const int rightOffsetY) {
        read();
        nfovCal.timestr = currentDateString();
        nfovCal.leftOffset.roiOffsetX = leftOffsetX;
        nfovCal.leftOffset.roiOffsetY = leftOffsetY;
        nfovCal.rightOffset.roiOffsetX = rightOffsetX;
        nfovCal.rightOffset.roiOffsetY = rightOffsetY;
        write();
    }

    void updateInstrumentAxis(const cv::Mat& xform, double x, double y, double z) {
        read();
        wfovCal.timestr = currentDateString();
        nfovCal.toIA = xform;
        wfovCal.labCenter.x = x;
        wfovCal.labCenter.y = y;
        wfovCal.labCenter.z = z;
        write();
    }

    void updatePhtometricExposure(int exposure) {
        read();
        photoCal.timestr  = currentDateString();
        photoCal.exposure = exposure;
        write();
    }

    void updatePhtometricGain(float leftGain, float rightGain, int exposure) {
        read();
        photoCal.timestr  = currentDateString();
        photoCal.leftCameraGain = leftGain;
        photoCal.rightCameraGain = rightGain;
        photoCal.exposure = exposure;
        write();
    }

    void updateDMDParameters(const cv::Mat& gK, const cv::Mat& gD,
                               const cv::Mat& gR, const cv::Mat& gT) {
        read();
        dmdCal.timestr = currentDateString();
        dmdCal.gK = gK;
        dmdCal.gD = gD;
        dmdCal.gR = gR;
        dmdCal.gT = gT;
        write();
    }

    void updateIRParameters(const int& irTop, const int& irBot) {
        read();
        irArcCal.timestr = currentDateString();
        irArcCal.IRTop = irTop;
        irArcCal.IRBot = irBot;
        write();
    }

    bool getNfovCameraMatrices(cv::Mat& leftDistortion, cv::Mat& leftIntrinsic,
                               cv::Mat& rightDistortion, cv::Mat& rightIntrinsic ) {
        fs::path filepath;
        auto filename = getLatestCalibrationFile();
        if (filename.empty() == true) {
            std::cout << "Calibration file not found " << filename << std::endl;
            return false;
        }
        if (readOnly == false) {
            filepath = calibrationDataRoot / filename;
        }
        else {
            filepath = filename;
        }
        cv::FileStorage fs(filepath.string(), cv::FileStorage::FORMAT_JSON |  cv::FileStorage::READ);
        if (fs.isOpened() == false) return false;
        auto nfov = fs["NFOVCalibrationData"];
        nfov["LeftDistortion"] >> leftDistortion;
        nfov["LeftIntrinsic"] >> leftIntrinsic;
        nfov["RightDistortion"] >> rightDistortion;
        nfov["RightIntrinsic"] >> rightIntrinsic;
        fs.release();
        return true;
    }

    bool getProjectionMatrix(cv::Mat& L, cv::Mat& R) {
        fs::path filepath;
        auto filename = getLatestCalibrationFile();
        if (filename.empty() == true) {
            std::cout << "Calibration file not found " << filename << std::endl;
            return false;
        }
        if (readOnly == false) {
            filepath = calibrationDataRoot / filename;
        }
        else {
            filepath = filename;
        }
        cv::FileStorage fs(filepath.string(), cv::FileStorage::FORMAT_JSON |  cv::FileStorage::READ);
        if (fs.isOpened() == false) return false;
        auto nfov = fs["NFOVCalibrationData"];
        nfov["LeftProjectionMatrix"] >> L;
        nfov["RightProjectionMatrix"] >> R;
        fs.release();
        return true;
    }

//    bool getRightTranformMatrix(cv::Mat& R, cv::Mat& T) {
//        if (getLatestCalibrationFile() == false) { return false;}
//        cv::FileStorage fs(latestFileOnDisk, cv::FileStorage::FORMAT_JSON |  cv::FileStorage::READ);
//        if (fs.isOpened() == false) return false;
//        auto nfov = fs["NFOVCalibrationData"];
//
//        fs.release();
//        return true;
//    }


    bool getStereoMatrix(cv::Mat& F, cv::Mat& E, cv::Mat& R, cv::Mat& T,
                         cv::Mat& R1, cv::Mat& R2, cv::Mat& P1, cv::Mat& P2 , cv::Mat& Q) {
        fs::path filepath;
        auto filename = getLatestCalibrationFile();
        if (filename.empty() == true) {
            std::cout << "Calibration file not found " << filename << std::endl;
            return false;
        }
        if (readOnly == false) {
            filepath = calibrationDataRoot / filename;
        }
        else {
            filepath = filename;
        }
        cv::FileStorage fs(filepath.string(), cv::FileStorage::FORMAT_JSON |  cv::FileStorage::READ);
        if (fs.isOpened() == false) return false;
        auto nfov = fs["NFOVCalibrationData"];
        nfov["FundamentalMatrix"] >> F;
        nfov["EssentialMatrix"] >> E;
        nfov["StereoRotation"] >> R;
        nfov["StereoTranslation"] >> T;
        nfov["RectifyLeft"] >> R1;
        nfov["RectifyRight"] >> R2;
        nfov["RectifiedProjectionLeft"] >> P1;
        nfov["RectifiedProjectionRight"] >> P2;
        nfov["DisparityToZMatrix"] >> Q;
        fs.release();
        return true;
    }

    bool getOffsets(int& lx, int& ly, int& rx, int& ry) {
        fs::path filepath;
        auto filename = getLatestCalibrationFile();
        if (filename.empty() == true) {
            std::cout << "Calibration file not found " << filename << std::endl;
            return false;
        }
        if (readOnly == false) {
            filepath = calibrationDataRoot / filename;
        }
        else {
            filepath = filename;
        }
        cv::FileStorage fs(filepath.string(), cv::FileStorage::FORMAT_JSON |  cv::FileStorage::READ);
        if (fs.isOpened() == false) return false;
        auto nfov = fs["NFOVCalibrationData"];
        nfov["LeftROI_X"] >> lx;
        nfov["LeftROI_Y"] >> ly;
        nfov["RightROI_X"] >> rx;
        nfov["RightROI_Y"] >> ry;
        fs.release();
        return true;
    }

    bool getInstrumentAxis(cv::Mat& xform, double&x, double&y, double&z) {
        fs::path filepath;
        auto filename = getLatestCalibrationFile();
        if (filename.empty() == true) {
            std::cout << "Calibration file not found " << filename << std::endl;
            return false;
        }
        if (readOnly == false) {
            filepath = calibrationDataRoot / filename;
        }
        else {
            filepath = filename;
        }
        cv::FileStorage fs(filepath.string(), cv::FileStorage::FORMAT_JSON |  cv::FileStorage::READ);
        if (fs.isOpened() == false) return false;
        auto nfov = fs["NFOVCalibrationData"];
        nfov["TransformToInstrumentAxis"] >> xform;
        auto wfov = fs["WFOVCalibrationData"];
        wfov["LabCenterX"] >> x;
        wfov["LabCenterY"] >> y;
        wfov["LabCenterZ"] >> z;
        fs.release();
        return true;
    }

    bool getPhotometricParameters(int& exposure, double &leftGain, double&rightGain ) {
        fs::path filepath;
        auto filename = getLatestCalibrationFile();
        if (filename.empty() == true) {
            std::cout << "Calibration file not found " << filename << std::endl;
            return false;
        }
        if (readOnly == false) {
            filepath = calibrationDataRoot / filename;
        }
        else {
            filepath = filename;
        }
        cv::FileStorage fs(filepath.string(), cv::FileStorage::FORMAT_JSON |  cv::FileStorage::READ);
        std::cout << "reading file " << filepath << std::endl;
        if (fs.isOpened() == false) return false;
        auto photo = fs["PhotometricCalibration"];
        photo["Exposure"] >> exposure;
        photo["LeftGain"] >> leftGain;
        photo["RightGain"] >> rightGain;
        fs.release();
        return true;
    }


    bool getDMDParameters(cv::Mat& gK, cv::Mat& gD, cv::Mat& gR, cv::Mat& gT) {
        fs::path filepath;
        auto filename = getLatestCalibrationFile();
        if (filename.empty() == true) {
            std::cout << "Calibration file not found " << filename << std::endl;
            return false;
        }
        if (readOnly == false) {
            filepath = calibrationDataRoot / filename;
        }
        else {
            filepath = filename;
        }
        cv::FileStorage fs(filepath.string(), cv::FileStorage::FORMAT_JSON |  cv::FileStorage::READ);
        std::cout << "reading file " << filepath << std::endl;
        if (fs.isOpened() == false) return false;
        auto dmd = fs["DMDCalibrationData"];
        if (dmd.empty()) return false;
        dmd["gK"] >> gK;
        dmd["gD"] >> gD;
        dmd["gR"] >> gR;
        dmd["gT"] >> gT;
        fs.release();
        return true;
    }


    bool getIRArcParameters(int& irTop, int& irBot) {
        fs::path filepath;
        auto filename = getLatestCalibrationFile();
        if (filename.empty() == true) {
            std::cout << "Calibration file not found " << filename << std::endl;
            return false;
        }
        if (readOnly == false) {
            filepath = calibrationDataRoot / filename;
        }
        else {
            filepath = filename;
        }
        cv::FileStorage fs(filepath.string(), cv::FileStorage::FORMAT_JSON |  cv::FileStorage::READ);
        std::cout << "reading file " << filepath << std::endl;
        if (fs.isOpened() == false) return false;
        auto ir = fs["IRCalibrationData"];
        ir["IRTop"] >> irTop;
        ir["IRBottom"] >> irBot;
        fs.release();
        return true;
    }
    std::string calFileOnDiskUsed;
};

