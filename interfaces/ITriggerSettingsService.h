#pragma once

#include "IService.h"

class ITriggerSettingsService : public IService
{
public:
    using IService::IService;
    virtual ~ITriggerSettingsService() override = default;

    virtual int frequency() const = 0;
    virtual int duration() const = 0; // ms
    virtual bool busy() const = 0;

    virtual void setFrequency(int value) = 0;
    virtual void setDuration(int value) = 0;

    virtual void apply() = 0;
    virtual void setEnabled(bool on) = 0;
};