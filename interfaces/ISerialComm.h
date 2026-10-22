#pragma once
#include <QObject>
#include <QJsonObject>

class ISerialComm : public QObject {
    Q_OBJECT
public:

    virtual ~ISerialComm() = default;
    virtual void openPort(const std::string& port, unsigned int bauderate) = 0;
    virtual void closePort() = 0;

    virtual void write(const std::vector<uint8_t>& data) = 0;
    virtual std::string read(size_t maxBytes) = 0;
    virtual bool isOpen() = 0;
};


