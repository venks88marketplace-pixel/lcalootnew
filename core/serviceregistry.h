#pragma once
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include "../interfaces/IService.h"
#include "../interfaces/IProcessor.h"
#include "../interfaces/ILoggerService.h"


class ServiceRegistry
{
    enum class VisitState {
        Unvisited = 0,
        Visiting,
        Created
    };

public:
    using ServicePtr = std::shared_ptr<IService>;

    ServiceRegistry() = default;
    ~ServiceRegistry();

    struct ServiceDescriptor {
        std::function<ServicePtr(ServiceRegistry&, const QJsonObject&)> factory;
        QStringList dependsOn;
    };

    using DescriptorMap = QHash<QString, ServiceDescriptor>;

    static DescriptorMap& serviceDescriptors();

    template<typename T>
    void registerInstance(const QString& name, std::shared_ptr<T> instance) {
        instances_[name] = instance;
    }

    static void registerFactory(
        const QString& name,
        std::function<ServicePtr(ServiceRegistry&, const QJsonObject&)> factory,
        QStringList dependsOn = {}
        );


    template<typename TService>
    static void registerSimpleType(const QString& name, QStringList dependsOn = {})
    {
        static_assert(std::is_base_of_v<IService, TService>,
                      "TService must derive from IService");

        registerFactory(
            name,
            [](ServiceRegistry&, const QJsonObject&) {
                return std::make_shared<TService>(nullptr);
            },
            std::move(dependsOn)
            );
    }

    void configure(const QJsonObject& root);
    void stopAll();
    void clear();

    ServicePtr get(const QString& name) const;
    ServicePtr mustGet(const QString& name) const;
    bool contains(const QString& name) const;

    template<typename T>
    std::shared_ptr<T> getAs(const QString& name) const
    {
        return std::dynamic_pointer_cast<T>(get(name));
    }

    template<typename T>
    std::shared_ptr<T> mustGetAs(const QString& name) const
    {
        auto ptr = std::dynamic_pointer_cast<T>(mustGet(name));
        if (!ptr) {
            throw std::runtime_error(
                QString("Service '%1' exists, but has unexpected type")
                    .arg(name).toStdString()
                );
        }
        return ptr;
    }
    QStringList instanceNames() const;

private:
    void createServiceRecursive(
        const QString& name,
        const QJsonObject& allServices,
        QHash<QString, VisitState>& visitState
        );

private:
    QHash<QString, ServicePtr> instances_;
};

