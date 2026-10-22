#pragma once
#include <QObject>
#include <QtQml>
#include "../models/ShapeModel.h"
#include "../models/UIMessageModel.h"
#include "../interfaces/IProcessor.h"
#include "../interfaces/IImageService.h"
#include "../interfaces/ILoggerService.h"
#include "../interfaces/IStepperControllerService.h"
#include "interfaces/IServerProxyService.h"

class BaseProcessor : public IProcessor {
    Q_OBJECT

public:
    BaseProcessor(QString name,
                  std::shared_ptr<ILoggerService> logger,
                  std::shared_ptr<IServerProxyService> proxyservice,
                  std::shared_ptr<imaging::IImageService> imageservice,
                  std::shared_ptr<IStepperControllerService> stepperservice,
                  QObject* parent=nullptr);

    ~BaseProcessor() override = default;

    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QObject* leftOverlay READ leftOverlay CONSTANT)
    Q_PROPERTY(QObject* rightOverlay READ rightOverlay CONSTANT)
    Q_PROPERTY(QObject* uiMessage READ uiMessage CONSTANT)
    Q_PROPERTY(QObject* imageService READ imageService CONSTANT)

    Q_INVOKABLE virtual void execute(const QString& action) override {}
    Q_INVOKABLE virtual void save() override {}

    Q_INVOKABLE void clearOverlays();

    virtual QString name() const override { return m_name; }
    virtual QString status() const override { return m_status; }

    ShapeModel* leftOverlay() const { return m_leftOverlay; }
    ShapeModel* rightOverlay() const { return m_rightOverlay; }
    UIMessageModel* uiMessage() const { return m_uiMessage; }

    QObject* imageService() const;
    QObject* stepperService() const;

protected:
    QString m_name;
    QString m_status;

    const std::shared_ptr<ILoggerService> m_logger;
    const std::shared_ptr<imaging::IImageService> m_imageService;
    const std::shared_ptr<IStepperControllerService> m_stepperService;
    const std::shared_ptr<IServerProxyService> m_proxy;

    ShapeModel* m_leftOverlay= nullptr;
    ShapeModel* m_rightOverlay = nullptr;

    UIMessageModel* m_uiMessage = nullptr;
    bool IsImageServiceSimulated() const;
};
