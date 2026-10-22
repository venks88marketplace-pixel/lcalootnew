#pragma once
#include "BaseProcessor.h"
#include "imageprocessor.h"
#include "../interfaces/IImageService.h"

class CameraOffsetProcessor : public BaseProcessor {
    Q_OBJECT

public:
    explicit CameraOffsetProcessor(QString name, std::shared_ptr<ILoggerService> logger,
                                        std::shared_ptr<IServerProxyService> proxyService,
                                        std::shared_ptr<imaging::IImageService> imageService,
                                        std::shared_ptr<IStepperControllerService>stepperService,
                                        QObject* parent=nullptr);

    Q_INVOKABLE virtual void execute(const QString& action) override; //compute offsets
    Q_INVOKABLE virtual void save() override {}

private:
    std::unique_ptr<ImageProcessor> m_imageProcessor;
};
