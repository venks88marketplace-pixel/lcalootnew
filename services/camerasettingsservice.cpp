#include "camerasettingsservice.h"
#include <QDebug>

CameraSettingsService::CameraSettingsService(std::shared_ptr<INetworkService> network, QObject* parent)
    : ICameraSettingsService(parent), m_network(std::move(network))
{
    if (m_network) {
        connect(m_network.get(), &INetworkService::messageReceived,
                this, &CameraSettingsService::onNetworkMessageReceived);
    }
}

void CameraSettingsService::configure(const QJsonObject& settings)
{
    Q_UNUSED(settings);
}

int CameraSettingsService::clampNonNegative(int v) const { return v < 0 ? 0 : v; }

void CameraSettingsService::setBusy(bool b)
{
    if (m_busy == b) return;
    m_busy = b;
    emit busyChanged();
}

void CameraSettingsService::setLeftGain(int v)
{
    v = clampNonNegative(v);
    if (m_leftGain == v) return;
    m_leftGain = v;
    emit leftGainChanged();
}

void CameraSettingsService::setLeftExposureUs(int v)
{
    v = clampNonNegative(v);
    if (m_leftExposureUs == v) return;
    m_leftExposureUs = v;
    emit leftExposureUsChanged();
}

void CameraSettingsService::setRightGain(int v)
{
    v = clampNonNegative(v);
    if (m_rightGain == v) return;
    m_rightGain = v;
    emit rightGainChanged();
}

void CameraSettingsService::setRightExposureUs(int v)
{
    v = clampNonNegative(v);
    if (m_rightExposureUs == v) return;
    m_rightExposureUs = v;
    emit rightExposureUsChanged();
}

void CameraSettingsService::apply()
{
    if (!m_network || !m_network->isConnected()) {
        emit applyFailed("Network not connected");
        return;
    }

    CalibrationApi::ToServer toServer;
    auto* req = toServer.mutable_set_exposure_settings();

    // LEFT
    {
        auto* s = req->add_settings();
        s->set_camera(CalibrationApi::LEFT);
        s->set_gain(static_cast<double>(m_leftGain));
        s->set_exposure_us(static_cast<double>(m_leftExposureUs));
    }

    // RIGHT
    {
        auto* s = req->add_settings();
        s->set_camera(CalibrationApi::RIGHT);
        s->set_gain(static_cast<double>(m_rightGain));
        s->set_exposure_us(static_cast<double>(m_rightExposureUs));
    }

    QByteArray data(static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializeToArray(data.data(), data.size())) {
        emit applyFailed("Failed to serialize camera settings request");
        return;
    }

    setBusy(true);
    m_network->sendMessage(data);
}

void CameraSettingsService::onNetworkMessageReceived(const QByteArray& data)
{
    CalibrationApi::FromServer fromServer;
    if (!fromServer.ParseFromArray(data.constData(), data.size())) return;

    if (!fromServer.has_exposure_response()) return;

    const auto& resp = fromServer.exposure_response();
    setBusy(false);

    if (resp.failures()) {
        emit applyFailed("Exposure settings apply failed");
        return;
    }

    // Sync local values from clamped response
    for (const auto& s : resp.settings()) {
        if (s.camera() == CalibrationApi::LEFT) {
            setLeftGain(static_cast<int>(s.gain()));
            setLeftExposureUs(static_cast<int>(s.exposure_us()));
        } else if (s.camera() == CalibrationApi::RIGHT) {
            setRightGain(static_cast<int>(s.gain()));
            setRightExposureUs(static_cast<int>(s.exposure_us()));
        }
    }

    emit applySucceeded("Exposure settings applied");
}