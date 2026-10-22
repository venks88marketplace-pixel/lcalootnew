#pragma once
#include <iomanip>
#include <opencv2/imgproc.hpp>
#include <string_view>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <opencv2/core.hpp>

namespace calibration {

    enum class EnumCameraSide{ LeftCam, RightCam };

    struct

        Parameter {
        bool trained = false;
        bool useRectification = true;
        cv::Mat K1, K2;
        cv::Mat D1, D2;
        cv::Mat Pex1, Pex2;
        cv::Mat rvec1, rvec2, tvec1, tvec2;
        cv::Mat R, T, E, F;
        cv::Mat R1, R2, P1, P2, Q;
    };

    inline constexpr std::string_view CamToString(EnumCameraSide camside) {
        switch (camside) {
        case EnumCameraSide::LeftCam:
            return "LeftCamera";
        case EnumCameraSide::RightCam:
            return "RightCamera";
        }
        return {};
    }

    inline cv::Mat QPixmapToGrayMat(const QPixmap& pixmap){
        QImage image = pixmap.toImage().convertToFormat(QImage::Format_Grayscale8);
        return cv::Mat(
                   image.height(),
                   image.width(),
                   CV_8UC1,
                   const_cast<uchar*>(image.bits()),
                   image.bytesPerLine()).clone();
    }

    inline std::string CurrentDateString() {
        auto p = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(p);
        std::tm tm{};
        localtime_s(&tm, &t);
        std::stringstream ss;
        ss << std::put_time(&tm,"%Y_%b_%d_%H_%M_%S");
        //std::cout << ss.str() << std::endl;
        return ss.str();
    }

    inline cv::Mat stretch(cv::Mat in){
        double minV = 0.0, maxV = 0.0;
        cv::minMaxLoc(in, &minV, &maxV);

        cv::Mat stretched;
        if (maxV > minV) {
            in.convertTo(stretched, CV_8U, 255.0 / (maxV - minV), -minV * 255.0 / (maxV - minV));
        } else {
            stretched = in.clone();
        }
        return stretched;
    }

    //json helpers
    // QJsonObject requireObject(const QJsonObject& parent,
    //                           const QString& key){
    //     auto it = parent.find(key);

    //     if (it == parent.end() || !it->isObject())
    //         throw std::runtime_error(
    //             QString("Missing object '%1'")
    //                 .arg(key).toStdString());

    //     return it->toObject();
    // }

    // QJsonArray requireArray(const QJsonObject& parent,
    //                                 const QString& key){
    //     auto it = parent.find(key);

    //     if (it == parent.end() || !it->isArray())
    //         throw std::runtime_error(
    //             QString("Missing array '%1'")
    //                 .arg(key).toStdString());

    //     return it->toArray();
    // }
    }
