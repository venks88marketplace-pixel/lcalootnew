#pragma once

#include <QObject>
#include <QImage>
#include <QByteArray>

#include "interfaces/IServerProxyService.h"
#include "interfaces/ILoggerService.h"
#include "interfaces/INetworkService.h"

class ServerProxyService : public IServerProxyService
{
    Q_OBJECT
public:
    explicit ServerProxyService(std::shared_ptr<ILoggerService> logger,
                         std::shared_ptr<INetworkService> network,
                         QObject* parent = nullptr);

    void setCameraGainsAndExposure(double leftGain, double rightGain, double exposure) override;
    void setCameraROIs(int width, int height, int leftOffsetX, int leftOffsetY, int rightOffsetX, int rightOffsetY) override;
    void setCameraTrigger(bool on, int frequency_hz, int duration_ms) override;
    void setNIR(int topCurrentPercent, int bottomCurrentPercent) override;

    void setUV(int power,  int duration) override;
    void setUVOff() override;
    void setDmdImage(QImage image) override;
    void homeStageMotors() override;
    void configure(const QJsonObject& settings) override {};

    std::pair<uint32_t, uint32_t> getImageDimentions(){return m_imageDimensions;}

public slots:
    void onNetworkMessageReceived(const QByteArray& data);

private:

    const std::shared_ptr<ILoggerService> m_logger;
    const std::shared_ptr<INetworkService> m_network;

    int m_leftOffX{0};
    int m_leftOffY{0};
    int m_rightOffX{0};
    int m_rightOffY{0};
    int m_exposureSaved_um{0};
    std::pair<uint32_t, uint32_t> m_imageDimensions;

    double m_leftGain{0.0};
    double m_rightGain{0.0};
};
