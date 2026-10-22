#include "nirledsettingsservice.h"

#include <QDebug>
#include "neo-calibration-server-api/calibration_server_api.pb.h"

NIRLedSettingsService::NIRLedSettingsService(std::shared_ptr<INetworkService> network, QObject* parent)
    : INIRLedSettingsService(parent), m_network(std::move(network))
{
    qDebug() << "[NIRLedSettingsService::ctor] START";
    qDebug() << "[NIRLedSettingsService::ctor] m_network valid?" << (m_network != nullptr);

    if (m_network) {
        qDebug() << "[NIRLedSettingsService::ctor] m_network->isConnected()?" << m_network->isConnected();
        connect(m_network.get(), &INetworkService::messageReceived,
                this, &NIRLedSettingsService::onNetworkMessageReceived);
        qDebug() << "[NIRLedSettingsService::ctor] Connected signal";
    } else {
        qCritical() << "[NIRLedSettingsService::ctor] m_network is NULL!";
    }

    qDebug() << "[NIRLedSettingsService::ctor] END";
}

int NIRLedSettingsService::clampPercent(int v) const
{
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
}

void NIRLedSettingsService::setBusy(bool b)
{
    if (m_busy == b) return;
    m_busy = b;
    emit busyChanged();
}

void NIRLedSettingsService::setLevelTop(int value)
{
    const int v = clampPercent(value);
    if (m_levelTop == v) return;
    m_levelTop = v;
    emit levelTopChanged();
}

void NIRLedSettingsService::setLevelBot(int value)
{
    const int v = clampPercent(value);
    if (m_levelBot == v) return;
    m_levelBot = v;
    emit levelBotChanged();
}

void NIRLedSettingsService::setEnabled(bool on)
{
    if (!on) {
        setLevelTop(0);
        setLevelBot(0);
    }
    apply();
}

void NIRLedSettingsService::apply()
{
    qDebug() << "[NIRLedSettingsService::apply] START";
    qDebug() << "[NIRLedSettingsService::apply] m_network valid?" << (m_network != nullptr);
    qDebug() << "[NIRLedSettingsService::apply] isConnected?" << (m_network ? m_network->isConnected() : false);

    if (!m_network || !m_network->isConnected()) {
        qWarning() << "[NIRLedSettingsService::apply] NOT CONNECTED";
        emit applyFailed("Network not connected");
        return;
    }

    CalibrationApi::ToServer toServer;
    auto* req = toServer.mutable_set_nir_led_controls();
    auto* controls = req->mutable_controls();
    controls->set_top_intensity_percent(static_cast<uint32_t>(m_levelTop));
    controls->set_bottom_intensity_percent(static_cast<uint32_t>(m_levelBot));

    QByteArray data(static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializeToArray(data.data(), data.size())) {
        qWarning() << "[NIRLedSettingsService::apply] SerializeToArray failed";
        emit applyFailed("Failed to serialize NIR LED request");
        return;
    }

    setBusy(true);
    qDebug() << "[NIRLedSettingsService::apply] Sending" << data.size()
             << "bytes top=" << m_levelTop << "bot=" << m_levelBot;

    m_network->sendMessage(data);
    qDebug() << "[NIRLedSettingsService::apply] Message sent";
}

void NIRLedSettingsService::onNetworkMessageReceived(const QByteArray& data)
{
    CalibrationApi::FromServer fromServer;
    if (!fromServer.ParseFromArray(data.constData(), data.size())) {
        return; // not parseable, ignore
    }

    if (!fromServer.has_nir_led_controls_response()) {
        return; // not our message, ignore
    }

    const auto& resp = fromServer.nir_led_controls_response();
    setBusy(false);

    if (resp.success()) {
        qDebug() << "[NIRLedSettingsService] apply success:" << QString::fromStdString(resp.message());
        emit applySucceeded(QString::fromStdString(resp.message()));
    } else {
        qWarning() << "[NIRLedSettingsService] apply failed:" << QString::fromStdString(resp.message());
        emit applyFailed(QString::fromStdString(resp.message()));
    }
}

void NIRLedSettingsService::configure(const QJsonObject& settings)
{
    Q_UNUSED(settings);
}