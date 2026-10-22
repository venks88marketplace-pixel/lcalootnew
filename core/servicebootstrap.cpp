#include "servicebootstrap.h"
#include "serviceregistry.h"
#include "services/LoggerService.h"
#include "services/NetworkServiceMock.h"
#include "../services/networkservice.h"
#include "../services/serverproxyservice.h"
#include "../services/AuthService.h"
#include "../services/imageservice.h"
#include "../services/imageservicemock.h"
#include "../services/camerasettingsservice.h"
#include "../services/nirledsettingsservice.h"
#include "../services/triggersettingsservice.h"
#include "../services/uvsettingsservice.h"
#include "../services/serialportservice.h"
#include "../services/StepperKst101ZfsService.h"
#include "../services/DepthStepperKst101ZfsService.h"
#include "../services/stepperservicemock.h"
#include "../services/BaslerImageService.h"
#include "../bendprocessors/CameraOffsetProcessor.h"
#include "../bendprocessors/PhotometricProcessor.h"
#include "../bendprocessors/StereoCalibrationProcessor.h"
#include "../bendprocessors/DMDCalibrationProcessor.h"
#include "../bendprocessors/UVCalibrationProcessor.h"
#include "../bendprocessors/UVAlignmentProcessor.h"
#include "../services/depthsensorservice.h"
#include "../interfaces/IDepthSensorService.h"
#include "../bendprocessors/DMDCheckProcessor.h"

using imaging::IImageService;
using imaging::ImageServiceMock;
using imaging::ImageService;
using namespace sps;

auto addTabProcessor =
    [](const QString& name, auto maker)
{
    ServiceRegistry::registerFactory(
        name,
        [maker](ServiceRegistry& reg, const QJsonObject&) -> ServiceRegistry::ServicePtr {
            auto logger  = reg.mustGetAs<LoggerService>("LoggingService");
            auto network = reg.mustGetAs<INetworkService>("NetworkService");
            return maker(logger, network);
        },
        {"LoggingService", "NetworkService"}
        );
};


std::shared_ptr<StereoCalibrationProcessor> initStereoCalProcessor(ServiceRegistry& reg, const QJsonObject& config, bool verifyMode)
{
    auto logger  = reg.mustGetAs<LoggerService>("LoggerService");
    auto proxy = reg.mustGetAs<IServerProxyService>("ServerProxyService");
    auto imageservice = reg.mustGetAs<IImageService>("ImageService");
    auto stepperservice = reg.mustGetAs<IStepperControllerService>("StepperControllerService");

    auto zPositionsArray = config.value("z_positions_mm").toArray();
    std::vector<double> zPositions;
    zPositions.reserve(zPositionsArray.size());
    for (const auto& item : zPositionsArray) {
        zPositions.push_back(item.toDouble()); // Use appropriate type conversion like .toInt(), .toString(), etc.
    }

    auto checkerSizeArray = config.value("checker_size_mm").toArray();
    std::pair<double,double> checkerSizePair{checkerSizeArray[0].toDouble(), checkerSizeArray[1].toDouble()};
    auto checkerbordDimensionsArray = config.value("checkerboard_dimensions").toArray();
    cv::Size checkerbordDimensions{checkerbordDimensionsArray[0].toInt(), checkerbordDimensionsArray[1].toInt()};
    auto acceptanceCriterion = config.value("acceptance_criterion_rms_tolerance").toDouble(0.175);
    auto acceptanceCriterionZ = config.value("acceptance_criterion_z_tolerance").toDouble(0.25);

    auto p = std::make_shared<StereoCalibrationProcessor>(
        "StereoCalibrationVerificationProcessor",
        std::move(logger),
        std::move(proxy),
        std::move(imageservice),
        std::move(stepperservice),
        zPositions,
        checkerSizePair,
        checkerbordDimensions,
        acceptanceCriterion,
        acceptanceCriterionZ,
        verifyMode
        );
    return p;
}


void ServiceBootstrap::registerFactories(const QJsonObject& config)
{
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    ServiceRegistry::registerFactory( "LoggerService",
     [](ServiceRegistry&, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
         return std::make_shared<LoggerService>(nullptr);
     });

    ServiceRegistry::registerFactory( "AuthService",
         [](ServiceRegistry&, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
             return std::make_shared<AuthService>(nullptr);
         });

    ServiceRegistry::registerFactory( "SerialPortService",
            [](ServiceRegistry&, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
            return std::make_shared<SerialPortService>(nullptr);
         });

    ServiceRegistry::registerFactory( "NetworkService",
         [](ServiceRegistry&, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
             const QString mode = config.value("mode").toString("real");
             if (mode == "mock") {
                 return std::make_shared<NetworkServiceMock>(nullptr);
             }
             return std::make_shared<NetworkService>(nullptr);
         });

    ServiceRegistry::registerFactory( "ServerProxyService",
                                     [](ServiceRegistry& reg, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
                                         qDebug() << "Creating ServerProxyService";
                                         auto logger  = reg.mustGetAs<LoggerService>("LoggerService");
                                         auto network = reg.mustGetAs<INetworkService>("NetworkService");
                                         return std::make_shared<ServerProxyService>(logger, network, nullptr);
                                     },
                                     {"NetworkService"}
                                     );

    ServiceRegistry::registerFactory( "ImageService",
         [](ServiceRegistry& reg, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
             qDebug() << "Creating ImageService";
             const QString mode = config.value("mode").toString("real");
             const auto folders = config.value("folders").toObject().toVariantMap();
             if (mode == "mock") {
                 return std::make_shared<ImageServiceMock>(folders, nullptr);
             }
             auto network = reg.mustGetAs<INetworkService>("NetworkService");
             return std::make_shared<ImageService>(network, nullptr);
         },
         {"NetworkService"}
         );

    ServiceRegistry::registerFactory( "StepperControllerService",
         [](ServiceRegistry& reg, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
             qDebug() << "Creating StepperControllerService";
             const QString mode = config.value("mode").toString("real");
             if (mode == "mock") {
                 return std::make_shared<StepperServiceMock>(nullptr);
             }
             auto serialport = reg.mustGetAs<ISerialPortService>("SerialPortService");
             const QJsonObject cfg = config.value("config").toObject();
             if (!cfg.contains("portName")) {
                 throw std::runtime_error("StepperControllerService: missing config.portName");
             }
             const QString portName = cfg.value("portName").toString();
             auto thread = new QThread();
             auto service = std::make_shared<StepperKst101ZfsService>(portName.toStdString(),  std::move(serialport));
             service->moveToThread(thread);
             thread->start();
             return service;
         });

    ServiceRegistry::registerFactory( "DepthStepperControllerService",
         [](ServiceRegistry& reg, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
             qDebug() << "Creating DepthStepperControllerService";
             const QString mode = config.value("mode").toString("real");
             if (mode == "mock") {
                 return std::make_shared<StepperServiceMock>(nullptr);
             }
             auto serialport = reg.mustGetAs<ISerialPortService>("SerialPortService");
             const QJsonObject cfg = config.value("config").toObject();
             if (!cfg.contains("portName")) {
                 throw std::runtime_error("DepthStepperControllerService: missing config.portName");
             }
             const QString portName = cfg.value("portName").toString();
             return std::make_shared<DepthStepperKst101ZfsService>(portName.toStdString(),  std::move(serialport));
         });

    ServiceRegistry::registerFactory(
        "CameraSettingsService",
        [](ServiceRegistry& reg, const QJsonObject&) -> ServiceRegistry::ServicePtr {
            qDebug() << "Factory: CameraSettingsService";
            qDebug() << "Registry currently has:" << reg.instanceNames();
            auto network = reg.mustGetAs<INetworkService>("NetworkService");
            qDebug() << "Resolved NetworkService OK";
            auto svc = std::make_shared<CameraSettingsService>(std::move(network));
            return std::static_pointer_cast<IService>(svc);
        },
        {"NetworkService"}
        );

    ServiceRegistry::registerFactory(
        "NIRLedSettingsService",
        [](ServiceRegistry& reg, const QJsonObject&) -> ServiceRegistry::ServicePtr {
            qDebug() << "Factory: NIRLedSettingsService";
            qDebug() << "Registry currently has:" << reg.instanceNames();
            auto network = reg.mustGetAs<INetworkService>("NetworkService");
            qDebug() << "Resolved NetworkService OK";
            auto svc = std::make_shared<NIRLedSettingsService>(std::move(network));
            return std::static_pointer_cast<IService>(svc);
        },
        {"NetworkService"}
        );

    ServiceRegistry::registerFactory(
        "TriggerSettingsService",
        [](ServiceRegistry& reg, const QJsonObject&) -> ServiceRegistry::ServicePtr {
            qDebug() << "Factory: TriggerSettingsService";
            qDebug() << "Registry currently has:" << reg.instanceNames();
            auto network = reg.mustGetAs<INetworkService>("NetworkService");
            qDebug() << "Resolved NetworkService OK";
            auto svc = std::make_shared<TriggerSettingsService>(std::move(network));
            return std::static_pointer_cast<IService>(svc);
        },
        {"NetworkService"}
        );

    ServiceRegistry::registerFactory(
        "UVSettingsService",
        [](ServiceRegistry& reg, const QJsonObject&) -> ServiceRegistry::ServicePtr {
            qDebug() << "Factory: UVSettingsService";
            qDebug() << "Registry currently has:" << reg.instanceNames();
            auto network = reg.mustGetAs<INetworkService>("NetworkService");
            qDebug() << "Resolved NetworkService OK";
            auto svc = std::make_shared<UVSettingsService>(std::move(network));
            return std::static_pointer_cast<IService>(svc);
        },
        {"NetworkService"}
        );

    ServiceRegistry::registerFactory(
        "CameraOffsetProcessor",
        [](ServiceRegistry& reg, const QJsonObject&) -> ServiceRegistry::ServicePtr {
            auto logger  = reg.mustGetAs<LoggerService>("LoggerService");
            auto proxy = reg.mustGetAs<IServerProxyService>("ServerProxyService");
            auto imageservice = reg.mustGetAs<IImageService>("ImageService");
            auto stepperservice = reg.mustGetAs<IStepperControllerService>("StepperControllerService");
            return std::make_shared<CameraOffsetProcessor>("CameraOffsetProcessor",
                    std::move(logger),  std::move(proxy), std::move(imageservice), std::move(stepperservice));
        },
        {"LoggerService", "ServerProxyService", "ImageService", "StepperControllerService"}
        );

    ServiceRegistry::registerFactory(
        "PhotometricProcessor",
        [](ServiceRegistry& reg, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
            const QJsonObject paramsObj =
                config.value("PhotometricParams").toObject();
            const PhotometricParameters params =
                PhotometricParameters::fromJson(paramsObj);
            auto logger  = reg.mustGetAs<LoggerService>("LoggerService");
            auto proxy = reg.mustGetAs<IServerProxyService>("ServerProxyService");
            auto imageservice = reg.mustGetAs<IImageService>("ImageService");
            auto stepperservice = reg.mustGetAs<IStepperControllerService>("StepperControllerService");
            return std::make_shared<PhotometricProcessor>("PhotometricProcessor",
                    std::move(logger),  std::move(proxy), std::move(imageservice), std::move(stepperservice), params);
        },
        {"LoggerService", "ServerProxyService", "ImageService", "StepperControllerService"}
        );

    ServiceRegistry::registerFactory(
        "StereoCalibrationProcessor",
        [](ServiceRegistry& reg, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
            auto p = initStereoCalProcessor(reg, config, false);

            // DEBUG CHECKS
            auto asService = std::static_pointer_cast<IService>(p);
            auto asProcessor = std::dynamic_pointer_cast<IProcessor>(asService);

            if (!asProcessor) {
                qFatal("StereoCalibrationProcessor is NOT castable to IProcessor!");
            }
            return asService;
        },
        {"LoggerService", "ServerProxyService", "ImageService", "StepperControllerService", "ServerProxyService"}
        );

    ServiceRegistry::registerFactory(
        "StereoCalibrationVerificationProcessor",
        [](ServiceRegistry& reg, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
            auto p = initStereoCalProcessor(reg, config, true);
            // DEBUG CHECKS
            auto asService = std::static_pointer_cast<IService>(p);
            auto asProcessor = std::dynamic_pointer_cast<IProcessor>(asService);

            if (!asProcessor) {
                qFatal("StereoCalibrationProcessor is NOT castable to IProcessor!");
            }

            return asService;
        },
        {"LoggerService", "ServerProxyService", "ImageService", "StepperControllerService"}
        );

    ServiceRegistry::registerFactory(
        "DMDCalibrationProcessor",
        [](ServiceRegistry& reg, const QJsonObject&) -> ServiceRegistry::ServicePtr {
            auto logger  = reg.mustGetAs<LoggerService>("LoggerService");
            auto proxy = reg.mustGetAs<IServerProxyService>("ServerProxyService");
            auto imageservice = reg.mustGetAs<IImageService>("ImageService");
            auto stepperservice = reg.mustGetAs<IStepperControllerService>("StepperControllerService");
            auto p = std::make_shared<DMDCalibrationProcessor>(
                "DMDCalibrationProcessor",
                std::move(logger),
                std::move(proxy),
                std::move(imageservice),
                std::move(stepperservice)
                );

            // DEBUG CHECKS
            auto asService = std::static_pointer_cast<IService>(p);
            auto asProcessor = std::dynamic_pointer_cast<IProcessor>(asService);

            if (!asProcessor) {
                qFatal("DMDCalibrationProcessor is NOT castable to IProcessor!");
            }

            return asService;
        },
        {"LoggerService", "ServerProxyService", "ImageService", "StepperControllerService", "ServerProxyService"}
        );

    ServiceRegistry::registerFactory(
        "DMDCheckProcessor",
        [](ServiceRegistry& reg, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
            auto logger  = reg.mustGetAs<LoggerService>("LoggerService");
            auto proxy = reg.mustGetAs<IServerProxyService>("ServerProxyService");
            auto imageservice = reg.mustGetAs<IImageService>("ImageService");
            auto stepperservice = reg.mustGetAs<IStepperControllerService>("StepperControllerService");

            auto zPositionsArray = config.value("z_positions_mm").toArray();
            std::vector<double> zPositions;
            zPositions.reserve(zPositionsArray.size());
            for (const auto& item : zPositionsArray) {
                zPositions.push_back(item.toDouble()); // Use appropriate type conversion like .toInt(), .toString(), etc.
            }

            auto checkerSizeArray = config.value("checker_size_mm").toArray();
            std::pair<double,double> checkerSizePair{checkerSizeArray[0].toDouble(), checkerSizeArray[1].toDouble()};
            auto checkerbordDimentionsArray = config.value("checkerboard_dimentions").toArray();
            cv::Size checkerbordDimentions{checkerbordDimentionsArray[0].toInt(), checkerbordDimentionsArray[1].toInt()};
            auto maxError = config.value("max_error").toDouble(0.175);
            auto maxError3d = config.value("max_error_3d").toDouble(0.25);

            auto p = std::make_shared<DMDCheckProcessor>(
                "StereoCalibrationVerificationProcessor",
                std::move(logger),
                std::move(proxy),
                std::move(imageservice),
                std::move(stepperservice),
                zPositions,
                checkerSizePair,
                checkerbordDimentions,
                maxError,
                maxError3d
                );

            // 🔍 DEBUG CHECKS
            auto asService = std::static_pointer_cast<IService>(p);
            auto asProcessor = std::dynamic_pointer_cast<IProcessor>(asService);

            if (!asProcessor) {
                qFatal("StereoCalibrationProcessor is NOT castable to IProcessor!");
            }

            return asService;
        },
        {"LoggerService", "ServerProxyService", "ImageService", "StepperControllerService", "ServerProxyService"}
        );


    ServiceRegistry::registerFactory(
        "DepthSensorService",
        [](ServiceRegistry& reg, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
            auto network = reg.mustGetAs<INetworkService>("NetworkService");
            auto stepper = reg.mustGetAs<IStepperControllerService>("DepthStepperControllerService");
            auto svc = std::make_shared<DepthSensorService>(std::move(network), std::move(stepper));
            svc->configure(config.value("config").toObject());
            return std::static_pointer_cast<IService>(svc);
        },
        {"NetworkService", "DepthStepperControllerService"}
    );

    ServiceRegistry::registerFactory(
        "UVAlignmentProcessor",
        [](ServiceRegistry& reg, const QJsonObject&) -> ServiceRegistry::ServicePtr {
            auto logger  = reg.mustGetAs<LoggerService>("LoggerService");
            auto proxy = reg.mustGetAs<IServerProxyService>("ServerProxyService");
            auto imageservice = reg.mustGetAs<IImageService>("ImageService");
            auto stepperservice = reg.mustGetAs<IStepperControllerService>("StepperControllerService");
            auto p = std::make_shared<UVAlignmentProcessor>(
                "UVAlignmentProcessor",
                std::move(logger),
                std::move(proxy),
                std::move(imageservice),
                std::move(stepperservice)
                );

            // DEBUG CHECKS
            auto asService = std::static_pointer_cast<IService>(p);
            auto asProcessor = std::dynamic_pointer_cast<IProcessor>(asService);

            if (!asProcessor) {
                qFatal("UVAlignmentProcessor is NOT castable to IProcessor!");
            }

            return asService;
        },
        {"LoggerService", "ServerProxyService", "ImageService", "StepperControllerService", "ServerProxyService"}
    );

    ServiceRegistry::registerFactory( "BaslerImageService",
        [](ServiceRegistry& reg, const QJsonObject& config) -> ServiceRegistry::ServicePtr {
            qDebug() << "Creating BaslerImageService";
            auto svc = std::make_shared<imaging::BaslerImageService>(nullptr);
            svc->configure(config.value(QStringLiteral("camera_config")).toObject());
            return svc;
        }
    );
}
