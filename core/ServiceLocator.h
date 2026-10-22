#include <QHash>
#include <functional>
#include <memory>
#include <typeindex>

class ServiceLocator {
public:
    // static ServiceLocator& instance() {
    //     static ServiceLocator locator;
    //     return locator;
    // }

    template<typename T>
    void registerFactory(std::function<std::shared_ptr<T>()> factory) {
        factories[typeid(T)] = [factory]() {
            return std::static_pointer_cast<void>(factory());
        };
    }

    template<typename T>
    std::shared_ptr<T> getService() {
        auto type = std::type_index(typeid(T));

        // If already created, return it
        if (services.contains(type)) {
            return std::static_pointer_cast<T>(services[type]);
        }

        // Otherwise, create via factory
        if (factories.contains(type)) {
            auto service = factories[type]();
            services[type] = service;
            return std::static_pointer_cast<T>(service);
        }

        return nullptr; // Not registered
    }

private:
    QHash<std::type_index, std::shared_ptr<void>> services;
    QHash<std::type_index, std::function<std::shared_ptr<void>()>> factories;

    ServiceLocator() = default;
    //Q_DISABLE_COPY(ServiceLocator)
};
