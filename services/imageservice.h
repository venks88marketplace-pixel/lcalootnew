#pragma once

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QColor>
#include <QQmlContext>
#include <QTimer>
#include <QJsonObject>
#include <memory>
#include <interfaces/IImageService.h>
#include <interfaces/INetworkService.h>
#include "neo-calibration-server-api/calibration_server_api.pb.h"


namespace imaging {
class ImageService : public IImageService
{
    Q_OBJECT
    Q_PROPERTY(double wideContrastPercent READ wideContrastPercent NOTIFY wideContrastPercentChanged)

public:
    explicit ImageService(std::shared_ptr<INetworkService>& network, QObject *parent = nullptr);

    explicit ImageService(const ImageService& rhs) = delete;
    ImageService& operator= (const ImageService& rhs) = delete;

    Q_INVOKABLE void startVideo() override;
    Q_INVOKABLE void stopVideo() override;
    Q_INVOKABLE void startWideVideo() override;
    Q_INVOKABLE void stopWideVideo() override;

    void configure(const QJsonObject& settings) override;
    void updateImages(const QPixmap& l, const QPixmap& r) override;

    // Explicit NARROW PAIR API
    Q_INVOKABLE void getNarrowPairImageQml();
    Q_INVOKABLE void streamNarrowPairVideo();
    Q_INVOKABLE void stopNarrowPairVideo();

    // Explicit WIDE API
    Q_INVOKABLE void getWideImageQml();
    // Q_INVOKABLE void streamWideVideo();
    // Q_INVOKABLE void stopWideVideo();

    double wideContrastPercent() const { return m_wideContrastPercent; }

    // When false (default), the raw WFOV frame is not decoded and
    // wideFrameReady() is not emitted. This keeps normal streaming
    // fast. Set to true only while a WFOV contrast measurement is
    // actively collecting frames.
    void setWideMeasurementActive(bool active) override { m_wideMeasurementActive = active; }
    bool wideMeasurementActive() const { return m_wideMeasurementActive; }
    void setWideExposure(double exposureUs, double gain = 0.0);
public slots:
    void onNetworkMessageReceived(const QByteArray& data);
    Q_INVOKABLE void getImagePair(IImageService::Caller caller, int index) override;

signals:
    void wideContrastPercentChanged();

private:
    enum class ActiveMode {
        NarrowPairSingle,
        NarrowPairVideo,
        WideSingle,
        WideVideo
    };

    void requestCaptureForMode();
    void sendToServerMessage(CalibrationApi::ToServer& toServer);
    void updateWideContrast(const QByteArray& imageData, uint32_t width, uint32_t height, int format);

    QPixmap bytesToPixmap(const QByteArray& imageData, uint32_t width, uint32_t height, int format);
    void saveImagesToDisk(const QPixmap& left, const QPixmap& right);

    QImage bytesToRawWideImage(
        const QByteArray& imageData,
        uint32_t width,
        uint32_t height,
        int format) const;

private:
    QTimer m_timer;
    int m_imageIndex = 0;
    std::shared_ptr<INetworkService> m_network;
    bool m_requestInFlight = false;
    ActiveMode m_mode = ActiveMode::NarrowPairSingle;
    double m_wideContrastPercent = 0.0;
    bool m_wideMeasurementActive = false;
};

}
