#include "configloader.h"
#include <QFile>
#include <QJsonDocument>
#include <stdexcept>

QJsonObject loadConfig(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(
            QString("Failed to open config: %1").arg(path).toStdString()
            );
    }

    const auto data = file.readAll();
    const auto doc = QJsonDocument::fromJson(data);

    if (!doc.isObject()) {
        throw std::runtime_error("Invalid config.json");
    }

    return doc.object();
}
