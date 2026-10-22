#pragma once
#include <QMetaType>
#include <QFile>
#include <QTextStream>

struct PhotometricResults {
    unsigned int peakLeftTop;
    unsigned int peakLeftBottom;
    unsigned int peakRightTop;
    unsigned int peakRightBottom;
    double topBalance;
    double bottomBalance;
    double leftBalance;
    double rightBalance;
    bool passed;

    bool saveToFile(const QString& fileName) const
    {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        QTextStream out(&file);
        out << "pickLeftTop="
            << peakLeftTop << '\n';
        out << "pickLeftBottom="
            << peakLeftBottom << '\n';
        out << "pickRightTop="
            << peakRightTop << '\n';
        out << "pickRightBottom="
            << peakRightBottom << '\n';
        out << "topBalance="
            << topBalance << '\n';
        out << "bottomBalance="
            << bottomBalance << '\n';
        out << "leftBalance="
            << leftBalance << '\n';
        out << "rightBalance="
            << rightBalance << '\n';
        out << "passed="
            << passed << '\n';
        return true;
    }
};
Q_DECLARE_METATYPE(PhotometricResults);