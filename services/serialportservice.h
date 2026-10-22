#pragma once
#include <iostream>
#include <QObject>
#include <memory>
#include <unordered_map>
#include "interfaces/ISerialPortService.h"

class SerialPortService : public ISerialPortService
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SerialPortService)

public:
    using ISerialPortService::ISerialPortService;
    explicit SerialPortService(QObject* parent = nullptr)
        :ISerialPortService(parent) {
        std::cout << "Serial port Service instantiated!!" << std::endl;
    }
    virtual std::shared_ptr<ISerialComm> getSerialPort(const std::string& portName) override;
    virtual void closePort(const std::string& portName) override;
    virtual void configure(const QJsonObject& settings) override {}
private:
    std::unordered_map<std::string, std::shared_ptr<ISerialComm>> m_ports;
};

