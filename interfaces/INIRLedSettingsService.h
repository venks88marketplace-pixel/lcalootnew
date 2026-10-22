#pragma once

#include "IService.h"

class INIRLedSettingsService : public IService
{
    Q_PROPERTY(int levelTop READ levelTop WRITE setLevelTop NOTIFY levelTopChanged)
    Q_PROPERTY(int levelBot READ levelBot WRITE setLevelBot NOTIFY levelBotChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    using IService::IService;
    virtual ~INIRLedSettingsService() override = default;

    virtual int levelTop() const = 0;
    virtual int levelBot() const = 0;
    virtual bool busy() const = 0;

    virtual void setLevelTop(int value) = 0;
    virtual void setLevelBot(int value) = 0;

    virtual void apply() = 0;
    virtual void setEnabled(bool on) = 0;
};