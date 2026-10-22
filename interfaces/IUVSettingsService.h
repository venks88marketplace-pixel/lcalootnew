#pragma once

#include "IService.h"

class IUVSettingsService : public IService
{
public:
    using IService::IService;
    virtual ~IUVSettingsService() override = default;

    virtual int power() const = 0;
    virtual bool busy() const = 0;

    virtual void setPower(int value) = 0;

    // ON path: start UV diagnostic
    virtual void apply() = 0;

    // true => start, false => cancel
    virtual void setEnabled(bool on) = 0;
};