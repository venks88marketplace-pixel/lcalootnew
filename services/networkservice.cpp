#include "networkservice.h"
#include <QMetaObject>
#include <QDebug>
#include <cstring>

using asio::ip::tcp;

namespace {
constexpr std::size_t kReadChunkSize = 64 * 1024;          // 64KB
constexpr quint32 kMaxMessageSize    = 100 * 1024 * 1024; // 100MB
}

NetworkService::~NetworkService() {
    disconnectFromHost();
}

void NetworkService::connectToHost(const QString &host, quint16 port)
{
    qDebug() << "[NetworkService::connectToHost] Attempting connection to" << host << ":" << port;
    if (m_connected.load()) {
        emit errorOccurred("Already connected");
        return;
    }
    m_readBuffer.resize(kReadChunkSize, 0);
    m_messageBuffer.clear();
    m_messageBuffer.reserve(12 * 1024 * 1024); // avoid repeated reallocs for ~10MB frames

    m_socket = std::make_unique<tcp::socket>(m_io);

    // Optional socket tuning for large frame transport
    std::error_code sockEc;
    m_socket->set_option(asio::socket_base::receive_buffer_size(2 * 1024 * 1024), sockEc);
    m_socket->set_option(asio::socket_base::send_buffer_size(2 * 1024 * 1024), sockEc);
    m_socket->set_option(tcp::no_delay(true), sockEc);

    tcp::resolver resolver(m_io);
    auto endpoints = resolver.resolve(host.toStdString(), std::to_string(port));

    asio::async_connect(*m_socket, endpoints, [this](std::error_code ec, const tcp::endpoint&) {
        if (!ec) {
            qDebug() << "[NetworkService::connectToHost] SUCCESS! Connected!";
            m_connected.store(true);
            QMetaObject::invokeMethod(this, [this]() {
                emit connectedChanged();
            }, Qt::QueuedConnection);
            startRead();
        } else {
            qCritical() << "[NetworkService::connectToHost] FAILED:" << ec.message().c_str();
            QMetaObject::invokeMethod(this, [this, ec] () {
                emit errorOccurred(QString("Connect failed: %1").arg(ec.message().c_str()));
            }, Qt::QueuedConnection);
        }
    });

    // start io event loop
    if (!m_ioThread.joinable()) {
        qDebug() << "[NetworkService::connectToHost] Starting IO thread...";
        m_ioThread = std::thread([this]{ m_io.run(); });
    }
}

void NetworkService::disconnectFromHost()
{
    //qDebug() << "[NetworkService::disconnectFromHost] shutting down";
    if (!m_connected.load() && !m_socket) {
        return;
    }
    m_connected.store(false);
    emit connectedChanged();

    bool wasConnected = m_connected.exchange(false);
    if (wasConnected)
        emit connectedChanged();

    if (m_socket) {
        asio::post(m_io, [this]() {
            if (m_socket) {
                std::error_code ec;
                m_socket->cancel(ec);
                m_socket->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                m_socket->close(ec);
            }
        });
    }
    m_io.stop();
    if (m_ioThread.joinable())
        m_ioThread.join();

    m_socket.reset();
    m_io.restart();

    if (wasConnected)
        emit disconnected();
}

void NetworkService::sendMessage(const QString &message)
{
    qDebug() << "Sending (QString):" << message;
    if (!m_connected.load()) {
        emit errorOccurred("Not connected");
        return;
    }

    std::string data = message.toStdString() + "\n";
    asio::post(m_io, [this, data] () {
        writeBytes(QByteArray(data.c_str(), static_cast<int>(data.length())));
    });
}

void NetworkService::sendMessage(const QByteArray& data)
{
    qDebug() << "[NetworkService::sendMessage] Sending (QByteArray):" << data.size() << "bytes";
    if (!m_connected.load()) {
        emit errorOccurred("Not connected");
        return;
    }
    // Add 4-byte big-endian length prefix (matching server protocol)
    QByteArray lengthPrefix(4, 0);
    quint32 size = static_cast<quint32>(data.size());
    lengthPrefix[0] = static_cast<char>((size >> 24) & 0xFF);
    lengthPrefix[1] = static_cast<char>((size >> 16) & 0xFF);
    lengthPrefix[2] = static_cast<char>((size >> 8) & 0xFF);
    lengthPrefix[3] = static_cast<char>(size & 0xFF);

    QByteArray fullMessage = lengthPrefix + data;
    qDebug() << "[NetworkService::sendMessage] With length prefix:" << fullMessage.size() << "bytes";
    qDebug() << "[NetworkService::sendMessage] Length prefix (hex):"
             << QString::number(size, 16) << "(" << size << "bytes)";

    QByteArray dataCopy = fullMessage;

    quint32 sizeOut = static_cast<quint32>(dataCopy.size());

    writeBytes(dataCopy);

    //asio::post(m_io, [this, dataCopy] () { writeBytes(dataCopy); });
}

void NetworkService::configure(const QJsonObject &settings)
{
    if (settings.contains("host")) m_host = settings["host"].toString();
    if (settings.contains("port")) m_port = settings["port"].toInt();

    qDebug() << "NetworkService configured with host=" << m_host << "port=" << m_port;
}

void NetworkService::startRead()
{
    if (!m_socket || !m_socket->is_open()) {
        emit errorOccurred("Socket not opened");
        return;
    }

    m_socket->async_read_some(asio::buffer(m_readBuffer.data(), m_readBuffer.size()),
        [this](std::error_code ec, std::size_t len) {
            if (!ec) {
                // Append received bytes
                m_messageBuffer.insert(
                    m_messageBuffer.end(),
                    m_readBuffer.begin(),
                    m_readBuffer.begin() + static_cast<std::ptrdiff_t>(len)
                );

                // Try to extract complete messages (length-prefixed)
                while (m_messageBuffer.size() >= 4) {
                    quint32 messageLength = 0;
                    messageLength |= (static_cast<quint32>(static_cast<unsigned char>(m_messageBuffer[0])) << 24);
                    messageLength |= (static_cast<quint32>(static_cast<unsigned char>(m_messageBuffer[1])) << 16);
                    messageLength |= (static_cast<quint32>(static_cast<unsigned char>(m_messageBuffer[2])) << 8);
                    messageLength |=  static_cast<quint32>(static_cast<unsigned char>(m_messageBuffer[3]));

                    if (messageLength > kMaxMessageSize) {
                        qCritical() << "[NetworkService::startRead] INVALID message length:" << messageLength;
                        m_connected.store(false);
                        emit connectedChanged();
                        close();
                        return;
                    }

                    const std::size_t totalNeeded = 4u + static_cast<std::size_t>(messageLength);
                    if (m_messageBuffer.size() < totalNeeded) {
                        // throttle log: only when crossing ~1MB boundaries
                        if ((m_messageBuffer.size() % (1024 * 1024)) < len) {
                            qDebug() << "[NetworkService::startRead] Incomplete message:"
                                     << m_messageBuffer.size() << "available,"
                                     << totalNeeded << "needed";
                        }
                        break;
                    }

                    // Extract payload only (skip 4-byte prefix)
                    QByteArray message(m_messageBuffer.data() + 4, static_cast<int>(messageLength));

                    qDebug() << "[NetworkService::startRead] Emitting messageReceived with" << message.size() << "bytes";
                    QMetaObject::invokeMethod(this, [this, message] () {
                        emit messageReceived(message);
                    }, Qt::QueuedConnection);

                    // Remove processed message
                    m_messageBuffer.erase(
                        m_messageBuffer.begin(),
                        m_messageBuffer.begin() + static_cast<std::ptrdiff_t>(totalNeeded)
                    );
                }

                startRead();
            } else {
                qCritical() << "[NetworkService::startRead] Error:" << ec.message().c_str();
                m_connected.store(false);
                emit connectedChanged();
                close();
            }
        });
}

void NetworkService::writeBytes(const QByteArray& data)
{
    if (!m_socket || !m_socket->is_open()) {
        emit errorOccurred("Socket not opened");
        return;
    }

    asio::write(*m_socket, asio::buffer(data));

    // asio::async_write(*m_socket, asio::buffer(data.constData(), static_cast<std::size_t>(data.size())),
    //     [this] (std::error_code ec, std::size_t len ) {
    //         if (ec) {
    //             QMetaObject::invokeMethod(this, [this, ec] () {
    //                 emit errorOccurred(QString("Write error: %1").arg(ec.message().c_str()));
    //             }, Qt::QueuedConnection);
    //             close();
    //         } else {
    //             qDebug() << "[NetworkService::writeBytes] Wrote" << len << "bytes successfully";
    //         }
    //     });
}

void NetworkService::close()
{
    std::error_code ec;
    if (m_socket && m_socket->is_open() ) {
        m_socket->shutdown(tcp::socket::shutdown_both, ec);
        m_socket->close(ec);
    }
}

void NetworkService::start()
{
    qDebug() << "[NetworkService::start] host=" << m_host << "port=" << m_port;
    if (!m_host.isEmpty() && m_port > 0) {
        qDebug() << "[NetworkService::start] Auto-connecting to" << m_host << ":" << m_port;
        connectToHost(m_host, static_cast<quint16>(m_port));
    } else {
        qDebug() << "[NetworkService::start] No host/port configured";
    }
}