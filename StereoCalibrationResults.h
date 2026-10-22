#pragma once
#include <QMetaType>
#include <QFile>
#include <QTextStream>

struct StereoCalibrationResults {
    std::map<std::string, double> results;
    bool saveToFile(const QString& fileName) const
    {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        QTextStream out(&file);
        int index = 0;
        for(const auto& [key, value] : results){
            out << key.c_str() << " = "
                << value << '\n';
        }
        return true;
    }
};
Q_DECLARE_METATYPE(StereoCalibrationResults);
