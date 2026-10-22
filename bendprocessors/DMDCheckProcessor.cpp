#include "DMDCheckProcessor.h"
#include "interfaces/IStepperControllerService.h"
#include "src/AppConfig.h"
#include <QTimer>
#include "DMD/DMDCheckWorker.h"

using imaging::IImageService;

 DMDCheckProcessor::DMDCheckProcessor(QString name,
                                    std::shared_ptr<ILoggerService> logger,
                                    std::shared_ptr<IServerProxyService> proxy,
                                    std::shared_ptr<IImageService> imageService,
                                    std::shared_ptr<IStepperControllerService> stepperService,
                                    std::vector<double> zPositions,
                                    std::pair<double,double> checkerDimentions,
                                    cv::Size patternSize,
                                    double maxError,
                                    double maxError3d,
                                    QObject* parent)
    : BaseProcessor(name, std::move(logger),
                    std::move(proxy),
                    std::move(imageService),
                    std::move(stepperService),
                    parent),
                    m_zPositions{std::move(zPositions)},
                    m_trainCheckerDimensions{std::move(checkerDimentions)},
                    m_patternSize{std::move(patternSize)},
                    m_maxError{maxError},
                    m_maxError3d{maxError3d}
                    {
    m_leftOverlay = new ShapeModel(this);
    m_rightOverlay = new ShapeModel(this);
    qDebug() << "StereoCalibrationProcessor CREATED";

    qDebug() << "StereoCalibrationProcessor thread =" << thread();

 }

 void DMDCheckProcessor::configure(const QJsonObject& settings){
     m_saveImageFolder = settings.value("save_image_folder").toString("").toStdString();
 }

 void DMDCheckProcessor::execute(const QString& action) {
     //hardware setup
     resetProgress();
     m_calibrationStatus = ResultStatus::NotRun;
     emit calibrationStatusChanged();
     emit calibrationStatusTextChanged();
     startCalibration();
 }

 void DMDCheckProcessor::startCalibration(){

     auto* thread = new QThread(this);
     auto* worker = new DMDCheckWorker(m_proxy,
                                       IsImageServiceSimulated(),
                                       m_zPositions,
                                       m_trainCheckerDimensions,
                                       m_patternSize,
                                       m_saveImageFolder,
                                       m_maxError,
                                       m_maxError3d);

     worker->moveToThread(thread);

     connect(thread,
             &QThread::started,
             worker,
             &DMDCheckWorker::run);
     connect(worker,
             &DMDCheckWorker::message,
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
         &DMDCheckWorker::finished,
         this,
         [this](bool success) {
             m_busy = false;
             if (success)
                m_progress = 1.0;
             emit progressChanged(m_progress);
             emit busyChanged();
         });
     connect(worker,
             &DMDCheckWorker::finished,
             thread,
             &QThread::quit);
     connect(worker,
             &DMDCheckWorker::finished,
             worker,
             &QObject::deleteLater);
     connect(thread,
             &QThread::finished,
             thread,
             &QObject::deleteLater);
     connect(
         worker,
         &DMDCheckWorker::progressChanged,
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
         &DMDCheckWorker::failed,
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
             &DMDCheckWorker::statusChanged,
             this,
             [this](const QString& text)
             {
                 setStatusMessage(text);
             },
             Qt::QueuedConnection);

     connect(worker,
             &DMDCheckWorker::acquireImagePairRequested,
             m_imageService.get(),
             &IImageService::getImagePair,
             Qt::QueuedConnection);

     connect(m_imageService.get(),
             &IImageService::imagesReady,
             worker,
             &DMDCheckWorker::onImagesReady,
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
             &DMDCheckWorker::moveStepperRequested,
             m_stepperService.get(),
             &IStepperControllerService::moveToPosition,
             Qt::QueuedConnection);

     connect( m_stepperService.get(),
             &IStepperControllerService::moveCompleted,
             worker,
             &DMDCheckWorker::onStepperMoveCompleted,
             Qt::QueuedConnection);

     connect(worker,
             &DMDCheckWorker::homeStepperRequested,
             m_stepperService.get(),
             &IStepperControllerService::moveHome,
             Qt::QueuedConnection);

     connect( m_stepperService.get(),
             &IStepperControllerService::moveHomeCompleted,
             worker,
             &DMDCheckWorker::onHomeMoveCompleted,
             Qt::QueuedConnection);
     connect(worker,
             &DMDCheckWorker::resultsReady,
             this,
             &DMDCheckProcessor::onResultsReady,
             Qt::QueuedConnection);


     qDebug() << "ImageService thread:"
              << m_imageService->thread();

     qDebug() << "Worker affinity:"
              << worker->thread();

     thread->start();
 }


void DMDCheckProcessor::onResultsReady(bool result) {
    m_passed = result;
    m_calibrationStatus = m_passed ? ResultStatus::Pass : ResultStatus::Fail;
    emit calibrationStatusTextChanged();
    emit passedChanged();
}

QString DMDCheckProcessor::calibrationStatusText() {
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

void DMDCheckProcessor::resetProgress() {
    m_progress = 0.0;
    emit progressChanged(m_progress);
}

