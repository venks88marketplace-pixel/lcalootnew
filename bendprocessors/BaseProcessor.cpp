#include "BaseProcessor.h"

BaseProcessor::BaseProcessor(QString name,
                             std::shared_ptr<ILoggerService> logger,
                             std::shared_ptr<IServerProxyService> proxyservice,
                             std::shared_ptr<imaging::IImageService> imageservice,
                             std::shared_ptr<IStepperControllerService> stepperservice,
                             QObject* parent)
    : IProcessor(parent),
    m_logger(std::move(logger)),
    m_proxy(std::move(proxyservice)),
    m_imageService(std::move(imageservice)),
    m_stepperService(std::move(stepperservice)),
    m_leftOverlay(new ShapeModel(this)),
    m_rightOverlay(new ShapeModel(this)),
    m_uiMessage(new UIMessageModel(this)) {
        m_name = name;
        m_status = "idle";
}

QObject* BaseProcessor::imageService() const {
    return m_imageService.get();
}

QObject* BaseProcessor::stepperService() const {
    return m_stepperService.get();
}

void BaseProcessor::clearOverlays() {
    leftOverlay()->clear();
    rightOverlay()->clear();
    uiMessage()->clear();
}

bool BaseProcessor::IsImageServiceSimulated() const {
    return QString("imaging::ImageServiceMock") == m_imageService.get()->metaObject()->className();
}
