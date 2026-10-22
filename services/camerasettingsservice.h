#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <memory>
#include <interfaces/INetworkService.h>
#include <interfaces/ICameraSettingsService.h>
#include "neo-calibration-server-api/calibration_server_api.pb.h"

class CameraSettingsService : public ICameraSettingsService
{
    Q_OBJECT
    Q_PROPERTY(int leftGain READ leftGain WRITE setLeftGain NOTIFY leftGainChanged)
    Q_PROPERTY(int leftExposureUs READ leftExposureUs WRITE setLeftExposureUs NOTIFY leftExposureUsChanged)
    Q_PROPERTY(int rightGain READ rightGain WRITE setRightGain NOTIFY rightGainChanged)
    Q_PROPERTY(int rightExposureUs READ rightExposureUs WRITE setRightExposureUs NOTIFY rightExposureUsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit CameraSettingsService(std::shared_ptr<INetworkService> network, QObject* parent = nullptr);

    int leftGain() const override { return m_leftGain; }
    int leftExposureUs() const override { return m_leftExposureUs; }
    int rightGain() const override { return m_rightGain; }
    int rightExposureUs() const override { return m_rightExposureUs; }
    bool busy() const override { return m_busy; }

    void setLeftGain(int v) override;
    void setLeftExposureUs(int v) override;
    void setRightGain(int v) override;
    void setRightExposureUs(int v) override;

    Q_INVOKABLE void apply() override;
    void configure(const QJsonObject& settings) override;

public slots:
    void onNetworkMessageReceived(const QByteArray& data);

signals:
    void leftGainChanged();
    void leftExposureUsChanged();
    void rightGainChanged();
    void rightExposureUsChanged();
    void busyChanged();

    void applySucceeded(const QString& message);
    void applyFailed(const QString& message);

private:
    int clampNonNegative(int v) const;
    void setBusy(bool b);

private:
    std::shared_ptr<INetworkService> m_network;
    int m_leftGain{0};
    int m_leftExposureUs{0};
    int m_rightGain{0};
    int m_rightExposureUs{0};
    bool m_busy{false};
};