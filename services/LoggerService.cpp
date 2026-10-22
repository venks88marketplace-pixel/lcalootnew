#include "LoggerService.h"

LoggerService::LoggerService(QObject *parent)
    : ILoggerService(parent)
{
    //m_instance = this;
}

void LoggerService::configure(const QJsonObject& settings)
{
    const QString fileName = settings.value("file").toString("app.log");

    spdlog::drop("app_logger");
    m_logger = spdlog::basic_logger_mt("app_logger", fileName.toStdString());
    m_logger->set_level(spdlog::level::debug);
}

void LoggerService::info(const QString &message)
{
    if (m_logger) m_logger->info(message.toStdString());
}

void LoggerService::warn(const QString &message)
{
    if (m_logger) m_logger->warn(message.toStdString());
}

void LoggerService::error(const QString &message)
{
    if (m_logger) m_logger->error(message.toStdString());
}

void LoggerService::debug(const QString &message)
{
    if (m_logger) m_logger->debug(message.toStdString());
}