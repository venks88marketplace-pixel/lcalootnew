#pragma once
#include <QObject>
#include <QJsonObject>

class IService : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~IService() = default;

    virtual void configure(const QJsonObject& settings) = 0;
    virtual void start() {}
    virtual void stop() {}
};


