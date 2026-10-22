#pragma once

#include <QObject>
#include <QImage>
#include "interfaces/IService.h"


struct nirLedControls {
    uint32_t top_intensity_percent;
    uint32_t bottom_intensity_percent;
};

struct cameraTrigger {
    uint32_t frequency_hz;
    uint32_t duration_ms;
};

class IServerProxyService : public IService
{
    Q_OBJECT
public:
    using IService::IService;
    //explicit IServerProxyService(QObject* parent = nullptr) :
    virtual ~IServerProxyService() override = default;

    virtual void setCameraGainsAndExposure(double leftGain, double rightGain, double exposure) = 0;
    virtual void setCameraROIs(int width, int height, int leftOffsetX, int leftOffestY, int rightOffsetX, int rightOffsetY) = 0;
    virtual void setCameraTrigger(bool on, int frequency_hz, int duration_ms) = 0;
    virtual void setNIR(int topCurrentPercent, int bottomCurrentPercent) = 0;

    virtual void setUV(int power,  int duration) = 0;
    virtual void setUVOff() = 0;
    virtual void setDmdImage(QImage image) = 0;
    virtual void homeStageMotors() = 0;

signals:
    void setCameraGainsAndExposureCompleted();
    void setCameraROIsCompleted();
    void setCameraTriggerCompleted();
    void setNIRCompleted();
    void setUVCompleted();
    void setDmdImageCompleted();
    void homeStageMotorsCompleted();

    void setFailed(QString msg);
    void setSucceeded(QString msg);
    void stageHomed();
};
