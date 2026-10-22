#pragma once

#include <QObject>
#include <QJsonObject>
#include "interfaces/IService.h"

class IStepperControllerService : public IService
{
    Q_OBJECT
public:
    explicit IStepperControllerService(QObject* parent = nullptr) : IService(parent) {}
    ~IStepperControllerService() override = default;

    virtual void moveToPosition(double position = 1.0) = 0;
    virtual void moveHome() = 0;
    virtual double getCurrentPosition() = 0;
    virtual bool isOpen() = 0;

signals:
    void moveCompleted();
    void moveHomeCompleted();
    void moveFailed(const QString& reason);
};
