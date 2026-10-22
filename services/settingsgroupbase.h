#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <functional>
#include "../interfaces/INetworkService.h"

class SettingsGroupBase : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool pending READ pending NOTIFY pendingChanged)
public:
    explicit SettingsGroupBase( std::shared_ptr<INetworkService> network, QObject *parent = nullptr)
        : QObject(parent), m_pending(false), m_network(network) {}

    bool pending() const { return m_pending; }
    INetworkService& network() const { return m_network; }

/*
    void sendWithRollback(const QString &key,
        const QVariant &newValue,
        const QVariant &oldValue,
        std::function<void(QVariant)> applyValue) {
            // Optimistic update: apply new value immediately
            applyValue(newValue);

            m_network.sendSetting(key, newValue, [=](bool success) {
                if (!success) {
                    // Rollback to old value
                    applyValue(oldValue);
                    emit settingChangeFailed(oldValue, newValue,
                                             QString("%1 update failed").arg(key));
                }
            });
        }
*/
public slots:
    virtual void apply() = 0;


signals:
    void pendingChanged(bool);
    void applySucceeded();
    void applyFailed(const QString &reason);

protected:
    void setPending(bool p) {
        if (m_pending != p) {
            m_pending = p;
            emit pendingChanged(m_pending);
        }
    }

private:
    bool m_pending;
    std::shared_ptr<INetworkService> m_network;
};


