#pragma once
#include <QObject>
#include "IService.h"
#include "ISerialComm.h"
#include <string>

class ISerialPortService : public IService {
    Q_OBJECT
public:
    using IService::IService;

    virtual ~ISerialPortService() = default;

    virtual std::shared_ptr<ISerialComm> getSerialPort(const std::string& portName) = 0;
    virtual void closePort(const std::string& portName) = 0;
};
