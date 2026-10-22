#include "serialportservice.h"
#include "../src/serialCommAsio.h"

std::shared_ptr<ISerialComm> SerialPortService::getSerialPort(const std::string& portName) {
    try {
        auto& port = m_ports[portName];
        if (!port) {
            port = std::make_shared<SerialCommAsio>();
            port->openPort(portName, 115200);
            m_ports[portName] = std::move( port);
        }
        return port;
    }
    catch( std::exception& ex) {
        qDebug() << "Exception thrown on serial port query";
    }
    return nullptr;
}

void SerialPortService::closePort(const std::string& portName) {
    auto it = m_ports.find(portName);
    if (it != m_ports.end()) {
        m_ports[portName]->closePort();
    }
}
