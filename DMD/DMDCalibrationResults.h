#pragma once
#include <QMetaType>
#include <QFile>
#include <QTextStream>

struct DMDCalibrationResults {
    bool uvDotsFound;
    QString dmdCalibrationStatus;

    bool saveToFile(const QString& fileName) const
    {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        QTextStream out(&file);
        out << "DMD Calibration Status="
            << dmdCalibrationStatus << '\n';
        return true;
    }
};
Q_DECLARE_METATYPE(DMDCalibrationResults);
