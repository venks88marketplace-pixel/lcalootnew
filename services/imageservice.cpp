#include <QtDebug>
#include "opencv2/opencv.hpp"
#include <math.h>
#include <cmath>
#include "imageservice.h"
#include "neo-calibration-server-api/calibration_server_api.pb.h"
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QThread>
#include <cstring>

namespace imaging {

ImageService::ImageService(std::shared_ptr<INetworkService>& network, QObject *parent)
    : IImageService(parent), m_network(network), m_imageIndex(0)
{
    qDebug() << "[ImageService::ctor] START";
    qDebug() << "[ImageService::ctor] m_network valid?" << (m_network != nullptr);
    if (m_network) {
        qDebug() << "[ImageService::ctor] m_network->isConnected()?" << m_network->isConnected();
        connect(m_network.get(), &INetworkService::messageReceived,
                this, &ImageService::onNetworkMessageReceived);
        qDebug() << "[ImageService::ctor] Connected signal";
    } else {
        qCritical() << "[ImageService::ctor] m_network is NULL!";
    }
    qDebug() << "[ImageService::ctor] END";
}

void ImageService::getImagePair(IImageService::Caller, int index)
{
    qDebug() << "getImagePair current thread:" << QThread::currentThread();
    qDebug() << "getImagePair ImageService affinity:" << thread();

    Q_ASSERT(QThread::currentThread() == thread());

    qDebug() << "[ImageService::getImagePair] Called";
    m_mode = ActiveMode::NarrowPairSingle;
    requestCaptureForMode();
}
void ImageService::configure(const QJsonObject& settings)
{
    Q_UNUSED(settings);
}

// Existing interface method -> narrow pair stream
void ImageService::startVideo()
{
    streamNarrowPairVideo();
}

// Existing interface method -> stop narrow pair stream
void ImageService::stopVideo()
{
    stopNarrowPairVideo();
}

// Explicit narrow pair API
void ImageService::getNarrowPairImageQml()
{
    m_mode = ActiveMode::NarrowPairSingle;
    requestCaptureForMode();
}

void ImageService::streamNarrowPairVideo()
{
    if (m_mode == ActiveMode::NarrowPairVideo) return;
    m_mode = ActiveMode::NarrowPairVideo;
    m_requestInFlight = false;
    qDebug() << "[ImageService] Narrow pair video started";
    requestCaptureForMode();
}

void ImageService::stopNarrowPairVideo()
{
    if (m_mode != ActiveMode::NarrowPairVideo && m_mode != ActiveMode::NarrowPairSingle) return;

    m_mode = ActiveMode::NarrowPairSingle;
    m_timer.stop();
    disconnect(&m_timer, &QTimer::timeout, this, &ImageService::requestCaptureForMode);

    if (m_network && m_network->isConnected()) {
        CalibrationApi::ToServer toServer;
        toServer.mutable_stop_video_stream();
        sendToServerMessage(toServer);
    }
    qDebug() << "[ImageService] Narrow pair video stopped";
}

// Explicit wide API
void ImageService::getWideImageQml()
{
    m_mode = ActiveMode::WideSingle;
    requestCaptureForMode();
}

void ImageService::sendToServerMessage(CalibrationApi::ToServer& toServer)
{
    QByteArray data(toServer.ByteSizeLong(), '\0');
    toServer.SerializeToArray(data.data(), data.size());
    m_network->sendMessage(data);
}

void ImageService::requestCaptureForMode()
{
    if (m_requestInFlight) return;
    if (!m_network || !m_network->isConnected()) {
        qWarning() << "[ImageService::requestCaptureForMode] NOT CONNECTED - retrying in 100ms";
        QTimer::singleShot(100, this, [this]() {
            if (m_mode == ActiveMode::NarrowPairVideo || m_mode == ActiveMode::WideVideo) {
                requestCaptureForMode();
            }
        });
        return;
    }

    CalibrationApi::ToServer toServer;

    switch (m_mode) {
    case ActiveMode::NarrowPairSingle:
        toServer.mutable_get_synchronized_capture();
        break;
    case ActiveMode::NarrowPairVideo:
        toServer.mutable_start_video_stream();
        break;
    case ActiveMode::WideSingle:
        toServer.mutable_get_wide_capture();
        break;
    case ActiveMode::WideVideo:
        toServer.mutable_get_wide_video_stream();
        break;
    }

    m_requestInFlight = true;
    sendToServerMessage(toServer);
}

void ImageService::updateWideContrast(const QByteArray& imageData, uint32_t width, uint32_t height, int format)
{
    if (imageData.isEmpty() || width == 0 || height == 0) return;

    const auto imgFmt = static_cast<CalibrationApi::CameraImageFormat>(format);
    cv::Mat gray;

    if (imgFmt == CalibrationApi::CameraImageFormat::PNG) {
        cv::Mat buf(1, imageData.size(), CV_8UC1, const_cast<char*>(imageData.constData()));
        cv::Mat decoded = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);
        if (decoded.empty()) return;
        gray = decoded;
    } else if (imgFmt == CalibrationApi::CameraImageFormat::MONO8 ||
               imgFmt == CalibrationApi::CameraImageFormat::GB8) {
        const int expectedSize = static_cast<int>(width * height);
        if (imageData.size() < expectedSize) return;
        gray = cv::Mat(static_cast<int>(height), static_cast<int>(width), CV_8UC1,
                       const_cast<char*>(imageData.constData())).clone(); // clone for safe ops
    } else {
        return;
    }

    // Small blur suppresses salt/pepper pixels
    cv::Mat denoised;
    cv::GaussianBlur(gray, denoised, cv::Size(3, 3), 0.0);

    // Histogram (8-bit)
    int histSize = 256;
    float range[] = {0, 256};
    const float* histRange = {range};
    cv::Mat hist;
    cv::calcHist(&denoised, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange, true, false);

    const double total = static_cast<double>(denoised.total());
    if (total <= 0.0) return;

    auto percentileValue = [&](double pct) -> double {
        const double target = pct * total;
        double csum = 0.0;
        for (int i = 0; i < 256; ++i) {
            csum += hist.at<float>(i);
            if (csum >= target) return static_cast<double>(i);
        }
        return 255.0;
    };

    // Robust low/high instead of min/max
    const double low  = percentileValue(0.05); // 5th percentile
    const double high = percentileValue(0.95); // 95th percentile

    double contrast = 0.0;
    const double denom = high + low;
    if (denom > 1e-9 && high >= low) {
        contrast = 100.0 * ((high - low) / denom);
    }

    // Clamp
    if (contrast < 0.0) contrast = 0.0;
    if (contrast > 100.0) contrast = 100.0;

    if (std::abs(contrast - m_wideContrastPercent) > 0.01) {
        m_wideContrastPercent = contrast;
        emit wideContrastPercentChanged();
    }

    qDebug() << "[Contrast] p5=" << low << "p95=" << high << "contrast=" << contrast;
}

void ImageService::onNetworkMessageReceived(const QByteArray& data)
{
    qDebug() << "onNetworkMessageReceived fired!";
    int dsize = data.size();
    const char* sample = data.constData();
    CalibrationApi::FromServer fromServer;
    if (!fromServer.ParseFromArray(data.constData(), data.size())) {
        qWarning() << "[ImageService::onNetworkMessageReceived] Failed to parse FromServer message";
        m_requestInFlight = false;
        return;
    }
    bool hasFrame = false;

    // Wide path
    if (fromServer.has_wide_stream_frame()) {
        const auto& wide =
            fromServer.wide_stream_frame().wide();

        const QByteArray wideBytes(
            wide.image().data(),
            static_cast<int>(wide.image().size()));

        // Only decode/emit the raw (un-stretched) frame while a
        // measurement is actively being collected. This avoids the
        // extra per-frame decode cost during normal live streaming.
        if (m_wideMeasurementActive) {
            const QImage rawWideImage =
                bytesToRawWideImage(
                    wideBytes,
                    wide.width(),
                    wide.height(),
                    wide.format());

            if (!rawWideImage.isNull()) {
                emit wideFrameReady(rawWideImage);
            }
        }

        updateWideContrast(
            wideBytes,
            wide.width(),
            wide.height(),
            wide.format());

        QPixmap wfov =
            bytesToPixmap(
                wideBytes,
                wide.width(),
                wide.height(),
                wide.format());

        if (!wfov.isNull()) {
            m_leftImage = wfov;
            m_rightImage = wfov;
            hasFrame = true;
        }
    } else if (fromServer.has_wide_capture()) {
        const auto& wide =
            fromServer.wide_capture().wide();

        const QByteArray wideBytes(
            wide.image().data(),
            static_cast<int>(wide.image().size()));

        if (m_wideMeasurementActive) {
            const QImage rawWideImage =
                bytesToRawWideImage(
                    wideBytes,
                    wide.width(),
                    wide.height(),
                    wide.format());

            if (!rawWideImage.isNull()) {
                emit wideFrameReady(rawWideImage);
            }
        }

        updateWideContrast(
            wideBytes,
            wide.width(),
            wide.height(),
            wide.format());

        QPixmap wfov =
            bytesToPixmap(
                wideBytes,
                wide.width(),
                wide.height(),
                wide.format());

        if (!wfov.isNull()) {
            m_leftImage = wfov;
            m_rightImage = wfov;
            hasFrame = true;
        }
    }
    // Narrow pair path
    else if (fromServer.has_synchronized_capture()) {
        const auto& sync = fromServer.synchronized_capture();

        m_leftImage = bytesToPixmap(
            QByteArray(sync.left().image().data(), static_cast<int>(sync.left().image().size())),
            sync.left().width(), sync.left().height(), sync.left().format()
        );
        m_rightImage = bytesToPixmap(
            QByteArray(sync.right().image().data(), static_cast<int>(sync.right().image().size())),
            sync.right().width(), sync.right().height(), sync.right().format()
        );

        hasFrame = !m_leftImage.isNull() && !m_rightImage.isNull();
    } else if (fromServer.has_stream_frame()) {
        const auto& sync = fromServer.stream_frame().frame();

        m_leftImage = bytesToPixmap(
            QByteArray(sync.left().image().data(), static_cast<int>(sync.left().image().size())),
            sync.left().width(), sync.left().height(), sync.left().format()
        );
        m_rightImage = bytesToPixmap(
            QByteArray(sync.right().image().data(), static_cast<int>(sync.right().image().size())),
            sync.right().width(), sync.right().height(), sync.right().format()
        );

        hasFrame = !m_leftImage.isNull() && !m_rightImage.isNull();
    }

    if (hasFrame) {
        m_imageIndex++;
        emit imagesReady(m_leftImage, m_rightImage);
    }

    m_requestInFlight = false;

    if (m_mode == ActiveMode::NarrowPairVideo || m_mode == ActiveMode::WideVideo) {
        QTimer::singleShot(0, this, &ImageService::requestCaptureForMode);
    }
}

void ImageService::saveImagesToDisk(const QPixmap& left, const QPixmap& right)
{
    QString saveDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (saveDir.isEmpty()) {
        saveDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }

    QDir dir(saveDir);
    if (!dir.exists("calibration_images")) {
        dir.mkdir("calibration_images");
    }
    saveDir = dir.filePath("calibration_images");

    QString leftPath = QString("%1/left_%2.png").arg(saveDir).arg(m_imageIndex);
    QString rightPath = QString("%1/right_%2.png").arg(saveDir).arg(m_imageIndex);

    left.save(leftPath);
    right.save(rightPath);
}

QPixmap ImageService::bytesToPixmap(const QByteArray& imageData, uint32_t width, uint32_t height, int format)
{
    if (imageData.isEmpty() || width == 0 || height == 0) {
        return QPixmap();
    }

    const auto imgFmt = static_cast<CalibrationApi::CameraImageFormat>(format);

    switch (imgFmt) {
    case CalibrationApi::CameraImageFormat::PNG: {
        QPixmap pixmap;
        if (pixmap.loadFromData(reinterpret_cast<const uchar*>(imageData.constData()), imageData.size(), "PNG")) {
            return pixmap;
        }
        return QPixmap();
    }

    case CalibrationApi::CameraImageFormat::MONO8:
    case CalibrationApi::CameraImageFormat::GB8: {
        const int expectedSize = static_cast<int>(width * height);
        if (imageData.size() < expectedSize)
            return QPixmap();

        cv::Mat mono(static_cast<int>(height), static_cast<int>(width), CV_8UC1,
                     const_cast<char*>(imageData.constData()));
        QImage image(mono.data, mono.cols, mono.rows,
                     static_cast<int>(mono.step), QImage::Format_Grayscale8);

        if (image.isNull()) return QPixmap();
        return QPixmap::fromImage(image.copy());
    }

    default:
        return QPixmap();
    }
}

void ImageService::updateImages(const QPixmap& l, const QPixmap& r)
{
    emit imagesReady(l, r);
}

QImage ImageService::bytesToRawWideImage(
    const QByteArray& imageData,
    uint32_t width,
    uint32_t height,
    int format) const
{
    if (imageData.isEmpty() || width == 0 || height == 0) {
        return {};
    }

    const auto imageFormat =
        static_cast<CalibrationApi::CameraImageFormat>(format);

    if (imageFormat == CalibrationApi::CameraImageFormat::PNG) {
        return QImage::fromData(
            reinterpret_cast<const uchar*>(imageData.constData()),
            imageData.size(),
            "PNG");
    }

    if (imageFormat == CalibrationApi::CameraImageFormat::MONO8 ||
        imageFormat == CalibrationApi::CameraImageFormat::GB8) {
        const qsizetype expectedSize =
            static_cast<qsizetype>(width) *
            static_cast<qsizetype>(height);

        if (imageData.size() < expectedSize) {
            return {};
        }

        QImage image(
            static_cast<int>(width),
            static_cast<int>(height),
            QImage::Format_Grayscale8);

        for (uint32_t y = 0; y < height; ++y) {
            std::memcpy(
                image.scanLine(static_cast<int>(y)),
                imageData.constData() +
                    static_cast<qsizetype>(y) * width,
                width);
        }

        return image;
    }

    return {};
}

void ImageService::startWideVideo()
{
    if (m_mode == ActiveMode::WideVideo) return;
    m_mode = ActiveMode::WideVideo;
    m_requestInFlight = false;
    qDebug() << "[ImageService] Wide video started";
    requestCaptureForMode();
}

void ImageService::stopWideVideo()
{
    if (m_mode != ActiveMode::WideVideo && m_mode != ActiveMode::WideSingle) return;

    m_mode = ActiveMode::WideSingle;
    m_timer.stop();
    disconnect(&m_timer, &QTimer::timeout, this, &ImageService::requestCaptureForMode);

    if (m_network && m_network->isConnected()) {
        CalibrationApi::ToServer toServer;
        toServer.mutable_stop_wide_video_stream();
        sendToServerMessage(toServer);
    }
    qDebug() << "[ImageService] Wide video stopped";
}
void ImageService::setWideExposure(double exposureUs, double gain)
{
    if (!m_network || !m_network->isConnected()) {
        qWarning() << "[ImageService::setWideExposure] Not connected";
        return;
    }

    CalibrationApi::ToServer toServer;
    auto* settings = toServer.mutable_set_exposure_settings();

    auto* exposure = settings->add_settings();
    exposure->set_camera(CalibrationApi::Camera::WIDE);
    exposure->set_gain(gain);
    exposure->set_exposure_us(exposureUs);

    sendToServerMessage(toServer);
}

} // namespace imaging

