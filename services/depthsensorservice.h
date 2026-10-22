#pragma once

#include <QObject>
#include <QJsonObject>
#include <QString>
#include <memory>

#include "interfaces/IDepthSensorService.h"
#include "interfaces/INetworkService.h"
#include "interfaces/IStepperControllerService.h"

class DepthSensorService : public IDepthSensorService
{
    Q_OBJECT
public:
    explicit DepthSensorService(std::shared_ptr<INetworkService> network,
                                std::shared_ptr<IStepperControllerService> stepper,
                                QObject* parent = nullptr);

    bool busy() const override { return m_busy; }
    double progress() const override { return m_progress; }
    QString statusText() const override { return m_statusText; }
    QString outputFilePath() const override { return m_outputFilePath; }

    void configure(const QJsonObject& settings) override;
    Q_INVOKABLE void startCalibrationRun() override;

private:
    void setBusy(bool b);
    void setProgress(double p);
    void setStatusText(const QString& s);
    void setOutputFilePath(const QString& p);
    double rawToMm(double raw) const;

private:
    std::shared_ptr<INetworkService> m_network;
    std::shared_ptr<IStepperControllerService> m_stepper;

    bool m_busy{false};
    double m_progress{0.0};
    QString m_statusText{""};   // no default "Status: Idle"
    QString m_outputFilePath;
};