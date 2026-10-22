#pragma once
#include <QObject>
#include <QString>
#include <QtQml>
#include "interfaces/IStepperControllerService.h"
#include <conio.h>
#include <chrono>
#include <asio.hpp>

class StepperServiceMock : public IStepperControllerService
{
    Q_OBJECT

public:
    explicit StepperServiceMock(QObject* parent =nullptr);
    Q_INVOKABLE void moveToPosition(double position = 1.0) override;
    Q_INVOKABLE void moveHome() override;
    Q_INVOKABLE double getCurrentPosition() override;
    bool isOpen() override;
    virtual void configure(const QJsonObject& settings) override {};
private:
    double m_currentPosition{0};
};

