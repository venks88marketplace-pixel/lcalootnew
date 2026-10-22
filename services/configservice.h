#pragma once
#include <QObject>
#include <QJsonObject>
#include <QVariant>

class ConfigService : public QObject{
    Q_OBJECT
public:
    explicit ConfigService(QObject *parent = nullptr);

    //using IService::IService;
    // Q_INVOKABLE bool load(const QString &path);
    // Q_INVOKABLE QVariant getValue(const QString &key) const;

    bool load(const QString &path);
    QJsonObject config() const { return m_config; }

    //void configure(const QJsonObject& settings) override;

    QJsonValue value(const QString &keyPath) const;
    QString stringValue(const QString &keyPath, const QString &defaultValue = QString()) const;
    int intValue(const QString &keyPath, int defaultValue = 0) const;
    bool boolValue(const QString &keyPath, bool defaultValue = false) const;

signals:
    void configLoaded();

private:
    QJsonObject m_config;
};

