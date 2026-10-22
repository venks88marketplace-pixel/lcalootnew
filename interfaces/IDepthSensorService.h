#pragma once

#include <QObject>
#include <QJsonObject>
#include "interfaces/IService.h"

class IDepthSensorService : public IService
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString outputFilePath READ outputFilePath NOTIFY outputFilePathChanged)

public:
    explicit IDepthSensorService(QObject* parent = nullptr) : IService(parent) {}
    ~IDepthSensorService() override = default;

    virtual bool busy() const = 0;
    virtual double progress() const = 0;          // 0.0 to 1.0
    virtual QString statusText() const = 0;
    virtual QString outputFilePath() const = 0;

    Q_INVOKABLE virtual void startCalibrationRun() = 0;

signals:
    void busyChanged();
    void progressChanged();
    void statusTextChanged();
    void outputFilePathChanged();
    void runCompleted(bool success, const QString& message);
};