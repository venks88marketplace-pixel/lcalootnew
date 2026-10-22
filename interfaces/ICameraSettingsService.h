#pragma once

#include "IService.h"

class ICameraSettingsService : public IService
{
public:
    using IService::IService;
    virtual ~ICameraSettingsService() override = default;

    // Left camera
    virtual int leftGain() const = 0;
    virtual int leftExposureUs() const = 0;
    virtual void setLeftGain(int v) = 0;
    virtual void setLeftExposureUs(int v) = 0;

    // Right camera
    virtual int rightGain() const = 0;
    virtual int rightExposureUs() const = 0;
    virtual void setRightGain(int v) = 0;
    virtual void setRightExposureUs(int v) = 0;

    virtual bool busy() const = 0;

    virtual void apply() = 0;
};