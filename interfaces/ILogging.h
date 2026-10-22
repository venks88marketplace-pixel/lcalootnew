#pragma once
#include <QObject>

class ILogger {
    Q_Object
    
public:
    using QObject::QObject;
    
    virtual ~ILogger() override = default;
    virtual void log(const QString &msg) = 0;
};
