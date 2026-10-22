#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <QString>
#include <QDebug>
#include <QObject>

#ifdef _WIN32
#include <oaidl.h>
#endif

class OphirMeterService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double currentReading READ getLastReading NOTIFY readingChanged)

public:
    using ReadingCallback = std::function<void(double irradiance_mw_per_cm2)>;

    OphirMeterService();
    ~OphirMeterService();

    bool connect();
    void disconnect();
    bool isConnected() const;

    double readIrradiance();

    void startStreaming(ReadingCallback callback, int interval_ms = 100);
    void stopStreaming();
    bool isStreaming() const;

    double getLastReading() const;
    std::string getDeviceInfo() const;
    std::string getLastError() const;

    // Sensor configuration
    void setSensorArea(double area_cm2) { sensorArea_ = area_cm2; }
    double getSensorArea() const { return sensorArea_; }

signals:
    void readingChanged(double newReading);

private:
    void streamingThread(ReadingCallback callback, int interval_ms);
    double convertPowerToIrradiance(double power_mw);
    bool retrieveSensorArea();

#ifdef _WIN32
    IDispatch* disp_ = nullptr;
#endif

    mutable std::mutex readingMutex_;
    mutable std::mutex errorMutex_;

    double lastReading_;
    double sensorArea_;  // in cm²
    std::string lastError_;
    std::string deviceInfo_;

    std::atomic<bool> connected_;
    std::atomic<bool> streaming_;
    std::thread streamThread_;

    long deviceHandle_;
    QString deviceSerial_;
};