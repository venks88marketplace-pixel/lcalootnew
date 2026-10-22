#pragma once
#include <QTimer>
#include <opencv2/core.hpp>
#include <pylon/PylonIncludes.h>
#include "../../interfaces/IBaslerImageService.h"

namespace imaging {

class BaslerImageService : public IBaslerImageService {
    Q_OBJECT
public:
    explicit BaslerImageService(QObject* parent = nullptr);
    ~BaslerImageService() override;

    Q_INVOKABLE void startVideo() override;
    Q_INVOKABLE void stopVideo() override;
    void configure(const QJsonObject& settings) override;

    // Shared use count: camera runs while at least one page holds it
    Q_INVOKABLE void acquireCamera();
    Q_INVOKABLE void releaseCamera();

private slots:
    void captureFrame();

private:
    bool initializeCamera();
    void cleanupCamera();

    Pylon::CInstantCamera* m_camera = nullptr;
    Pylon::CImageFormatConverter m_formatConverter;
    Pylon::CPylonImage m_pylonImage;
    QTimer m_captureTimer;
    bool m_isGrabbing = false;
    int m_targetFps = 30;
    int m_users = 0;
};

} // namespace imaging