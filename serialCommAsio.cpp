#include "serialCommAsio.h"
#include <iostream>

class SerialException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

SerialCommAsio::SerialCommAsio()
    : m_port(m_io),
    workGuard(asio::make_work_guard(m_io)) {
        ioThread = std::thread([this]() {
        m_io.run();
    });
}

SerialCommAsio::~SerialCommAsio(){
    closePort();
    workGuard.reset();
    m_io.stop();
    if (ioThread.joinable())
        ioThread.join();
};

void SerialCommAsio::openPort(const std::string& device, unsigned int baudrate) {
    asio::error_code ec;

    m_port.open(device, ec);
    if (ec)
        throw SerialException("Open failed: " + ec.message());
    m_port.set_option(asio::serial_port_base::baud_rate(baudrate));
    m_port.set_option(asio::serial_port_base::character_size(8));
    m_port.set_option(asio::serial_port_base::parity(
        asio::serial_port_base::parity::none));
    m_port.set_option(asio::serial_port_base::stop_bits(
        asio::serial_port_base::stop_bits::one));
    m_port.set_option(asio::serial_port_base::flow_control(
        asio::serial_port_base::flow_control::none));
}

void SerialCommAsio::closePort() {
    if (m_port.is_open()) {
        asio::error_code ec;
        m_port.close(ec);
    }
}

void SerialCommAsio::write(const std::vector<uint8_t>& data) {
    asio::error_code ec;
    asio::write(m_port, asio::buffer(data), ec);
    if (ec)
        throw SerialException("Write failed: " + ec.message());
}
std::string SerialCommAsio::read(size_t maxBytes) {
    std::vector<char> buf(maxBytes);
    asio::error_code ec;
    size_t len = asio::read(m_port, asio::buffer(buf), ec);
    if (ec)
        throw SerialException("Read failed: " + ec.message());

    std::cout << "-----------------------" << len << std::endl;
    return std::string(buf.data(), len);
}

bool SerialCommAsio::isOpen() {
    return m_port.is_open();
}

// std::size_t bytes_available(asio::serial_port& port) {
//     //asio::serial_port_base::bytes_readable command;
//     //port.io_control(command);
//     //return command.get();


// //    std::size_t bytes_available(asio::serial_port& port) {
//         HANDLE h = port.native_handle();

//         COMSTAT status;
//         DWORD errors;

//         if (ClearCommError(h, &errors, &status)) {
//             return status.cbInQue;  // ✅ bytes available
//         }
//         return 0;
// //    }


// }
