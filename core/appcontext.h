#pragma once
#include <QObject>
#include <memory>
#include <QtQml/qqml.h>

class ServiceRegistry;
class LoggerService;
class UVCalibrationProcessor;
class UVAlignmentProcessor;

#include "../services/camerasettingsservice.h"
#include "../services/nirledsettingsservice.h"
#include "../services/triggersettingsservice.h"
#include "../services/uvsettingsservice.h"
#include "../services/networkservice.h"
#include "../services/AuthService.h"
#include "../services/imageservice.h"
#include "../services/BaslerImageService.h"
#include "../services/UVBlockSetupService.h"
#include "../bendprocessors/CameraOffsetProcessor.h"
#include "../bendprocessors/PhotometricProcessor.h"
#include "../bendprocessors/StereoCalibrationProcessor.h"
#include "../bendprocessors/UVCalibrationProcessor.h"
#include "../bendprocessors/UVAlignmentProcessor.h"
#include "../bendprocessors/DMDCalibrationProcessor.h"
#include "../bendprocessors/DMDCheckProcessor.h"
#include "interfaces/ISerialPortService.h"
#include "interfaces/IStepperControllerService.h"
#include <interfaces/INIRLedSettingsService.h>
#include <interfaces/ITriggerSettingsService.h>
#include <interfaces/ICameraSettingsService.h>
#include <interfaces/IUVSettingsService.h>
#include "interfaces/IDepthSensorService.h"
#include "../wfov/WfovContrastProcessor.h"

class AppContext : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QObject* authService READ authService CONSTANT)
    Q_PROPERTY(QObject* networkService READ networkService CONSTANT)
    Q_PROPERTY(QObject* cameraOffsetProcessor READ cameraOffsetProcessor CONSTANT)
    Q_PROPERTY(QObject* photometricProcessor READ photometricProcessor CONSTANT)
    Q_PROPERTY(QObject* stereoCalibrationProcessor READ stereoCalibrationProcessor CONSTANT)
    Q_PROPERTY(QObject* stereoCalibrationVerificationProcessor READ stereoCalibrationVerificationProcessor CONSTANT)
    Q_PROPERTY(QObject* dmdCalibrationProcessor READ dmdCalibrationProcessor CONSTANT)
    Q_PROPERTY(QObject* dmdCheckProcessor READ dmdCheckProcessor CONSTANT)
    Q_PROPERTY(UVCalibrationProcessor* uvCalibrationProcessor READ getUvCalibrationProcessor CONSTANT)
    Q_PROPERTY(UVAlignmentProcessor* uvAlignmentProcessor READ getUvAlignmentProcessor CONSTANT)

    Q_PROPERTY(QObject* cameraSettingsService READ cameraSettingsService CONSTANT)
    Q_PROPERTY(QObject* nirledSettingsService READ nirledSettingsService CONSTANT)
    Q_PROPERTY(QObject* triggerSettingsService READ triggerSettingsService CONSTANT)
    Q_PROPERTY(QObject* uvSettingsService READ uvSettingsService CONSTANT)
    Q_PROPERTY(QObject* imageService READ imageService CONSTANT)
    Q_PROPERTY(QObject* serialPortService READ serialPortService CONSTANT)
    Q_PROPERTY(QObject* stepperControllerService READ stepperControllerService CONSTANT)
    Q_PROPERTY(QObject* depthStepperControllerService READ depthStepperControllerService CONSTANT)
    Q_PROPERTY(QObject* depthSensorService READ depthSensorService CONSTANT)
    Q_PROPERTY(WfovContrastProcessor* wfovContrastProcessor READ wfovContrastProcessor CONSTANT)
    Q_PROPERTY(QObject* baslerImageService READ baslerImageService CONSTANT)
    Q_PROPERTY(QObject* uvBlockSetupService READ uvBlockSetupService CONSTANT)

public:
    explicit AppContext(ServiceRegistry& registry, QObject* parent = nullptr);

    QObject* authService() const;
    QObject* networkService() const;
    QObject* cameraOffsetProcessor() const;
    QObject* photometricProcessor() const;
    QObject* stereoCalibrationProcessor() const;
    QObject* stereoCalibrationVerificationProcessor() const;
    QObject* dmdCalibrationProcessor() const;
    QObject* dmdCheckProcessor() const;
    UVCalibrationProcessor* getUvCalibrationProcessor() const { return uvCalibrationProcessor_.get(); }
    UVAlignmentProcessor* getUvAlignmentProcessor() const { return uvAlignmentProcessor_.get(); }

    QObject* cameraSettingsService() const;
    QObject* nirledSettingsService() const;
    QObject* triggerSettingsService() const;
    QObject* uvSettingsService() const;
    QObject* imageService() const;
    QObject* serialPortService() const;
    QObject* stepperControllerService() const;
    QObject* depthStepperControllerService() const;
    QObject* depthSensorService() const;

    Q_INVOKABLE bool createCalibrationFile(const QString& metaString);

    WfovContrastProcessor* wfovContrastProcessor() const { return m_wfovContrastProcessor.get(); }
    QObject* baslerImageService() const;
    QObject* uvBlockSetupService() const;

private:
    std::shared_ptr<AuthService> m_auth;
    std::shared_ptr<LoggerService> m_loggerservice;
    std::shared_ptr<INetworkService> m_network;

    std::shared_ptr<ICameraSettingsService> m_cameraSettingsService;
    std::shared_ptr<INIRLedSettingsService> m_nirledSettingsService;
    std::shared_ptr<ITriggerSettingsService> m_triggerSettingsService;
    std::shared_ptr<IUVSettingsService> m_uvSettingsService;
    std::shared_ptr<imaging::IImageService> m_imageService;
    std::shared_ptr<ISerialPortService> m_serialPortService;
    std::shared_ptr<IStepperControllerService> m_stepperControllerService;
    std::shared_ptr<IStepperControllerService> m_depthStepperControllerService;

    std::shared_ptr<CameraOffsetProcessor> m_cameraOffsetProcessor;
    std::shared_ptr<PhotometricProcessor> m_photometricProcessor;
    std::shared_ptr<StereoCalibrationProcessor> m_stereoCalibrationProcessor;
    std::shared_ptr<StereoCalibrationProcessor> m_stereoCalibrationVerificationProcessor;
    std::shared_ptr<DMDCalibrationProcessor> m_dmdCalibrationProcessor;
    std::shared_ptr<DMDCheckProcessor> m_dmdCheckProcessor;
    std::shared_ptr<UVCalibrationProcessor> uvCalibrationProcessor_;
    std::shared_ptr<UVAlignmentProcessor> uvAlignmentProcessor_;
    std::shared_ptr<IDepthSensorService> m_depthSensorService;

    std::shared_ptr<WfovContrastProcessor> m_wfovContrastProcessor;
    std::shared_ptr<imaging::BaslerImageService> m_baslerImageService;
    std::shared_ptr<UVBlockSetupService> m_uvBlockSetupService;
};