#pragma once
#include "../interfaces/ISerialComm.h"
#include <asio.hpp>

#include <string>

class SerialCommAsio : public ISerialComm {
    Q_OBJECT
public:
    SerialCommAsio();
    virtual ~SerialCommAsio();

    SerialCommAsio(const SerialCommAsio&) = delete;
    SerialCommAsio& operator=(const SerialCommAsio&) = delete;

    SerialCommAsio(SerialCommAsio&&) = delete;
    SerialCommAsio& operator=(SerialCommAsio&&) = delete;

    virtual void openPort(const std::string& device, unsigned int baudrate) override;
    virtual void closePort() override;

    virtual void write(const std::vector<uint8_t>& data) override;
    virtual std::string read(size_t maxBytes) override;

    bool isOpen() override;
    std::string device() { return m_device; }

    asio::serial_port& serialPort() {return m_port;};


private:
    std::string m_device;
    asio::io_context m_io;
    asio::serial_port m_port;
    asio::executor_work_guard<asio::io_context::executor_type> workGuard;
    std::thread ioThread;
};


