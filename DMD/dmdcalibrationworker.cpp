#include "dmdcalibrationworker.h"
#include <QTimer>
#include <QtCore>
#include "utils.h"

DMDCalibrationWorker::DMDCalibrationWorker(
        std::shared_ptr<IServerProxyService> proxy,
        bool mockedHW,
        QObject* parent)
        : m_proxyService{proxy}, m_mockedHW{mockedHW}, QObject(parent)
{
    m_dmdCalibrator = std::make_unique<DMDCalibrator>(parent);
    qDebug() << "DMDCalibratorWorker CREATED";
}

void DMDCalibrationWorker::run(){

    if (!m_mockedHW) {
        int exposure;
        double leftGain, rightGain;
        int leftOffsetX,leftOffsetY, rightOffsetX,rightOffsetY;

        m_calStore.getOffsets(leftOffsetX, leftOffsetY, rightOffsetX, rightOffsetY);
        m_proxyService->setCameraROIs( m_imageDimensions.first, m_imageDimensions.second, leftOffsetX, leftOffsetY, rightOffsetX, rightOffsetY);
        m_proxyService->setNIR(m_nirSettings.top_intensity_percent, m_nirSettings.bottom_intensity_percent);
        m_proxyService->setCameraTrigger(true, m_triggerSettings.frequency_hz, m_triggerSettings.duration_ms);

        m_proxyService->setCameraGainsAndExposure( m_leftGain, m_rightGain, m_exposure);

        emit statusChanged(QString("Homing XYZ motors..."));
        m_proxyService->homeStageMotors();
        //need to pause here for the motors to settle
        QThread::sleep(8);
        m_proxyService->setUV(m_uvPower, m_uvDuration);
        m_proxyService->setDmdImage(QImage(":/images/dotImage_3_5x3.bmp"));
    }

    double z1 = 9.75;
    double z_increm = 0.25;
    for (int z = 0; z < 11; z++) {
        m_zPlanes.push_back( z1 + z * z_increm);
    }
    m_totalSteps = m_zPlanes.size() + 3;
    emit statusChanged(QString("Homing Thorlabs stepper motor..."));
    emit progressChanged(m_current,  m_totalSteps);
    emit homeStepperRequested();
}

void DMDCalibrationWorker::startStep() {
    emit statusChanged(QString("Moving motor to position %1").arg(m_zPlanes[m_index]));
    emit moveStepperRequested(m_zPlanes[m_index]);
}

void DMDCalibrationWorker::moveToZero() {
    m_movingToZero = true;
    emit statusChanged(QString("Moving motor to position 0 mm"));
    emit moveStepperRequested(0);
}

void DMDCalibrationWorker::onStepperMoveCompleted() {
    qDebug() << "stepper move completed";
    qDebug()  << "emmiting  acquireImagePairRequested";
    m_current++;
    if (m_movingToZero) {
        emit progressChanged(m_current, m_totalSteps);
        emit statusChanged(QString("Calibration completed."));
        emit resultsReady(std::move(m_results));
        emit finished(true);
    } else {
        emit progressChanged(m_current, m_totalSteps);
        emit acquireImagePairRequested(IImageService::Caller::DMDCal, m_index);
    }
}

void DMDCalibrationWorker::onHomeMoveCompleted() {
    qDebug() << "Homing stepper move completed";
    m_current++;
    emit progressChanged(m_current, m_totalSteps);
    emit statusChanged(QString("Home completed"));
    startStep();
}

void DMDCalibrationWorker::onImagesReady(QPixmap imageL,
                                         QPixmap imageR) {
    qDebug() << "image ready received";
    emit statusChanged(
        QString("Processing images for position: %1").arg(m_zPlanes[m_index]));

    cv::Mat limage = calibration::QPixmapToGrayMat(imageL);
    cv::Mat rimage = calibration::QPixmapToGrayMat(imageR);

    if (limage.rows == 0 || rimage.rows == 0) {
        qDebug() << "invalid images";
        return;
    }
    cv::Mat res = m_dmdCalibrator->collectAndSaveUVDots(
                        m_savedImageFolder,
                        limage,
                        rimage,
                        m_dmdCalibrator->calParams());
    if ( res.empty()){
        qDebug() << "Grid detection FAILED!";

        m_index = 0;
        m_results.uvDotsFound = false;
        m_results.dmdCalibrationStatus = "Fail";
        emit progressChanged(m_index,
                             m_totalSteps);
        moveToZero();
        return;
    }

    emit progressChanged(m_current,
                         m_totalSteps);
    ++m_index;
    if (m_index < m_zPlanes.size()){
        qDebug()
            << "emmiting  moveToPosition via startStep";
        startStep();
    }
    else{
        m_current++;
        emit progressChanged(m_current,
                             m_totalSteps);
        //train 3D step is very quick.
        emit statusChanged(QString("Homing stepper motor..."));
        m_dmdCalibrator->trainDMDCM(m_savedImageFolder,
                            m_dmdCalibrator->calParams());

        m_results.uvDotsFound = true;
        m_results.dmdCalibrationStatus = "Pass";

        emit progressChanged(m_current, m_totalSteps);
        moveToZero();
    }
}
