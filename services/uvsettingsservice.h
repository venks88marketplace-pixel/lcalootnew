#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <memory>
#include <interfaces/INetworkService.h>
#include <interfaces/IUVSettingsService.h>

class UVSettingsService : public IUVSettingsService
{
    Q_OBJECT
    Q_PROPERTY(int power READ power WRITE setPower NOTIFY powerChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit UVSettingsService(std::shared_ptr<INetworkService> network, QObject* parent = nullptr);

    int power() const override { return m_power; }
    bool busy() const override { return m_busy; }

    void setPower(int value) override;

    Q_INVOKABLE void apply() override;           // start_uv_diagnostic
    Q_INVOKABLE void setEnabled(bool on) override; // false => cancel_uv_diagnostic
    void configure(const QJsonObject& settings) override;

public slots:
    void onNetworkMessageReceived(const QByteArray& data);

signals:
    void powerChanged();
    void busyChanged();

    void applySucceeded(const QString& message);
    void applyFailed(const QString& message);

private:
    int clampNonNegative(int v) const;
    void setBusy(bool b);

private:
    std::shared_ptr<INetworkService> m_network;
    int m_power{0};      // maps to dac_counts
    bool m_busy{false};
};