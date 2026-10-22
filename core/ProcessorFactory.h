#pragma once

#include <memory>
#include <QString>

class ServiceRegistry;
class IProcessor;

class ProcessorFactory
{
public:
    explicit ProcessorFactory(ServiceRegistry& registry);

    std::shared_ptr<IProcessor> create(const QString& type);

private:
    ServiceRegistry& m_registry;
};

