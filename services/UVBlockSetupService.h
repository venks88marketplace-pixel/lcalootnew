#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QTimer>
#include <memory>
#include <interfaces/INetworkService.h>
#include <interfaces/IUVSettingsService.h>

class UVBlockSetupService : public QObject
{
    Q_OBJECT

public:
    UVBlockSetupService(std::shared_ptr<INetworkService> network,
                        std::shared_ptr<IUVSettingsService> uvSettings,
                        QObject* parent = nullptr);

    // UV DAC on (same path as UVCalibrationProcessor), then camera exposure = 1 ms.
    Q_INVOKABLE void begin();

    // UV off (CancelUvDiagnostic via setEnabled(false)).
    Q_INVOKABLE void finish();

signals:
    void setupFinished(bool success, const QString& message);
    void finishSent(bool success, const QString& message);

private slots:
    void onNetworkMessageReceived(const QByteArray& data);
    void onTimeout();
    void onReapply();
    void onDeviceSettingsTimeout();

private:
    void sendExposure();
    void completeSetup(bool success, const QString& message);

    // Sends SetUvDeviceSettings message. Returns true if message was queued/sent.
    bool sendSetUvDeviceSettings(uint32_t pd1_gain_count,
                                 uint32_t pd2_gain_count = 0,
                                 uint32_t pd3_gain_count = 0,
                                 uint32_t pi_gain_count = 0);

    static constexpr int    UV_DAC_COUNTS       = 1100;
    static constexpr int    REAPPLY_MS          = 4 * 60 * 1000; // service sends 5 min per start
    static constexpr double CAMERA_EXPOSURE_US  = 1000.0;        // 1 ms
    static constexpr double CAMERA_GAIN         = 0.0;
    static constexpr int    RESPONSE_TIMEOUT_MS = 10000;

    std::shared_ptr<INetworkService> m_network;
    std::shared_ptr<IUVSettingsService> m_uv;
    QTimer m_timeout;
    QTimer m_reapply;

    // New timer / flag for waiting for device_settings reply
    QTimer m_deviceSettingsTimer;
    bool m_waitingDeviceSettings = false;

    bool m_waitingExposure = false;
    bool m_exposureQueued = false;
};