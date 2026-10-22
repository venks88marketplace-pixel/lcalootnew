#include "BaslerImageService.h"
#include <QDebug>
#include <QImage>

using namespace Pylon;
using namespace GenApi;

namespace imaging {

BaslerImageService::BaslerImageService(QObject* parent)
    : IBaslerImageService(parent)
{
    PylonInitialize();
    m_formatConverter.OutputPixelFormat = PixelType_Mono8;
    connect(&m_captureTimer, &QTimer::timeout, this, &BaslerImageService::captureFrame);
    m_captureTimer.setInterval(1000 / m_targetFps);
}

BaslerImageService::~BaslerImageService()
{
    stopVideo();
    cleanupCamera();
    PylonTerminate();
}

void BaslerImageService::configure(const QJsonObject& settings)
{
    m_targetFps = settings.value(QStringLiteral("fps")).toInt(30);
    if (m_targetFps <= 0) m_targetFps = 30;
    m_captureTimer.setInterval(1000 / m_targetFps);
}

void BaslerImageService::acquireCamera()
{
    ++m_users;
    qInfo() << "[Basler] acquire, users =" << m_users;
    startVideo();   // no-op if already grabbing
}

void BaslerImageService::releaseCamera()
{
    if (m_users > 0) --m_users;
    qInfo() << "[Basler] release, users =" << m_users;
    if (m_users == 0) stopVideo();
}

bool BaslerImageService::initializeCamera()
{
    try {
        CTlFactory& tl = CTlFactory::GetInstance();
        DeviceInfoList_t devices;
        if (tl.EnumerateDevices(devices) == 0) {
            qCritical() << "[Basler] No camera found";
            return false;
        }
        m_camera = new CInstantCamera(tl.CreateDevice(devices[0]));
        m_camera->Open();

        INodeMap& nodes = m_camera->GetNodeMap();
        CEnumerationPtr pixelFormat(nodes.GetNode("PixelFormat"));
        if (pixelFormat.IsValid() && IsWritable(pixelFormat))
            pixelFormat->FromString("Mono8");

        CBooleanPtr frameRateEnable(nodes.GetNode("AcquisitionFrameRateEnable"));
        if (frameRateEnable.IsValid() && IsWritable(frameRateEnable))
            frameRateEnable->SetValue(true);
        CFloatPtr frameRate(nodes.GetNode("AcquisitionFrameRate"));
        if (frameRate.IsValid() && IsWritable(frameRate))
            frameRate->SetValue(static_cast<double>(m_targetFps));

        return true;
    } catch (const GenericException& e) {
        qCritical() << "[Basler] Init failed:" << e.GetDescription();
        cleanupCamera();
        return false;
    }
}

void BaslerImageService::cleanupCamera()
{
    if (!m_camera) return;
    try {
        if (m_camera->IsOpen()) m_camera->Close();
    } catch (const GenericException& e) {
        qWarning() << "[Basler] Close failed:" << e.GetDescription();
    }
    delete m_camera;
    m_camera = nullptr;
}

void BaslerImageService::startVideo()
{
    if (m_isGrabbing) return;
    if (!m_camera && !initializeCamera()) return;
    try {
        m_camera->StartGrabbing(GrabStrategy_LatestImageOnly);
        m_isGrabbing = true;
        m_captureTimer.start();
    } catch (const GenericException& e) {
        qCritical() << "[Basler] Start failed:" << e.GetDescription();
    }
}

void BaslerImageService::stopVideo()
{
    m_captureTimer.stop();
    if (m_camera && m_isGrabbing) {
        try { m_camera->StopGrabbing(); }
        catch (const GenericException& e) { qWarning() << "[Basler] Stop failed:" << e.GetDescription(); }
    }
    m_isGrabbing = false;
}

void BaslerImageService::captureFrame()
{
    if (!m_camera || !m_isGrabbing) return;
    try {
        CGrabResultPtr result;
        if (!m_camera->RetrieveResult(0, result, TimeoutHandling_Return)) return;
        if (!result->GrabSucceeded()) return;

        m_formatConverter.Convert(m_pylonImage, result);
        const int w = static_cast<int>(m_pylonImage.GetWidth());
        const int h = static_cast<int>(m_pylonImage.GetHeight());
        const int stride = w + static_cast<int>(m_pylonImage.GetPaddingX());

        QImage img(static_cast<const uchar*>(m_pylonImage.GetBuffer()), w, h, stride,
                   QImage::Format_Grayscale8);
        emit imageReady(QPixmap::fromImage(img.copy()));
    } catch (const GenericException& e) {
        qWarning() << "[Basler] Capture failed:" << e.GetDescription();
    }
}

} // namespace imaging