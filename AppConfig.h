#include <QJsonObject>

class AppConfig {
public:
    explicit AppConfig(QJsonObject root)
        : root_(std::move(root)) {}

    const QJsonObject& json() const {
        return root_;
    }

private:
    QJsonObject root_;
};