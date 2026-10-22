#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QDebug>
#include <asio.hpp>
#include <thread>
#include <atomic>
#include <array>
#include <cstdint>
#include "../interfaces/INetworkService.h"

class NetworkService : public INetworkService {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

public:
    explicit NetworkService(QObject* parent = nullptr)
        : INetworkService(parent)
    {}

    ~NetworkService() override;

    Q_INVOKABLE void connectToHost(const QString &host, quint16 port);
    Q_INVOKABLE void disconnectFromHost();
    Q_INVOKABLE void sendMessage(const QString &message);

    bool connected() const { return m_connected.load(); }

    // INetworkService overrides
    void sendMessage(const QByteArray& data) override;
    bool isConnected() const override { return m_connected.load(); }
    void configure(const QJsonObject& settings) override;
    void start();
    Q_INVOKABLE bool checkConnection() const {
        qDebug() << "[NetworkService::checkConnection] m_connected =" << m_connected.load();
        return m_connected.load();
    }

signals:
    void disconnected();
    void errorOccurred(QString msg);
    void connectedChanged();

private:
    void startRead();
    void writeBytes(const QByteArray& data);
    void close();
    QByteArray receiveBuffer(); // (unused, kept for compatibility)

    std::atomic<bool> m_connected{false};

    asio::io_context m_io;
    std::unique_ptr<asio::ip::tcp::socket> m_socket;
    std::thread m_ioThread;

    // Bigger buffer to avoid 2KB trickle reads
    std::vector<char> m_readBuffer;
    std::vector<char> m_messageBuffer; // Accumulate bytes for complete message

    // from config.json
    QString m_host;
    int     m_port = 0;
};