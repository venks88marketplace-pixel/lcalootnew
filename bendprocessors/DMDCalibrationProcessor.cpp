#include "DMDCalibrationProcessor.h"
#include "../interfaces/IStepperControllerService.h"
#include "src/AppConfig.h"
#include <QTimer>
#include "neo-calibration-server-api/calibration_server_api.pb.h"
#include "src/services/imageservicemock.h"
#include "src/DMD/dmdcalibrationworker.h"
#include <QImage>
#include <typeInfo>

#include <fstream>
#include <vector>
#include <cstdint>
#include <winsock2.h>


using imaging::IImageService;

DMDCalibrationProcessor::DMDCalibrationProcessor(QString name,
                       std::shared_ptr<ILoggerService> logger,
                       std::shared_ptr<IServerProxyService> proxy,
                       std::shared_ptr<IImageService> imageService,
                       std::shared_ptr<IStepperControllerService> stepperService,
                       QObject* parent)
    : BaseProcessor(name, std::move(logger),
                    std::move(proxy),
                    std::move(imageService),
                    std::move(stepperService),
                    parent) {
    m_leftOverlay = new ShapeModel(this);
    m_rightOverlay = new ShapeModel(this);
    qDebug() << "DMDCalibrationProcessor CREATED";

    if (typeid(*m_imageService.get()) == typeid(imaging::ImageServiceMock))
        m_isImageServiceMock = true;

    qDebug() << "StereoCalibrationProcessor thread =" << thread();
}

void DMDCalibrationProcessor::startCalibration(){
    auto* thread = new QThread(this);
    auto* worker = new DMDCalibrationWorker(m_proxy, IsImageServiceSimulated());
    worker->moveToThread(thread);

    connect(thread,
            &QThread::started,
            worker,
            &DMDCalibrationWorker::run);
    connect(worker,
            &DMDCalibrationWorker::message,
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
        &DMDCalibrationWorker::finished,
        this,
        [this](bool success) {
            m_busy = false;
            if (success)
                m_progress = 1.0;
            else
                m_progress = 0.0;
            emit progressChanged();
            emit busyChanged();
        });
    connect(worker,
            &DMDCalibrationWorker::finished,
            thread,
            &QThread::quit);
    connect(worker,
            &DMDCalibrationWorker::finished,
            worker,
            &QObject::deleteLater);
    connect(thread,
            &QThread::finished,
            thread,
            &QObject::deleteLater);

    connect(
        worker,
        &DMDCalibrationWorker::progressChanged,
        this,
        [this](int current, int total){
            if (total <= 0)
                m_progress = 0.0;
            else
                m_progress =
                    static_cast<double>(current) /
                    static_cast<double>(total);
            emit progressChanged();
        });

    connect(
        worker,
        &DMDCalibrationWorker::failed,
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
        &DMDCalibrationWorker::statusChanged,
        this,
        [this](const QString& text)
        {
            if (m_statusMessage == text)
                return;
            m_statusMessage = text;
            emit statusMessageChanged();
        });
    connect(worker,
            &DMDCalibrationWorker::acquireImagePairRequested,
            m_imageService.get(),
            &IImageService::getImagePair,
            Qt::QueuedConnection);

    connect(m_imageService.get(),
            &IImageService::imagesReady,
            worker,
            &DMDCalibrationWorker::onImagesReady,
            Qt::QueuedConnection);
    connect(worker,
            &DMDCalibrationWorker::moveStepperRequested,
            m_stepperService.get(),
            &IStepperControllerService::moveToPosition,
            Qt::QueuedConnection);

    connect( m_stepperService.get(),
            &IStepperControllerService::moveCompleted,
            worker,
            &DMDCalibrationWorker::onStepperMoveCompleted,
            Qt::QueuedConnection);

    connect(worker,
            &DMDCalibrationWorker::homeStepperRequested,
            m_stepperService.get(),
            &IStepperControllerService::moveHome,
            Qt::QueuedConnection);

    connect( m_stepperService.get(),
            &IStepperControllerService::moveHomeCompleted,
            worker,
            &DMDCalibrationWorker::onHomeMoveCompleted,
            Qt::QueuedConnection);

    connect(worker,
            &DMDCalibrationWorker::resultsReady,
            this,
            &DMDCalibrationProcessor::onResultsReady,
            Qt::QueuedConnection);

    qDebug() << "ImageService thread:"
             << m_imageService->thread();

    qDebug() << "Worker affinity:"
             << worker->thread();

    thread->start();
}

void DMDCalibrationProcessor::execute(const QString& action) {
    startCalibration();
}

void DMDCalibrationProcessor::onResultsReady(DMDCalibrationResults results) {
    qDebug() << ".............dmd calibration results received";
    m_dmdCalibrationStatus =
        results.dmdCalibrationStatus.compare("Pass") == 0
        ? ResultStatus::Pass
        : ResultStatus::Fail;
    //final status is always a pass (completed).
    //Correctness of the results to be verified in a separate step.
    //If verification failes, workflow should go back to dmd calibration page.
    m_dmdCalibrationStatusText = calibrationStatusText();
    emit dmdCalibrationStatusTextChanged();
    //emit passedChanged();
}


QString DMDCalibrationProcessor::calibrationStatusText()
{
    switch (m_dmdCalibrationStatus) {
    case ResultStatus::NotRun:
        return "Not Run";
    case ResultStatus::Pass:
        return "Pass";
    case ResultStatus::Fail:
        return "Fail";
    }
    return {};
}
void DMDCalibrationProcessor::resetProgress(){
    m_progress = 0.0;
    emit progressChanged();
}
