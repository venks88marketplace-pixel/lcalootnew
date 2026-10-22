#include "configservice.h"
#include <QFile>
#include <QDebug>
#include <QJsonParseError>

ConfigService::ConfigService(QObject *parent) : QObject(parent) {}

bool ConfigService::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file:" << path;
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << err.errorString();
        return false;
    }

    m_config = doc.object();
    emit configLoaded();
    return true;
}

QJsonValue ConfigService::value(const QString &keyPath) const {
    QStringList parts = keyPath.split('.');
    QJsonValue val = m_config;
    for (const QString& key : parts) {
        if (!val.isObject())
            return {};
        val = val.toObject().value(key);
    }
    return val;
}
QString ConfigService::stringValue(const QString &keyPath, const QString &defaultValue) const {
    QJsonValue v = value(keyPath);
    return v.isString() ? v.toString() : defaultValue;
}

int ConfigService::intValue(const QString &keyPath, int defaultValue) const {
    QJsonValue v = value(keyPath);
    return v.isDouble() ? v.toInt() : defaultValue;
}

bool ConfigService::boolValue(const QString &keyPath, bool defaultValue) const {
    QJsonValue v = value(keyPath);
    return v.isBool() ? v.toBool() : defaultValue;
}

