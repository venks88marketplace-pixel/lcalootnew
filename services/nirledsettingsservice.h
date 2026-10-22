#pragma once

#include <QObject>
#include <QJsonObject>
#include <QByteArray>
#include <memory>
#include <interfaces/INetworkService.h>
#include <interfaces/INIRLedSettingsService.h>

class NIRLedSettingsService : public INIRLedSettingsService
{
    Q_OBJECT
    Q_PROPERTY(int levelTop READ levelTop WRITE setLevelTop NOTIFY levelTopChanged)
    Q_PROPERTY(int levelBot READ levelBot WRITE setLevelBot NOTIFY levelBotChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit NIRLedSettingsService(std::shared_ptr<INetworkService> network, QObject* parent = nullptr);

    int levelTop() const override { return m_levelTop; }
    int levelBot() const override { return m_levelBot; }
    bool busy() const override { return m_busy; }

    void setLevelTop(int value) override;
    void setLevelBot(int value) override;
    void configure(const QJsonObject& settings) override;

    Q_INVOKABLE void apply() override;
    Q_INVOKABLE void setEnabled(bool on) override; // convenience: ON=>apply current values, OFF=>zero both then apply

public slots:
    void onNetworkMessageReceived(const QByteArray& data);

signals:
    void levelTopChanged();
    void levelBotChanged();
    void busyChanged();

    void applySucceeded(const QString& message);
    void applyFailed(const QString& message);

private:
    int clampPercent(int v) const;
    void setBusy(bool b);

private:
    std::shared_ptr<INetworkService> m_network;
    int m_levelTop{0};
    int m_levelBot{0};
    bool m_busy{false};
};