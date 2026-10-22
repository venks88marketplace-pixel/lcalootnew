#include "serviceregistry.h"
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>
#include <algorithm>

ServiceRegistry::~ServiceRegistry()
{
    stopAll();
}

ServiceRegistry::DescriptorMap& ServiceRegistry::serviceDescriptors()
{
    static DescriptorMap descriptors;
    return descriptors;
}

void ServiceRegistry::registerFactory(
    const QString& name,
    std::function<ServicePtr(ServiceRegistry&, const QJsonObject&)> factory,
    QStringList dependsOn)
{
    if (name.isEmpty()) {
        throw std::runtime_error("ServiceRegistry::registerFactory: empty service name");
    }
    if (!factory) {
        throw std::runtime_error("ServiceRegistry::registerFactory: null factory");
    }

    serviceDescriptors().insert(name, ServiceDescriptor{
        std::move(factory),
         std::move(dependsOn)
    });
}

void ServiceRegistry::configure(const QJsonObject& root)
{
    if (!root.contains("services")) {
        return;
    }
    const QJsonObject allServices = root.value("services").toObject();
    QStringList names(allServices.keys());
    std::sort(names.begin(), names.end());
    QHash<QString, VisitState> visitState;
    const QStringList names2 = names; //avoid container detachment
    for (const auto& name : names2) {
        const QJsonObject settings = allServices.value(name).toObject();
        if (!settings.value("enabled").toBool(true)) {
            continue;
        }
        createServiceRecursive(name, allServices, visitState);
    }
}
void ServiceRegistry::createServiceRecursive(
    const QString& name,
    const QJsonObject& allServices,
    QHash<QString, VisitState>& visitState)
{
    if (instances_.contains(name)) {
        return;
    }

    qDebug() << "Creating service: " << name;

    const VisitState state = visitState.value(name, VisitState::Unvisited);
    if (state == VisitState::Visiting) {
        throw std::runtime_error(
            QString("Circular dependency detected while creating service '%1'")
                .arg(name).toStdString()
            );
    }
    if (state == VisitState::Created) {
        return;
    }

    if (!allServices.contains(name)) {
        throw std::runtime_error(
            QString("Service '%1' referenced in dependency graph but not found in JSON")
                .arg(name).toStdString()
            );
    }

    const QJsonObject settings = allServices.value(name).toObject();
    if (!settings.value("enabled").toBool(true)) {
        return;
    }


    auto it = serviceDescriptors().find(name);

    if (it == serviceDescriptors().end()) {
        throw std::runtime_error(
            QString("No factory registered for service '%1'")
                .arg(name).toStdString()
            );
    }

    visitState[name] = VisitState::Visiting;

    QStringList deps = it.value().dependsOn;

    qDebug() << "Dependencies for" << name << ":" << deps;

    if (settings.contains("depends_on") && settings.value("depends_on").isArray()) {
        const QJsonArray depArray = settings.value("depends_on").toArray();
        for (const auto& dep : depArray) {
            deps.push_back(dep.toString());
        }
    }

    deps.removeDuplicates();

    for (const auto& depName : deps) {
        createServiceRecursive(depName, allServices, visitState);
    }

    qDebug() << "Invoking factory for:" << name;
    ServicePtr instance = it.value().factory(*this, settings);
    if (!instance) {
        throw std::runtime_error(
            QString("Factory returned null for service '%1'")
                .arg(name).toStdString()
            );
    }

    instance->configure(settings);
    instance->start();

    instances_.insert(name, instance);
    visitState[name] = VisitState::Created;

    qDebug() << "ServiceRegistry created:" << name;
}

void ServiceRegistry::stopAll()
{
    QStringList keys = instances_.keys();
    std::sort(keys.begin(), keys.end(), std::greater<QString>());
    const QStringList keys2 = keys; //avoid container detachment
    for (const auto& name : keys2) {
        auto it = instances_.find(name);
        if (it == instances_.end() || !it.value()) {
            continue;
        }
        try {
            it.value()->stop();
        } catch (const std::exception& ex) {
            qWarning() << "Exception while stopping service" << name << ":" << ex.what();
        } catch (...) {
            qWarning() << "Unknown exception while stopping service" << name;
        }
    }
}

void ServiceRegistry::clear()
{
    stopAll();
    instances_.clear();
}

ServiceRegistry::ServicePtr ServiceRegistry::get(const QString& name) const
{
    auto it = instances_.find(name);
    if (it != instances_.end()) {
        return it.value();
    }
    return nullptr;
}

ServiceRegistry::ServicePtr ServiceRegistry::mustGet(const QString& name) const
{
    auto ptr = get(name);
    if (!ptr) {
        throw std::runtime_error(
            QString("Service '%1' not found").arg(name).toStdString()
            );
    }
    return ptr;
}

bool ServiceRegistry::contains(const QString& name) const
{
    return instances_.contains(name);
}

QStringList ServiceRegistry::instanceNames() const
{
    return instances_.keys();
}

