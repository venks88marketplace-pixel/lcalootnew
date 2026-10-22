#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <memory>
#include <interfaces/INetworkService.h>
#include <interfaces/ITriggerSettingsService.h>

class TriggerSettingsService : public ITriggerSettingsService
{
    Q_OBJECT
    Q_PROPERTY(int frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)
    Q_PROPERTY(int duration READ duration WRITE setDuration NOTIFY durationChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit TriggerSettingsService(std::shared_ptr<INetworkService> network, QObject* parent = nullptr);

    int frequency() const override { return m_frequency; }
    int duration() const override { return m_duration; }
    bool busy() const override { return m_busy; }

    void setFrequency(int value) override;
    void setDuration(int value) override;

    Q_INVOKABLE void apply() override;
    Q_INVOKABLE void setEnabled(bool on) override;
    void configure(const QJsonObject& settings) override;

public slots:
    void onNetworkMessageReceived(const QByteArray& data);

signals:
    void frequencyChanged();
    void durationChanged();
    void busyChanged();

    void applySucceeded(const QString& message);
    void applyFailed(const QString& message);

private:
    int clampNonNegative(int v) const;
    void setBusy(bool b);

private:
    std::shared_ptr<INetworkService> m_network;
    int m_frequency{0};
    int m_duration{0};
    bool m_busy{false};
};