#pragma once
#include "IService.h"
#include <QByteArray>

class INetworkService : public IService {
    Q_OBJECT
public:
    using IService::IService;
    virtual ~INetworkService() = default;

    virtual void sendMessage(const QByteArray& data) = 0;
    virtual bool isConnected() const = 0;

signals:
    void messageReceived(const QByteArray& data);
};