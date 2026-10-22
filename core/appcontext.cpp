#include "appcontext.h"

#include "serviceregistry.h"

#include "services/AuthService.h"
#include "services/LoggerService.h"
#include "services/BaslerImageService.h"
#include "services/UVBlockSetupService.h"

#include "interfaces/INetworkService.h"
#include "interfaces/INIRLedSettingsService.h"
#include "interfaces/ITriggerSettingsService.h"
#include "interfaces/ICameraSettingsService.h"
#include "interfaces/IUVSettingsService.h"
#include "interfaces/ISerialPortService.h"
#include "interfaces/IStepperControllerService.h"
#include "interfaces/IDepthSensorService.h"

#include "bendprocessors/UVCalibrationProcessor.h"
#include "../wfov/WfovContrastProcessor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>

AppContext::AppContext(
    ServiceRegistry& registry,
    QObject* parent)
    : QObject(parent)
{
    m_auth =
        registry.mustGetAs<AuthService>(
            "AuthService");

    m_network =
        registry.mustGetAs<INetworkService>(
            "NetworkService");

    m_cameraOffsetProcessor =
        registry.mustGetAs<CameraOffsetProcessor>(
            "CameraOffsetProcessor");

    m_photometricProcessor =
        registry.mustGetAs<PhotometricProcessor>(
            "PhotometricProcessor");

    m_stereoCalibrationProcessor =
        registry.mustGetAs<StereoCalibrationProcessor>(
            "StereoCalibrationProcessor");
    m_stereoCalibrationVerificationProcessor = registry.mustGetAs<StereoCalibrationProcessor>("StereoCalibrationVerificationProcessor");

    m_dmdCalibrationProcessor =
        registry.mustGetAs<DMDCalibrationProcessor>(
            "DMDCalibrationProcessor");
    m_dmdCheckProcessor = registry.mustGetAs<DMDCheckProcessor>("DMDCheckProcessor");
    // Get UV Settings Service first
    m_uvSettingsService = registry.mustGetAs<IUVSettingsService>("UVSettingsService");

    // Basler camera service is created directly for UV Launch assembly
    m_baslerImageService = std::make_shared<imaging::BaslerImageService>(nullptr);
    m_baslerImageService->configure(QJsonObject());

    // UV Block Assembly page setup/teardown commands (PD1 gain, UV DAC, exposure)
    m_uvBlockSetupService = std::make_shared<UVBlockSetupService>(m_network, m_uvSettingsService, nullptr);

    // Create UVCalibrationProcessor with all required services
    auto logger = registry.mustGetAs<LoggerService>("LoggerService");
    uvCalibrationProcessor_ = std::make_shared<UVCalibrationProcessor>(
        "UVCalibrationProcessor",
        std::move(logger),
        m_network,
        m_uvSettingsService
    );

    m_cameraSettingsService =
        registry.mustGetAs<ICameraSettingsService>(
            "CameraSettingsService");

    m_nirledSettingsService =
        registry.mustGetAs<INIRLedSettingsService>(
            "NIRLedSettingsService");

    m_triggerSettingsService =
        registry.mustGetAs<ITriggerSettingsService>(
            "TriggerSettingsService");

    m_imageService =
        registry.mustGetAs<imaging::IImageService>(
            "ImageService");

    m_serialPortService =
        registry.mustGetAs<ISerialPortService>(
            "SerialPortService");

    m_stepperControllerService =
        registry.mustGetAs<IStepperControllerService>(
            "StepperControllerService");

    m_depthStepperControllerService =
        registry.mustGetAs<IStepperControllerService>(
            "DepthStepperControllerService");

    m_depthSensorService =
        registry.mustGetAs<IDepthSensorService>(
            "DepthSensorService");

    // Create UVAlignmentProcessor
    uvAlignmentProcessor_ =
        registry.mustGetAs<UVAlignmentProcessor>(
            "UVAlignmentProcessor");

    // Must be created after ImageService has been resolved.
    m_wfovContrastProcessor =
        std::make_shared<WfovContrastProcessor>(
            m_imageService,
            this);
}

QObject* AppContext::authService() const
{
    return m_auth.get();
}

QObject* AppContext::networkService() const
{
    return m_network.get();
}

QObject* AppContext::cameraSettingsService() const
{
    return m_cameraSettingsService.get();
}

QObject* AppContext::nirledSettingsService() const
{
    return m_nirledSettingsService.get();
}

QObject* AppContext::triggerSettingsService() const
{
    return m_triggerSettingsService.get();
}

QObject* AppContext::uvSettingsService() const
{
    return m_uvSettingsService.get();
}

QObject* AppContext::imageService() const
{
    return m_imageService.get();
}

QObject* AppContext::serialPortService() const
{
    return m_serialPortService.get();
}

QObject* AppContext::stepperControllerService() const
{
    return m_stepperControllerService.get();
}

QObject* AppContext::depthStepperControllerService() const
{
    return m_depthStepperControllerService.get();
}

QObject* AppContext::depthSensorService() const
{
    return m_depthSensorService.get();
}

QObject* AppContext::cameraOffsetProcessor() const
{
    return m_cameraOffsetProcessor.get();
}

QObject* AppContext::photometricProcessor() const
{
    return m_photometricProcessor.get();
}

QObject* AppContext::stereoCalibrationProcessor() const
{
    return m_stereoCalibrationProcessor.get();
}

QObject* AppContext::stereoCalibrationVerificationProcessor() const
{
    return m_stereoCalibrationVerificationProcessor.get();
}

QObject* AppContext::dmdCalibrationProcessor() const
{
    return m_dmdCalibrationProcessor.get();
}
QObject* AppContext::dmdCheckProcessor() const {
    return m_dmdCheckProcessor.get();
}

QObject* AppContext::baslerImageService() const
{
    return m_baslerImageService.get();
}

QObject* AppContext::uvBlockSetupService() const
{
    return m_uvBlockSetupService.get();
}

bool AppContext::createCalibrationFile(
    const QString& metaString)
{
    const QString targetPath =
        QCoreApplication::applicationDirPath()
        + "/calibfile_"
        + metaString
        + ".json";

    if (QFile::exists(targetPath)) {
        return true;
    }

    QFile source(":/config/calfile_template.json");

    if (!source.open(QIODevice::ReadOnly)) {
        return false;
    }

    QDir().mkpath(QFileInfo(targetPath).path());

    QFile destination(targetPath);

    if (!destination.open(QIODevice::WriteOnly)) {
        return false;
    }

    destination.write(source.readAll());

    return true;
}
