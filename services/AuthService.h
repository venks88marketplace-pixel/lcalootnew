#pragma once
#include <QObject>
#include "../interfaces/IService.h"

class AuthService : public IService {
    Q_OBJECT
public:
    using IService::IService;

    Q_INVOKABLE void signIn(const QString& username, const QString& password);
    virtual void configure(const QJsonObject& settings) override{}

signals:
    void signedIn();
    void signInFailed(QString reason);

private:
    //LoggerService* m_logger;
};
