#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include "../interfaces/IService.h"
#include "../interfaces/ILoggerService.h"

class LoggerService : public ILoggerService {
    Q_OBJECT
public:
    //using ILoggerService::ILoggerService;
    explicit LoggerService(QObject *parent = nullptr);

    void configure(const QJsonObject& settings) override;

    Q_INVOKABLE void info(const QString &message);
    Q_INVOKABLE void warn(const QString &message);
    Q_INVOKABLE void error(const QString &message);
    Q_INVOKABLE void debug(const QString &message);

private:
    std::shared_ptr<spdlog::logger> m_logger;
};