#pragma once
#include <QObject>

class ILoggerService : public IService {
    Q_OBJECT
public:
    using IService::IService;
public slots:
    virtual void info(const QString& msg) = 0;
    virtual void warn(const QString& msg) = 0;
    virtual void error(const QString& msg) = 0;
signals:
    void messageLogged(QString level, QString msg);
};