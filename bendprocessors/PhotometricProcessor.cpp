#include "PhotometricProcessor.h"
#include <QDir>
#include "Photometric/photocalibrationworker.h"

using imaging::IImageService;

PhotometricProcessor::PhotometricProcessor(QString name,
            std::shared_ptr<ILoggerService> logger,
            std::shared_ptr<IServerProxyService> proxy,
            std::shared_ptr<IImageService> imageprovider,
            std::shared_ptr<IStepperControllerService> stepperService,
            const PhotometricParameters defaultParams,
            QObject* parent)
    :  BaseProcessor(name, std::move(logger),
            std::move(proxy),
            std::move(imageprovider),
            std::move(stepperService),
            parent),
            m_defaultParams(defaultParams){
    m_leftOverlay = new ShapeModel(this);
    m_rightOverlay = new ShapeModel(this);
    qDebug() << "PhotometricProcessor thread =" << thread();
}

void PhotometricProcessor::startCalibration(){
    auto* thread = new QThread(this);
    auto* worker = new PhotoCalibrationWorker(m_proxy,
            IsImageServiceSimulated(),
            uiMessage(),
            leftOverlay(),
            rightOverlay(),
            m_defaultParams);
    worker->moveToThread(thread);

    connect(thread,
            &QThread::started,
            worker,
            &PhotoCalibrationWorker::run);
    connect(worker,
            &PhotoCalibrationWorker::message,
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
        &PhotoCalibrationWorker::finished,
        this,
        [this](bool success) {
            m_busy = false;
            if (success)
                m_progress = 1.0;
            emit progressChanged();
            emit busyChanged();
        });
    connect(worker,
            &PhotoCalibrationWorker::finished,
            thread,
            &QThread::quit);
    connect(worker,
            &PhotoCalibrationWorker::finished,
            worker,
            &QObject::deleteLater);
    connect(thread,
            &QThread::finished,
            thread,
            &QObject::deleteLater);
    connect(
        worker,
        &PhotoCalibrationWorker::progressChanged,
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
        &PhotoCalibrationWorker::failed,
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
            &PhotoCalibrationWorker::statusChanged,
            this,
            [this](const QString& text)
            {
                if (m_statusMessage == text)
                    return;
                m_statusMessage = text;
                emit statusMessageChanged();
            });

    connect(worker,
            &PhotoCalibrationWorker::acquireImagePairRequested,
            m_imageService.get(),
            &IImageService::getImagePair,
            Qt::QueuedConnection);

    connect(m_imageService.get(),
            &IImageService::imagesReady,
            worker,
            &PhotoCalibrationWorker::onImagesReady,
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
            &PhotoCalibrationWorker::moveStepperRequested,
            m_stepperService.get(),
            &IStepperControllerService::moveToPosition,
            Qt::QueuedConnection);

    connect( m_stepperService.get(),
            &IStepperControllerService::moveCompleted,
            worker,
            &PhotoCalibrationWorker::onStepperMoveCompleted,
            Qt::QueuedConnection);

    connect(worker,
            &PhotoCalibrationWorker::homeStepperRequested,
            m_stepperService.get(),
            &IStepperControllerService::moveHome,
            Qt::QueuedConnection);

    connect( m_stepperService.get(),
            &IStepperControllerService::moveHomeCompleted,
            worker,
            &PhotoCalibrationWorker::onHomeMoveCompleted,
            Qt::QueuedConnection);

    connect(worker,
            &PhotoCalibrationWorker::resultsReady,
            this,
            &PhotometricProcessor::onResultsReady,
            Qt::QueuedConnection);

    qDebug() << "ImageService thread:"
             << m_imageService->thread();

    qDebug() << "Worker affinity:"
             << worker->thread();

    thread->start();
}

void PhotometricProcessor::execute(const QString& action) {
    resetProgress();
    m_resultStatus = ResultStatus::NotRun;
    emit resultStatusText();
    emit resultStatusTextChanged();
    startCalibration();
}


//tolerances need to be confirmed
void PhotometricProcessor::onResultsReady(PhotometricResults results) {
    qDebug() << ".............photometric results received";
    m_resultStatus = results.passed ? ResultStatus::Pass : ResultStatus::Fail;
    m_resultStatusText = resultStatusText();
    emit resultStatusTextChanged();
    emit passedChanged();
}

QString PhotometricProcessor::resultStatusText()
{
    switch (m_resultStatus) {
    case ResultStatus::NotRun:
        return "Not Run";
    case ResultStatus::Pass:
        return "Pass";
    case ResultStatus::Fail:
        return "Fail";
    }
    return {};
}

void PhotometricProcessor::resetProgress() {
    m_progress = 0.0;
    emit progressChanged();
}
