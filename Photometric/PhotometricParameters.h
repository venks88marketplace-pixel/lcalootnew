#pragma once
#include <QMetaType>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

struct PhotometricParameters {
    double cameraExposure;
    double leftCameraGain;
    double rightCameraGain;
    double gainIncrement;
    unsigned int topNir;
    unsigned int nirIncrement;
    unsigned int bottomNir;
    double zPosition_mm;
    double acceptanceLeftRightDiff;
    std::pair<int, int> acceptanceLeftRightPeakBracket;
    std::pair<double, double> cameraGainRange;

    static PhotometricParameters fromJson(const QJsonObject& obj){
        if (obj.isEmpty())
            throw std::runtime_error(
                "PhotometricParams is missing or empty");
        PhotometricParameters p;
        p.cameraExposure =
            obj.value("exposure").toDouble(p.cameraExposure);
        p.leftCameraGain =
            obj.value("gain_left_camera").toDouble(p.leftCameraGain);
        p.rightCameraGain =
            obj.value("gain_right_camera").toDouble(p.rightCameraGain);
        p.gainIncrement =
            obj.value("gain_increment").toDouble(p.gainIncrement);
        p.topNir =
            obj.value("nir_default").toInt(p.topNir);
        p.bottomNir =
            obj.value("nir_default").toInt(p.bottomNir);
        p.nirIncrement =
            obj.value("nir_increment").toInt(p.nirIncrement);
        p.zPosition_mm =
            obj.value("z_position_mm").toDouble(p.zPosition_mm);
        p.acceptanceLeftRightDiff =
            obj.value("acceptance_left_right_diff_prcnt").toDouble(p.acceptanceLeftRightDiff);

        auto rangeArray = obj.value("acceptance_criterion_peak_range").toArray();
        p.acceptanceLeftRightPeakBracket = std::pair<double,double>(rangeArray[0].toInt(),
                                                            rangeArray[1].toInt());
        auto gainRangeArray = obj.value("camera_gain_range_db").toArray();
        p.cameraGainRange = std::pair<int,int>(gainRangeArray[0].toDouble(),
                                                         gainRangeArray[1].toDouble());

        return p;
    }
};
Q_DECLARE_METATYPE(PhotometricParameters);