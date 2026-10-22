#pragma once
#include <QPixmap>
#include <QJsonObject>
#include "IService.h"

class IBaslerImageService : public IService {
    Q_OBJECT
public:
    using IService::IService;
    ~IBaslerImageService() override = default;

    Q_INVOKABLE virtual void startVideo() = 0;
    Q_INVOKABLE virtual void stopVideo() = 0;

signals:
    void imageReady(const QPixmap& image);
};