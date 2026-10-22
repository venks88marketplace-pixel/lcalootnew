#pragma once
#include <QObject>
#include <QString>
#include "IService.h"
#include "models/MetaData.h"

class IProcessor : public IService
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)

public:
    using IService::IService;
    virtual ~IProcessor() = default;
    virtual void configure(const QJsonObject& settings) override {}

    virtual QString name() const = 0;
    virtual QString status() const = 0;

    Q_INVOKABLE virtual void execute(const QString& action) = 0;
    Q_INVOKABLE virtual void save() = 0;

signals:
    void nameChanged();
    void stateChanged();
};
