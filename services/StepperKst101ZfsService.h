#pragma once
#include <QObject>
#include <QString>
#include <QtQml>
#include "interfaces/IStepperControllerService.h"
#include "interfaces/ISerialPortService.h"
#include <conio.h>
#include <chrono>
#include <asio.hpp>

using namespace std::chrono;
using Time = std::chrono::steady_clock;
using ms = std::chrono::milliseconds;
using float_sec = std::chrono::duration<float>;
using float_time_point = std::chrono::time_point<Time, float_sec>;

namespace sps {

class StepperKst101ZfsService : public IStepperControllerService
{
    Q_OBJECT

public:
    explicit StepperKst101ZfsService( const std::string& portName,
                std::shared_ptr<ISerialPortService> serialportservice,
                QObject* parent =nullptr);

    void moveToPosition(double position = 1.0) override;
    void moveHome() override;
    double getCurrentPosition() override;
    bool isOpen() override;
    virtual void configure(const QJsonObject& settings) override {};

    bool disableStepMotor() const { return m_disableStepMotor; }
    char* serialNumber() { return m_serialNumber;}
    float_time_point
    getCurrentTime() {
        return Time::now();
    }

public slots:
    void setDisableStepMotor(bool value) {
        if (m_disableStepMotor == value)
            return;
        m_disableStepMotor = value;
        emit disableStepMotorChanged();
    }

signals:
    void disableStepMotorChanged();

private:
    bool waitForMoveComplete(char* serialNo,
                             WORD msgType,
                             WORD msgId,
                             std::chrono::seconds timeout);
    short moveToPosition(char* serialNo, double mm);
    bool getDeviceSerialNumber();
    bool m_disableStepMotor = false;
    char* m_serialNumber;
    const std::string m_portNumber;
    std::shared_ptr<ISerialPortService> m_serialportservice;
    bool m_KST101IsOpen{false};
};
}
