#include "StereoCalibrationProcessor.h"
#include "interfaces/IStepperControllerService.h"
#include "src/AppConfig.h"
#include <QTimer>
#include "src/stereocalibrationworker.h"
#include "neo-calibration-server-api/calibration_server_api.pb.h"
#include "src/services/imageservicemock.h"
#include <typeInfo>


using imaging::IImageService;
using namespace sps;

 StereoCalibrationProcessor::StereoCalibrationProcessor(QString name,
                                    std::shared_ptr<ILoggerService> logger,
                                    std::shared_ptr<IServerProxyService> proxy,
                                    std::shared_ptr<IImageService> imageService,
                                    std::shared_ptr<IStepperControllerService> stepperService,
                                    std::vector<double> zPositions,
                                    std::pair<double,double> checkerDimentions,
                                    cv::Size patternSize,
                                    double acceptanceCriterionRms,
                                    double acceptanceCriterionZ,
                                    bool verifyMode,
                                    QObject* parent)
    : BaseProcessor(name, std::move(logger),
                    std::move(proxy),
                    std::move(imageService),
                    std::move(stepperService),
                    parent),
                    m_zPositions{std::move(zPositions)},
                    m_trainCheckerDimensions{std::move(checkerDimentions)},
                    m_patternSize{std::move(patternSize)},
                    m_acceptanceCriterionRms{acceptanceCriterionRms},
                    m_acceptanceCriterionZ{acceptanceCriterionZ},
                    m_verifyMode{verifyMode} {
    m_leftOverlay = new ShapeModel(this);
    m_rightOverlay = new ShapeModel(this);
    qDebug() << "StereoCalibrationProcessor CREATED";

    qDebug() << "StereoCalibrationProcessor thread =" << thread();

 }

 void StereoCalibrationProcessor::execute(const QString& action) {
     //hardware setup
     resetProgress();
     m_calibrationStatus = ResultStatus::NotRun;
     emit calibrationStatusChanged();
     emit calibrationStatusTextChanged();
     startCalibration();
 }

 void StereoCalibrationProcessor::startCalibration(){

     auto* thread = new QThread(this);
     auto* worker = new StereoCalibrationWorker(m_proxy, IsImageServiceSimulated(), m_zPositions, m_trainCheckerDimensions, m_patternSize, m_verifyMode);

     worker->moveToThread(thread);

     connect(thread,
             &QThread::started,
             worker,
             &StereoCalibrationWorker::run);
     connect(worker,
             &StereoCalibrationWorker::message,
             this,
             [this](const QString& text,
                    QColor color,
                    QString level) {
                 m_uiMessage->addMessage(
                     text,
                     color,
                     "INFO");
             });
     connect(
         worker,
         &StereoCalibrationWorker::finished,
         this,
         [this](bool success) {
             m_busy = false;
             if (success)
                m_progress = 1.0;
             emit progressChanged(m_progress);
             emit busyChanged();
         });
     connect(worker,
             &StereoCalibrationWorker::finished,
             thread,
             &QThread::quit);
     connect(worker,
             &StereoCalibrationWorker::finished,
             worker,
             &QObject::deleteLater);
     connect(thread,
             &QThread::finished,
             thread,
             &QObject::deleteLater);
     connect(
         worker,
         &StereoCalibrationWorker::progressChanged,
         this,
         [this](int current, int total){
             if (total <= 0)
                 m_progress = 0.0;
             else
                 m_progress =
                     static_cast<double>(current) /
                     static_cast<double>(total);
             emit progressChanged(m_progress);
         });

     connect(
         worker,
         &StereoCalibrationWorker::failed,
         this,
         [this](const QString& error) {
             m_busy = false;
             m_uiMessage->addMessage(
                 error,
                 Qt::red,
                 "ERROR");
             emit busyChanged();
         });

     connect(worker,
             &StereoCalibrationWorker::statusChanged,
             this,
             [this](const QString& text)
             {
                 setStatusMessage(text);
             },
             Qt::QueuedConnection);

     connect(worker,
             &StereoCalibrationWorker::acquireImagePairRequested,
             m_imageService.get(),
             &IImageService::getImagePair,
             Qt::QueuedConnection);

     connect(m_imageService.get(),
             &IImageService::imagesReady,
             worker,
             &StereoCalibrationWorker::onImagesReady,
             Qt::QueuedConnection);

     connect(m_imageService.get(),
             &IImageService::imagesReady,
             m_imageService.get(),
             [](QPixmap imageL, QPixmap imageR)
             {
                 qDebug() << "TEST: imagesReady received in main thread"
                          << imageL.size()
                          << imageR.size();
             });

     connect(m_imageService.get(),
             &IImageService::imagesReady,
             worker,
             [](QPixmap imageL, QPixmap imageR)
             {
                 qDebug() << "TEST: imagesReady arrived in worker thread"
                          << QThread::currentThread()
                          << imageL.size()
                          << imageR.size();
             },
             Qt::QueuedConnection);

     connect(worker,
             &StereoCalibrationWorker::moveStepperRequested,
             m_stepperService.get(),
             &IStepperControllerService::moveToPosition,
             Qt::QueuedConnection);

     connect( m_stepperService.get(),
             &IStepperControllerService::moveCompleted,
             worker,
             &StereoCalibrationWorker::onStepperMoveCompleted,
             Qt::QueuedConnection);

     connect(worker,
             &StereoCalibrationWorker::homeStepperRequested,
             m_stepperService.get(),
             &IStepperControllerService::moveHome,
             Qt::QueuedConnection);

     connect( m_stepperService.get(),
             &IStepperControllerService::moveHomeCompleted,
             worker,
             &StereoCalibrationWorker::onHomeMoveCompleted,
             Qt::QueuedConnection);
     connect(worker,
             &StereoCalibrationWorker::resultsReady,
             this,
             &StereoCalibrationProcessor::onResultsReady,
             Qt::QueuedConnection);


     qDebug() << "ImageService thread:"
              << m_imageService->thread();

     qDebug() << "Worker affinity:"
              << worker->thread();

     thread->start();
}

static bool starts_with(const std::string& str, const std::string& prefix) {
    if (str.size() < prefix.size()) return false;
    return str.compare(0, prefix.size(), prefix) == 0;
}

void StereoCalibrationProcessor::onResultsReady(StereoCalibrationResults results) {
    qDebug() << ".............stereo calibration results received";
    bool passed = false;
    try {
        if(m_verifyMode) {
            passed = results.results.size() == (m_zPositions.size() * 3);
            double prevZ;
            int prevZplane = -1;
            for (const auto& [key, value] : results.results){
                if (starts_with(key, "VMS_")) {
                    passed &= std::abs(value - m_trainCheckerDimensions.second) <= m_acceptanceCriterionRms;
                } else if (starts_with(key, "HMS_")) {
                    passed &= std::abs(value - m_trainCheckerDimensions.first) <= m_acceptanceCriterionRms;
                } else if (starts_with(key, "Z_")) {
                    int zPlane = std::stoi(key.substr(2));
                    if(prevZplane != -1) {
                        passed &= (std::abs(prevZ - value - (zPlane - prevZplane)) <= m_acceptanceCriterionZ);
                    }
                    prevZ= value;
                    prevZplane = zPlane;
                }
            }
        }
        else {
            passed = results.results.at("AverageReprojectionError") <= m_acceptanceCriterionRms;
        }
    } catch(const std::exception& e ) {
        passed = false;
    }
    m_passed = passed;
    m_calibrationStatus = m_passed ? ResultStatus::Pass : ResultStatus::Fail;
    emit calibrationStatusTextChanged();
    emit passedChanged();
}

QString StereoCalibrationProcessor::calibrationStatusText() {
    switch (m_calibrationStatus) {
    case ResultStatus::NotRun:
        return "Not Run";
    case ResultStatus::Pass:
        return "Pass";
    case ResultStatus::Fail:
        return "Fail";
    }
    return {};
}

void StereoCalibrationProcessor::resetProgress() {
    m_progress = 0.0;
    emit progressChanged(m_progress);
}

