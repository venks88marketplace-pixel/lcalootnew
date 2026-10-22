#include "serverproxyservice.h"
#include "neo-calibration-server-api/calibration_server_api.pb.h"

ServerProxyService::ServerProxyService(std::shared_ptr<ILoggerService> logger,
                     std::shared_ptr<INetworkService> network,
                     QObject* parent)
    : IServerProxyService(parent), m_logger{logger}, m_network{network}
{
    auto c = connect(m_network.get(),
                     &INetworkService::messageReceived,
                     this,
                     &ServerProxyService::onNetworkMessageReceived,
                     Qt::DirectConnection);
    Q_ASSERT(c);
}

void ServerProxyService::setCameraGainsAndExposure(double leftGain, double rightGain, double exposure){
    if (!m_network || !m_network->isConnected()) {
        emit setFailed("Network not connected");
        return;
    }
    CalibrationApi::ToServer toServer;
    auto* req = toServer.mutable_set_exposure_settings();
    {
        auto* s = req->add_settings();
        s->set_camera(CalibrationApi::LEFT);
        s->set_gain(static_cast<double>(leftGain));
        s->set_exposure_us(static_cast<double>(exposure));
    }
    {
        auto* s = req->add_settings();
        s->set_camera(CalibrationApi::RIGHT);
        s->set_gain(static_cast<double>(rightGain));
        s->set_exposure_us(static_cast<double>(exposure));
    }

    QByteArray data(static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializeToArray(data.data(), data.size())) {
        emit setFailed("Failed to serialize camera settings request");
        return;
    }

    m_network->sendMessage(data);
}

void ServerProxyService::setCameraROIs(int width, int height, int leftOffsetX, int leftOffsetY, int rightOffsetX, int rightOffsetY){
    if (!m_network || !m_network->isConnected()) {
        emit setFailed("Network not connected");
        return;
    }
    CalibrationApi::ToServer toServer;
    auto* req = toServer.mutable_set_region_of_interest();
    {
        auto* s = req->add_regions();
        s->set_camera(CalibrationApi::LEFT);
        s->set_width(width);
        s->set_height(height);
        s->set_xoffset(leftOffsetX);
        s->set_yoffset(leftOffsetY);
        s->set_reversex(true);
        s->set_reversey(true);
    }
    {
        auto* s = req->add_regions();
        s->set_camera(CalibrationApi::RIGHT);
        s->set_width(width);
        s->set_height(height);
        s->set_xoffset(rightOffsetX);
        s->set_yoffset(rightOffsetY);
        s->set_reversex(true);
        s->set_reversey(true);

    }
    QByteArray data (static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializePartialToArray(data.data(), data.size())){
        emit setFailed("Failed to serialize camera settings request");
        return;
    }

    m_network->sendMessage(data);
}

void ServerProxyService::setCameraTrigger(bool on, int frequency_hz, int duration_ms){
    if (!m_network || !m_network->isConnected()) {
        emit setFailed("Network not connected");
        return;
    }
    CalibrationApi::ToServer toServer;
    auto* trigger = toServer.mutable_set_camera_trigger_controls()->mutable_controls();

    trigger->set_frequency_hz(frequency_hz);
    trigger->set_duration_ms(on ? duration_ms : 0);

    QByteArray data (static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializePartialToArray(data.data(), data.size())){
        emit setFailed("Failed to serialize trigger request");
        return;
    }
    //setBusy(true);
    m_network->sendMessage(data);
}

void ServerProxyService::setNIR(int topCurrentPercent, int bottomCurrentPercent){
    if (!m_network || !m_network->isConnected()) {
        emit setFailed("Network not connected");
        return;
    }
    CalibrationApi::ToServer toServer;
    auto* req = toServer.mutable_set_nir_led_controls();
    auto* controls = req->mutable_controls();
    controls->set_top_intensity_percent(static_cast<uint32_t>(topCurrentPercent));
    controls->set_bottom_intensity_percent(static_cast<uint32_t>(bottomCurrentPercent));

    QByteArray data (static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializePartialToArray(data.data(), data.size())){
        emit setFailed("Failed to serialize Nir request");
        return;
    }
    m_network->sendMessage(data);
}

void ServerProxyService::homeStageMotors(){
    if (!m_network || !m_network->isConnected()) {
        emit setFailed("Network not connected");
        return;
    }
    CalibrationApi::ToServer toServer;
    toServer.mutable_home_motors();
    QByteArray data (static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializePartialToArray(data.data(), data.size())){
        emit setFailed("Failed to serialize homeXYZMotors request");
        return;
    }

    m_network->sendMessage(data);
}

void ServerProxyService::setUV(int power,  int duration){
    if (!m_network || !m_network->isConnected()) {
        //emit applyFailed("Network not connected");
        return;
    }
    CalibrationApi::ToServer toServer;
    auto* req = toServer.mutable_start_uv_diagnostic();
    req->set_dac_counts(static_cast<uint32_t>(power));
    req->set_duration_ms(duration);
    QByteArray data(static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializeToArray(data.data(), data.size())) {
        emit setFailed("Failed to serialize UV start request");
        return;
    }

    m_network->sendMessage(data);
}


void ServerProxyService::setUVOff()
{
    if (!m_network || !m_network->isConnected()) {
        //emit applyFailed("Network not connected");
        return;
    }

    CalibrationApi::ToServer toServer;
    toServer.mutable_cancel_uv_diagnostic();

    QByteArray data(static_cast<int>(toServer.ByteSizeLong()), '\0');
    if (!toServer.SerializeToArray(data.data(), data.size())) {
        //emit applyFailed("Failed to serialize UV cancel request");
        return;
    }

    m_network->sendMessage(data);
}


void ServerProxyService::setDmdImage(QImage image){
    if (!m_network || !m_network->isConnected()) {
        //emit applyFailed("Network not connected");
        return;
    }
    CalibrationApi::ToServer toServer;
    auto* dmd = toServer.mutable_calibrate_dmd();
    dmd->set_image(reinterpret_cast<const char*>(const_cast<uchar*>(image.bits())),
                   image.height() * image.width());
    const std::string protoMsg = toServer.SerializeAsString();
    m_network->sendMessage(QByteArray::fromStdString(protoMsg));
}

void ServerProxyService::onNetworkMessageReceived(const QByteArray& data)
{
    CalibrationApi::FromServer fromServer;
    if (!fromServer.ParseFromArray(data.constData(), data.size())) return;

    if (!fromServer.has_exposure_response()) {
        const auto& resp = fromServer.exposure_response();
        if (resp.failures()) {
            emit setFailed("Setting Exposure failed.");
            return;
        }
        for (const auto& s : resp.settings()) {
            if (s.camera() == CalibrationApi::LEFT) {
                m_leftGain = (static_cast<int>(s.gain()));
                m_exposureSaved_um = (static_cast<int>(s.exposure_us()));
            } else if (s.camera() == CalibrationApi::RIGHT) {
                m_rightGain = (static_cast<int>(s.gain()));
                m_exposureSaved_um = (static_cast<int>(s.exposure_us()));
            }
        }
        emit setSucceeded("Exposure settings applied");
    }else if (fromServer.has_region_of_interest()) {
        const auto& resp = fromServer.region_of_interest();
        if (resp.failures()) {
            emit setFailed("Roi settings apply failed");
            return;
        }
        for (const auto& s : resp.regions()) {
            if (s.camera() == CalibrationApi::LEFT) {
                m_imageDimensions.first = (static_cast<int>(s.width()));
                m_imageDimensions.second = (static_cast<int>(s.height()));
                m_leftOffX = (static_cast<int>(s.xoffset()));
                m_leftOffY = (static_cast<int>(s.yoffset()));
            } else if (s.camera() == CalibrationApi::RIGHT) {
                m_imageDimensions.first = (static_cast<int>(s.width()));
                m_imageDimensions.second = (static_cast<int>(s.height()));
                m_rightOffX = (static_cast<int>(s.xoffset()));
                m_rightOffY = (static_cast<int>(s.yoffset()));
            }
        }
        emit setSucceeded("Roi settings applied");
    }else if (fromServer.has_nir_led_controls_response()) {
        const auto& resp = fromServer.nir_led_controls_response();
        if (!resp.success()) {
            emit setFailed("Nir settings apply failed");
            return;
        }
        std::string msg = resp.message();
        emit setSucceeded("Nir settings applied");
    }else if (fromServer.has_camera_trigger_controls_response()) {
        const auto& resp = fromServer.camera_trigger_controls_response();
        if (!resp.success()) {
            emit setFailed("Trigger settings apply failed");
            return;
        }
        std::string msg = resp.message();
        emit setSucceeded("Camera trigger settings applied");
    }else if (fromServer.has_motors_homed()) {
        const auto& resp = fromServer.motors_homed();
        emit stageHomed();
    }
}
