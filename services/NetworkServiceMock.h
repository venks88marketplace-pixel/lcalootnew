#pragma once
#include "../interfaces/INetworkService.h"

#include <QByteArray>
#include <QDebug>

class NetworkServiceMock : public INetworkService
{
    Q_OBJECT
public:
    using INetworkService::INetworkService;

    void configure(const QJsonObject& settings) override
    {
        Q_UNUSED(settings);
    }

    void start() override
    {
        connected_ = true;
        qDebug() << "NetworkServiceMock started";
    }

    void stop() override
    {
        connected_ = false;
        qDebug() << "NetworkServiceMock stopped";
    }

    void sendMessage(const QByteArray& data) override
    {
        lastMessage_ = data;
        qDebug() << "NetworkServiceMock captured:" << data;
    }

    bool isConnected() const override
    {
        return connected_;
    }

    QByteArray lastMessage() const
    {
        return lastMessage_;
    }

private:
    bool connected_ = false;
    QByteArray lastMessage_;
};
