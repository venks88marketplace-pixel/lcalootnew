#include "triggersettingsservice.h"

#include <QDebug>
#include "neo-calibration-server-api/calibration_server_api.pb.h"

TriggerSettingsService::TriggerSettingsService(std::shared_ptr<INetworkService> network, QObject* parent)
    : ITriggerSettingsService(parent), m_network(std::move(network))
{
    qDebug() << "[TriggerSettingsService::ctor] START";
    qDebug() << "[TriggerSettingsService::ctor] m_network valid?" << (m_network != nullptr);

    if (m_network) {
        qDebug() << "[TriggerSettingsService::ctor] m_network->isConnected()?" << m_network->isConnected();
        connect(m_network.get(), &INetworkService::messageReceived,
                this, &TriggerSettingsService::onNetworkMessageReceived);
        qDebug() << "[TriggerSettingsService::ctor] Connected signal";
    } else {
        qCritical() << "[TriggerSettingsService::ctor] m_network is NULL!";
    }

    qDebug() << "[TriggerSettingsService::ctor] END";
}

void TriggerSettingsService::configure(const QJsonObject& settings)
{
    Q_UNUSED(settings);
}

int TriggerSettingsService::clampNonNegative(int v) const
{
    return v < 0 ? 0 : v;
}

void TriggerSettingsService::setBusy(bool b)
{
    if (m_busy == b) return;
    m_busy = b;
    emit busyChanged();
}

void TriggerSettingsService::setFrequency(int value)
{
    const int v = clampNonNegative(value);
    if (m_frequency == v) return;
    m_frequency = v;
    emit frequencyChanged();
}

void TriggerSettingsService::setDuration(int value)
{
    const int v = clampNonNegative(value);
    if (m_duration == v) return;
    m_duration = v;
    emit durationChanged();
}

void TriggerSettingsService::setEnabled(bool on)
{
    if (!on) {
        setFrequency(0);
        setDuration(0);
    }
    apply();
}

void TriggerSettingsService::apply()
{
    qDebug() << "[TriggerSettingsService::apply] START";
    qDebug() << "[TriggerSettingsService::apply] isConnected?" << (m_network ? m_network->isConnected() : false);

    if (!m_network || !m_network->isConnected()) {
        qWarning() << "[TriggerSettingsService::apply] NOT CONNECTED";
        emit applyFailed("Network not connected");
        return;
    }

    CalibrationApi::ToServer toServer;
    auto* req = toServer.mutable_set_camera_trigger_controls();
    auto* controls = req->mutable_controls();
    controls->set_frequency_hz(static_cast<uint32_t>(m_frequency));
    controls->set_duration_ms(static_cast<uint32_t>(m_duration));

    QByteArray data(static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializeToArray(data.data(), data.size())) {
        qWarning() << "[TriggerSettingsService::apply] SerializeToArray failed";
        emit applyFailed("Failed to serialize Trigger request");
        return;
    }

    setBusy(true);
    qDebug() << "[TriggerSettingsService::apply] Sending" << data.size()
             << "bytes freq=" << m_frequency << "dur=" << m_duration;

    m_network->sendMessage(data);
    qDebug() << "[TriggerSettingsService::apply] Message sent";
}

void TriggerSettingsService::onNetworkMessageReceived(const QByteArray& data)
{
    CalibrationApi::FromServer fromServer;
    if (!fromServer.ParseFromArray(data.constData(), data.size())) return;
    if (!fromServer.has_camera_trigger_controls_response()) return;

    const auto& resp = fromServer.camera_trigger_controls_response();
    setBusy(false);

    if (resp.success()) {
        emit applySucceeded(QString::fromStdString(resp.message()));
    } else {
        emit applyFailed(QString::fromStdString(resp.message()));
    }
}