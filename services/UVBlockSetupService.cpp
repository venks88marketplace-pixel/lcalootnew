#include "UVBlockSetupService.h"

#include <QDebug>
#include "neo-calibration-server-api/calibration_server_api.pb.h"

UVBlockSetupService::UVBlockSetupService(std::shared_ptr<INetworkService> network,
                                         std::shared_ptr<IUVSettingsService> uvSettings,
                                         QObject* parent)
    : QObject(parent), m_network(std::move(network)), m_uv(std::move(uvSettings))
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(RESPONSE_TIMEOUT_MS);
    connect(&m_timeout, &QTimer::timeout, this, &UVBlockSetupService::onTimeout);

    m_reapply.setInterval(REAPPLY_MS);
    connect(&m_reapply, &QTimer::timeout, this, &UVBlockSetupService::onReapply);

    m_deviceSettingsTimer.setSingleShot(true);
    m_deviceSettingsTimer.setInterval(RESPONSE_TIMEOUT_MS);
    connect(&m_deviceSettingsTimer, &QTimer::timeout, this, &UVBlockSetupService::onDeviceSettingsTimeout);

    if (m_network) {
        connect(m_network.get(), &INetworkService::messageReceived,
                this, &UVBlockSetupService::onNetworkMessageReceived);
    }
}

bool UVBlockSetupService::sendSetUvDeviceSettings(uint32_t pd1_gain_count,
                                                  uint32_t pd2_gain_count,
                                                  uint32_t pd3_gain_count,
                                                  uint32_t pi_gain_count)
{
    if (!m_network || !m_network->isConnected()) {
        qWarning() << "[UVBlockSetup] Network not connected while setting UV device settings";
        return false;
    }

    CalibrationApi::ToServer msg;
    auto* req = msg.mutable_set_uv_device_settings();
    req->set_pd1_gain_count(pd1_gain_count);
    req->set_pd2_gain_count(pd2_gain_count);
    req->set_pd3_gain_count(pd3_gain_count);
    req->set_pi_gain_count(pi_gain_count);

    QByteArray data(static_cast<int>(msg.ByteSizeLong()), '\0');
    if (!msg.SerializeToArray(data.data(), data.size())) {
        qWarning() << "[UVBlockSetup] Failed to serialize SetUvDeviceSettings";
        return false;
    }

    m_network->sendMessage(data);

    qInfo() << "[UVBlockSetup] Sent SetUvDeviceSettings"
            << "pd1=" << pd1_gain_count
            << "pd2=" << pd2_gain_count
            << "pd3=" << pd3_gain_count
            << "pi=" << pi_gain_count;
    return true;
}

void UVBlockSetupService::begin()
{
    if (!m_network || !m_network->isConnected() || !m_uv) {
        qWarning() << "[UVBlockSetup] Network or UV service not available";
        completeSetup(false, "Network not connected");
        return;
    }

    // 1) Request device settings update (PD1 = 1000) and wait for a readback.
    if (!sendSetUvDeviceSettings(1000, 0, 0, 0)) {
        completeSetup(false, "Failed to send SetUvDeviceSettings");
        return;
    }

    // set a flag and start a short timer — wait for FromServer.device_settings
    m_waitingDeviceSettings = true;
    m_deviceSettingsTimer.start();
    qInfo() << "[UVBlockSetup] Waiting for device_settings reply before starting UV";
}

void UVBlockSetupService::sendExposure()
{
    CalibrationApi::ToServer msg;
    auto* req = msg.mutable_set_exposure_settings();
    for (auto cam : {CalibrationApi::LEFT, CalibrationApi::RIGHT}) {
        auto* s = req->add_settings();
        s->set_camera(cam);
        s->set_gain(CAMERA_GAIN);
        s->set_exposure_us(CAMERA_EXPOSURE_US);
    }
    QByteArray data(static_cast<int>(msg.ByteSizeLong()), '\0');
    if (!msg.SerializeToArray(data.data(), data.size())) {
        completeSetup(false, "Failed to serialize exposure request");
        return;
    }
    m_network->sendMessage(data);
    qInfo() << "[UVBlockSetup] Sent SetExposureSettings exposure_us =" << CAMERA_EXPOSURE_US;

    m_waitingExposure = true;
    m_timeout.start();
}

void UVBlockSetupService::onReapply()
{
    if (!m_uv || !m_network || !m_network->isConnected()) return;
    qInfo() << "[UVBlockSetup] Re-applying UV dac" << UV_DAC_COUNTS;
    m_uv->setPower(UV_DAC_COUNTS);
    m_uv->setEnabled(true);
}

void UVBlockSetupService::finish()
{
    m_reapply.stop();
    m_timeout.stop();
    m_deviceSettingsTimer.stop();
    m_waitingExposure = false;
    m_waitingDeviceSettings = false;

    if (!m_uv || !m_network || !m_network->isConnected()) {
        qWarning() << "[UVBlockSetup] Network not connected, cannot turn UV off";
        emit finishSent(false, "Network not connected");
        return;
    }

    m_uv->setEnabled(false);   // CancelUvDiagnostic, same as UVCalibrationProcessor
    qInfo() << "[UVBlockSetup] UV off (cancel sent)";
    emit finishSent(true, "UV off");
}

void UVBlockSetupService::onNetworkMessageReceived(const QByteArray& data)
{
    CalibrationApi::FromServer fromServer;
    if (!fromServer.ParseFromArray(data.constData(), data.size())) return;

    if (fromServer.has_uv_diagnostic_response()) {
        const auto& r = fromServer.uv_diagnostic_response();
        const QString text = QString::fromStdString(r.message());
        if (r.success()) qInfo() << "[UVBlockSetup] UvDiagnosticResponse success:" << text;
        else             qWarning() << "[UVBlockSetup] UvDiagnosticResponse FAILED:" << text;

        // If exposure was queued after UV start, trigger it here (keeps original behavior).
        if (m_exposureQueued) {
            m_exposureQueued = false;
            m_timeout.stop();
            QTimer::singleShot(1500, this, [this]() { sendExposure(); });
        }
        return;
    }

    if (fromServer.has_exposure_response() && m_waitingExposure) {
        const bool failed = fromServer.exposure_response().failures();
        completeSetup(!failed, failed ? "Exposure settings apply failed" : "Setup complete");
        return;
    }

    if (fromServer.has_device_settings()) {
        const auto& ds = fromServer.device_settings();
        qInfo() << "[UVBlockSetup] DeviceSettings reply:"
                << "pd1=" << ds.pd1_gain_count()
                << "pd2=" << ds.pd2_gain_count()
                << "pd3=" << ds.pd3_gain_count()
                << "pi=" << ds.pi_gain_count();

        if (m_waitingDeviceSettings) {
            m_waitingDeviceSettings = false;
            m_deviceSettingsTimer.stop();

            // Optionally verify we got the pd1 we requested, else warn
            if (ds.pd1_gain_count() != 1000) {
                qWarning() << "[UVBlockSetup] PD1 readback mismatch: expected 1000 got" << ds.pd1_gain_count();
                // We choose to proceed anyway (fallback) but this is logged for diagnosis.
            }

            // Now that device settings are applied (or at least we've been replied to), start the UV diagnostic
            m_uv->setPower(UV_DAC_COUNTS);
            m_uv->setEnabled(true);
            m_uv->apply();
            qInfo() << "[UVBlockSetup] UV on (setPower, setEnabled(true), apply), dac =" << UV_DAC_COUNTS;
            if (REAPPLY_MS > 0) m_reapply.start();

            // Send exposure only after the UV reply, plus a short delay (same behavior as before)
            m_exposureQueued = true;
            m_timeout.start();
        }
        return;
    }
}

void UVBlockSetupService::onDeviceSettingsTimeout()
{
    if (!m_waitingDeviceSettings) return;
    m_waitingDeviceSettings = false;
    qWarning() << "[UVBlockSetup] No device_settings reply; proceeding to start UV as fallback";

    // Proceed with UV start even if device settings not confirmed
    m_uv->setPower(UV_DAC_COUNTS);
    m_uv->setEnabled(true);
    m_uv->apply();
    qInfo() << "[UVBlockSetup] UV on (fallback start), dac =" << UV_DAC_COUNTS;
    if (REAPPLY_MS > 0) m_reapply.start();

    m_exposureQueued = true;
    m_timeout.start();
}

void UVBlockSetupService::onTimeout()
{
    if (m_exposureQueued) {
        m_exposureQueued = false;
        qWarning() << "[UVBlockSetup] No UV reply, sending exposure anyway";
        sendExposure();
        return;
    }
    qWarning() << "[UVBlockSetup] Timed out waiting for exposure response";
    completeSetup(false, "Timed out waiting for exposure response");
}

void UVBlockSetupService::completeSetup(bool success, const QString& message)
{
    m_waitingExposure = false;
    m_timeout.stop();
    if (success) qInfo() << "[UVBlockSetup]" << message;
    else         qWarning() << "[UVBlockSetup]" << message;
    emit setupFinished(success, message);
}