cmake_minimum_required(VERSION 3.23)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(CalibrationTool VERSION 0.1 LANGUAGES CXX)

message(STATUS "Binary dir: ${CMAKE_BINARY_DIR}")
message(STATUS "Using Qt6_DIR=${Qt6_DIR}")

if(NOT "${CMAKE_CURRENT_BINARY_DIR}" MATCHES "build")
  message(FATAL_ERROR "You must call cmake from the build directory.
    CMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
   Check your 'git status' for artifacts cmake may have placed in unexpected #directories.")
endif()

set(CMAKE_CXX_EXTENSION OFF)
set(QT_QML_GENERATE_QMLLS_INI ON)
set(QML_IMPORT_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set(QML_IMPORT_TRACE ON)
set(CMAKE_AUTORCC ON)


#set(QT_CREATOR_SKIP_CONAN_SETUP ON)
set(pylon_DIR "C:/Program Files/Basler/pylon/Development/CMake/pylon")
set(THORLABS_KINESIS_DIR
    "C:/Program Files/Thorlabs/Kinesis"
    CACHE PATH "Thorlabs Kinesis installation directory"
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/headers)

find_package(Qt6 COMPONENTS Core Quick Gui SerialPort REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(OpenCV REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)
find_package(asio 1.32 CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(pylon 11.4 CONFIG REQUIRED)

message(STATUS "Protobuf_PROTOC_EXECUTABLE=${Protobuf_PROTOC_EXECUTABLE}")
message(STATUS "Protobuf_LIBRARIES=${Protobuf_LIBRARIES}")

#get_target_property(ASIO_INCLUDE_DIRS asio::asio INTERFACE_INCLUDE_DIRECTORIES)
#message(STATUS "Asio includes: ${ASIO_INCLUDE_DIRS}")

set(CALIBRATION_PROTO
    ${CMAKE_CURRENT_SOURCE_DIR}/neo-calibration-server-api/calibration_server_api.proto
)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appCalibrationTool
    main.cpp
    application.qrc
)

qt_add_qml_module(appCalibrationTool
    URI App
    VERSION 1.0
    QML_FILES
    qml/App.qml
        qml/NearLEDs.qml
        qml/CameraSettings.qml
        qml/UVControl.qml
        qml/StepMotor.qml
        qml/pages/DepthSensorCalibrationPage.qml
        qml/pages/DmdCalibrationPage.qml
        qml/pages/DmdCalibrationCheckPage.qml        
        qml/pages/HeadAssemblySelectionPage.qml
        qml/pages/HomePage.qml  
        qml/pages/InstrumentAxisCalibrationPage.qml
        qml/pages/OffsetCalibrationPage.qml
        qml/pages/NfovAssemblyPage.qml
        qml/pages/PhotometricCalibrationPage.qml
        qml/pages/StereoCalibration3mmPage.qml
        qml/pages/StereoCalibration4mmPage.qml
        qml/pages/UVBlockAssemblyPage.qml
        qml/pages/UVLaunchAssemblyPage.qml
        qml/pages/WfovAssemblyPage.qml
        qml/pages/MainTabsPage.qml
        qml/pages/MetaDataPage.qml
        qml/pages/SignInPage.qml
        qml/pages/UVIrradianceCalibrationPage.qml
        qml/tabs/TabCameraOffset.qml
        qml/tabs/TabPhotometric.qml
        qml/tabs/TabStereoCalibration.qml
        qml/tabs/TabDmdCalibration.qml
        qml/tabs/TabUVCalibration.qml
        qml/components/CameraControl.qml
        qml/components/CameraControlPanel.qml
        qml/components/ChevronButton.qml
        qml/components/NavButton.qml
        qml/components/LoggerConsole.qml
        qml/components/OverlayCanvas.qml
        qml/HardwareTrigger.qml
        qml/ImageControl2.qml
        qml/CustomTextField.qml

    SOURCES
        pixmapimage.cpp
        pixmapimage.h
        src/AppConfig.h
        src/services/imageservicemock.cpp
        src/services/imageservicemock.h
        interfaces/IImageService.h
        interfaces/IBaslerImageService.h
        interfaces/IService.h
        interfaces/INetworkService.h
        interfaces/IProcessor.h
        interfaces/ILoggerService.h
        interfaces/ISerialPortService.h
        interfaces/ISerialComm.h
        interfaces/IStepperControllerService.h
        interfaces/IServerProxyService.h
        utils.h
        src/configloader.h
        src/configloader.cpp
        src/core/appcontext.h
        src/core/appcontext.cpp
        src/core/servicebootstrap.h
        src/core/servicebootstrap.cpp
        src/core/serviceregistry.cpp
        src/core/serviceregistry.h
        src/services/networkservice.cpp
        src/services/networkservice.h
        src/services/AuthService.cpp
        src/services/AuthService.h
        src/services/LoggerService.cpp
        src/services/LoggerService.h
        src/services/nirledsettingsservice.h
        src/services/nirledsettingsservice.cpp
        src/services/camerasettingsservice.h
        src/services/camerasettingsservice.cpp
        src/services/triggersettingsservice.h
        src/services/triggersettingsservice.cpp
        src/services/uvsettingsservice.h
        src/services/uvsettingsservice.cpp
        src/services/NetworkServiceMock.h
        src/services/imageservice.h
        src/services/imageservice.cpp
        src/services/serialportservice.h
        src/services/serialportservice.cpp
        src/bendprocessors/UVCalibrationProcessor.h
        src/bendprocessors/UVCalibrationProcessor.cpp
        src/bendprocessors/CameraOffsetProcessor.cpp
        src/bendprocessors/CameraOffsetProcessor.h
        src/bendprocessors/PhotometricProcessor.cpp
        src/bendprocessors/PhotometricProcessor.h
        src/bendprocessors/BaseProcessor.cpp
        src/bendprocessors/BaseProcessor.h
        src/bendprocessors/StereoCalibrationProcessor.cpp
        src/bendprocessors/StereoCalibrationProcessor.h
        src/bendprocessors/UVAlignmentProcessor.cpp
        src/bendprocessors/UVAlignmentProcessor.h
        src/models/MetaData.cpp
        src/models/MetaData.h
        src/models/Session.cpp
        src/models/Session.h
        src/models/ShapeModel.cpp
        src/models/ShapeModel.h
        src/serialCommAsio.h
        src/serialCommAsio.cpp
        src/services/StepperKst101ZfsService.h
        src/services/StepperKst101ZfsService.cpp
        src/services/DepthStepperKst101ZfsService.h
        src/services/DepthStepperKst101ZfsService.cpp
        src/services/stepperservicemock.h
        src/services/stepperservicemock.cpp
        src/services/serverproxyservice.h
        src/services/serverproxyservice.cpp
        headers/calibrationStorage.h
        headers/spatialCalData.h
        headers/imageprocessor.h
        imageprocessor.cpp
        src/ImageSide.h
        src/models/UIMessageModel.h
        src/models/UIMessageModel.cpp
        src/services/OphirMeterService.cpp
        src/services/OphirMeterService.h
        src/bendprocessors/UVCalibrationProcessor.cpp
        src/bendprocessors/UVCalibrationProcessor.h
        src/bendprocessors/DMDCalibrationProcessor.h
        src/bendprocessors/DMDCalibrationProcessor.cpp
        src/DMD/dmdcalibrationworker.h
        src/DMD/dmdcalibrationworker.cpp
        src/Photometric/photocalibrationworker.h
        src/Photometric/photocalibrationworker.cpp
        src/Photometric/photometricanalyzer.h
        src/Photometric/photometricanalyzer.cpp
        src/Photometric/PhotometricResults.h
        src/Photometric/PhotometricParameters.h
        src/stereocameracalibrator.h
        src/stereocameracalibrator.cpp
        src/stereocalibrationworker.h
        src/stereocalibrationworker.cpp
        src/DMD/blobfinder.h
        src/DMD/blobfinder.cpp
        src/DMD/pointstogrid.h
        src/DMD/pointstogrid.cpp
        src/DMD/dmdcalibrator.h
        src/DMD/dmdcalibrator.cpp
        src/DMD/DMDCalibrationResults.h
        interfaces/IDepthSensorService.h
        src/services/depthsensorservice.h
        src/services/depthsensorservice.cpp
        src/StereoCalibrationResults.h
        src/DMD/DMDCalibrationResults.h
        src/wfov/WfovContrastProcessor.h
        src/wfov/WfovContrastProcessor.cpp
        src/services/BaslerImageService.cpp
        src/services/BaslerImageService.h
        src/services/UVBlockSetupService.cpp
        src/services/UVBlockSetupService.h

    RESOURCES
        assets/checkerboardL.png
        assets/checkerboardR.png
        assets/uvspottest1.png
        assets/uvspottestR.png
        assets/fih-left.png
        assets/fih-right.png
        assets/uvspottest1outlined.png
        application.qrc

        DEPENDENCIES QtQuick
        SOURCES src/bendprocessors/DMDCheckProcessor.h
        SOURCES src/DMD/DMDCheckWorker.h
        SOURCES src/bendprocessors/DMDCheckProcessor.cpp
        SOURCES src/DMD/DMDCheckWorker.cpp
        SOURCES src/DMD/dmdchecker.h src/DMD/dmdchecker.cpp
)

protobuf_generate(
    TARGET appCalibrationTool
    LANGUAGE cpp
    PROTOS
        ${CALIBRATION_PROTO}
)

target_include_directories(appCalibrationTool 
    PRIVATE 
        src
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_BINARY_DIR}
        ${THORLABS_KINESIS_DIR}
)

target_link_directories(appCalibrationTool
    PRIVATE

)

target_link_libraries(appCalibrationTool
  PRIVATE 
    Qt6::Quick 
    Qt6::Gui
    Qt6::SerialPort
    ${OpenCV_LIBS} 
    spdlog::spdlog 
    asio::asio 
    protobuf::protobuf 
    nlohmann_json::nlohmann_json
    pylon::pylon
    ${THORLABS_KINESIS_DIR}/Thorlabs.MotionControl.KCube.StepperMotor.lib
)
target_link_libraries(appCalibrationTool PRIVATE Qt6::Core)
target_link_libraries(appCalibrationTool PRIVATE Qt6::Core)

add_custom_command(TARGET appCalibrationTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${THORLABS_KINESIS_DIR}/Thorlabs.MotionControl.KCube.StepperMotor.dll
        ${THORLABS_KINESIS_DIR}/Thorlabs.MotionControl.DeviceManager.dll
        $<TARGET_FILE_DIR:appCalibrationTool>
    COMMENT "Copying Thorlabs Kinesis DLLs"
)

target_compile_definitions(appCalibrationTool PRIVATE QT_QML_DEBUG)
get_target_property(APP_INCLUDES appCalibrationTool INCLUDE_DIRECTORIES)
get_target_property(APP_INTERFACE_INCLUDES appCalibrationTool INTERFACE_INCLUDE_DIRECTORIES)

message(STATUS "appCalibrationTool includes = ${APP_INCLUDES}")
message(STATUS "appCalibrationTool interface includes = ${APP_INTERFACE_INCLUDES}")

include(GNUInstallDirs)
install(TARGETS appCalibrationTool
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

