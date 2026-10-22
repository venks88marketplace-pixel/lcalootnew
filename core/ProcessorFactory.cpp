#include "ProcessorFactory.h"

#include "serviceregistry.h"
#include "../interfaces/ILoggerService.h"
#include "../interfaces/INetworkService.h"
#include "../interfaces/IImageService.h"
#include "../interfaces/IStepperControllerService.h"

#include "../bendprocessors/CameraOffsetProcessor.h"
#include "../bendprocessors/PhotometricProcessor.h"
#include "../bendprocessors/StereoCalibrationProcessor.h"
#include "../bendprocessors/UVAlignmentProcessor.h"

#include <stdexcept>

using imaging::IImageService;

ProcessorFactory::ProcessorFactory(ServiceRegistry& registry)
    : m_registry(registry)
{
}

std::shared_ptr<IProcessor> ProcessorFactory::create(const QString& type)
{
    auto logger  = m_registry.mustGetAs<ILoggerService>("LoggerService");
    auto network = m_registry.mustGetAs<INetworkService>("NetworkService");
    auto imageprovider = m_registry.mustGetAs<IImageService>("ImageService");
    auto stepperservice = m_registry.mustGetAs<IStepperControllerService>("StepperService");

    if (type == "CameraOffsetProcessor") {
        return std::make_shared<CameraOffsetProcessor>(type,
                                                       std::move(logger),
                                                       std::move(network),
                                                       std::move(imageprovider),
                                                       std::move(stepperservice));
    }

    if (type == "PhotometricProcessor") {
        return std::make_shared<PhotometricProcessor>(type,
                                          std::move(logger),
                                          std::move(network),
                                          std::move(imageprovider),
                                          std::move(stepperservice));
    }

    if (type == "StereoCalibrationProcessor") {
        return std::make_shared<StereoCalibrationProcessor>(type,
                                        std::move(logger),
                                        std::move(network),
                                        std::move(imageprovider),
                                        std::move(stepperservice));
    }

    if (type == "UVAlignmentProcessor") {
        return std::make_shared<UVAlignmentProcessor>(type,
                                        std::move(logger),
                                        std::move(network),
                                        std::move(imageprovider),
                                        std::move(stepperservice));
    }
    throw std::runtime_error(QString("Unknown processor type: %1").arg(type).toStdString());
}



