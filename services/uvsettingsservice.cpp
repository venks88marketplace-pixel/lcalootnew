#include "uvsettingsservice.h"

#include <QDebug>
#include "neo-calibration-server-api/calibration_server_api.pb.h"

UVSettingsService::UVSettingsService(std::shared_ptr<INetworkService> network, QObject* parent)
    : IUVSettingsService(parent), m_network(std::move(network))
{
    if (m_network) {
        connect(m_network.get(), &INetworkService::messageReceived,
                this, &UVSettingsService::onNetworkMessageReceived);
    }
}

void UVSettingsService::configure(const QJsonObject& settings)
{
    Q_UNUSED(settings);
}

int UVSettingsService::clampNonNegative(int v) const
{
    return v < 0 ? 0 : v;
}

void UVSettingsService::setBusy(bool b)
{
    if (m_busy == b) return;
    m_busy = b;
    emit busyChanged();
}

void UVSettingsService::setPower(int value)
{
    const int v = clampNonNegative(value);
    if (m_power == v) return;
    m_power = v;
    emit powerChanged();
}

void UVSettingsService::apply()
{
    if (!m_network || !m_network->isConnected()) {
        emit applyFailed("Network not connected");
        return;
    }

    CalibrationApi::ToServer toServer;
    auto* req = toServer.mutable_start_uv_diagnostic();
    req->set_dac_counts(static_cast<uint32_t>(m_power));
    req->set_duration_ms(300000); // fixed 5 minutes

    QByteArray data(static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializeToArray(data.data(), data.size())) {
        emit applyFailed("Failed to serialize UV start request");
        return;
    }

    setBusy(true);
    m_network->sendMessage(data);
}

void UVSettingsService::setEnabled(bool on)
{
    if (on) {
        apply();
        return;
    }

    if (!m_network || !m_network->isConnected()) {
        emit applyFailed("Network not connected");
        return;
    }

    CalibrationApi::ToServer toServer;
    toServer.mutable_cancel_uv_diagnostic();

    QByteArray data(static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializeToArray(data.data(), data.size())) {
        emit applyFailed("Failed to serialize UV cancel request");
        return;
    }

    setBusy(true);
    m_network->sendMessage(data);
}

void UVSettingsService::onNetworkMessageReceived(const QByteArray& data)
{
    CalibrationApi::FromServer fromServer;
    if (!fromServer.ParseFromArray(data.constData(), data.size())) {
        return;
    }

    if (!fromServer.has_uv_diagnostic_response()) {
        return;
    }

    const auto& resp = fromServer.uv_diagnostic_response();
    setBusy(false);

    if (resp.success()) {
        emit applySucceeded(QString::fromStdString(resp.message()));
    } else {
        emit applyFailed(QString::fromStdString(resp.message()));
    }
}