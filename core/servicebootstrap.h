#pragma once

#include <QJsonObject>

class ServiceBootstrap
{
public:
    static void registerFactories(const QJsonObject& config);
};


