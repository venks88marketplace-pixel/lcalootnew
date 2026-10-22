#include "photocalibrationworker.h"
#include <interfaces/IServerProxyService.h>
#include "headers/calibrationStorage.h"
#include <QThread>
#include <QDir>
#include "utils.h"

PhotoCalibrationWorker::PhotoCalibrationWorker(
    std::shared_ptr<IServerProxyService> proxyService,
    bool mockedHW,
    UIMessageModel* uiMessageModel,
    ShapeModel* leftOverlay,
    ShapeModel* rightOverlay,
    QObject* parent)
    : QObject(parent),
    m_proxyService(std::move(proxyService)),
    m_mockedHW(mockedHW),
    m_uiMessageModel(uiMessageModel),
    m_leftOverlay(leftOverlay),
    m_rightOverlay(rightOverlay){
    m_photoAnalyzer = std::make_unique<PhotometricAnalyzer>(parent);
}

void PhotoCalibrationWorker::run(){

    calibrationStorage  calstore;
    std::string dataRootPhotometric = calstore.calDataRoot().string()
                                      + "PhotometricCalibrationAnalysis//";
    m_photoAnalyzer->calDataRootPhotometric(dataRootPhotometric);

    if (!m_mockedHW) {
        //hardware setup
        int exposure;
        double leftGain, rightGain;
        int leftOffsetX,leftOffsetY, rightOffsetX,rightOffsetY;

        m_calStore.getPhotometricParameters(exposure, leftGain, rightGain);
        m_calStore.getOffsets(leftOffsetX, leftOffsetY, rightOffsetX, rightOffsetY);
        m_proxyService->setCameraGainsAndExposure(leftGain, rightGain, exposure);
        m_proxyService->setCameraROIs( m_imageDimensions.first, m_imageDimensions.second, leftOffsetX, leftOffsetY, rightOffsetX, rightOffsetY);
        m_proxyService->setNIR(m_nirSettings.top_intensity_percent, m_nirSettings.bottom_intensity_percent);
        m_proxyService->setCameraTrigger(true, m_triggerSettings.frequency_hz, m_triggerSettings.duration_ms);
        emit statusChanged(QString("Homming XYZ motors..."));
        m_proxyService->homeStageMotors(); //xyz motors
        //need to pause here for the motors to settle
        QThread::sleep(8);
    }

    if (fs::exists(m_photoAnalyzer->calDataRootPhotometric()) == false) {
        fs::create_directories(fs::path(m_photoAnalyzer->calDataRootPhotometric()));
    }
    emit statusChanged(QString("Homming Thorlabs stepper motor..."));
    emit progressChanged(1,  3);
    emit homeStepperRequested();
}

void PhotoCalibrationWorker::startStep() {
    m_state = State::MovingStepper;
    emit statusChanged(QString("Moving motor to position %1 mm").arg(m_zPosition));
    emit moveStepperRequested(m_zPosition);
    emit progressChanged(2, 3);
}

void PhotoCalibrationWorker::moveToZero() {
    m_movingToZero = true;

    emit statusChanged(QString("Moving motor to position 0 mm"));
    emit moveStepperRequested(0);
}

void PhotoCalibrationWorker::onStepperMoveCompleted() {
    qDebug() << "stepper move completed";
    //only one image pair used in this task
    //index can be used for unit tests
    if(!m_movingToZero) {
        m_state = State::WaitingForImages;
        emit acquireImagePairRequested(imaging::IImageService::Caller::PhotometricCal, m_index);
    } else {
        emit statusChanged(QString("Calibration completed."));
         emit progressChanged(3, 3);
        emit resultsReady(std::move(m_results));
        m_state = State::Finished;
        emit finished(true);
    }
}

void PhotoCalibrationWorker::onHomeMoveCompleted() {
    startStep();
}

void PhotoCalibrationWorker::onImagesReady(QPixmap imageL, QPixmap imageR) {
    if (m_state != State::WaitingForImages)
        return;
    //m_state = State::ProcessingImages;

    qDebug() << "image ready received";
    emit statusChanged(QString("Processing images for position: %1").arg(m_zPosition));
    std::string fileNameBase = m_photoAnalyzer->currentDateString();

    cv::Mat limage = calibration::QPixmapToGrayMat(imageL);
    cv::Mat rimage = calibration::QPixmapToGrayMat(imageR);

    if (limage.rows == 0 || rimage.rows == 0) {
        qDebug() << "invalid images";
        return;
    }

    std::pair<int,int> leftImagePeaks;
    std::pair<int,int> rightImagePeaks;

    std::tuple<cv::Mat, std::pair<int,int>> lRes =
        m_photoAnalyzer->evaluateIntensitySimilarity(imageL, ImageSide::Left);
    leftImagePeaks = std::get<1>(lRes);

     std::tuple<cv::Mat, std::pair<int,int>> rRes =
         m_photoAnalyzer->evaluateIntensitySimilarity(imageR, ImageSide::Right);
     rightImagePeaks = std::get<1>(rRes);

     m_leftOverlay-> addPolyline(
          m_photoAnalyzer->m_polylineLT,
          qRgba(0,255,0,255), 4);

     m_leftOverlay-> addPolyline(
         m_photoAnalyzer->m_polylineLB,
         qRgba(0,255,0,255), 4);

     m_rightOverlay-> addPolyline(
         m_photoAnalyzer->m_polylineRT,
         qRgba(0,255,0,255), 4);

     m_rightOverlay-> addPolyline(
         m_photoAnalyzer->m_polylineRB,
         qRgba(0,255,0,255), 4);

     //overlay results to UI
     QString leftTopRes = QStringLiteral("Left-Top  Max at: ") + QString::number(leftImagePeaks.first);
     m_leftOverlay->addLabel(2, 10, leftTopRes, QColor("magenta"), 3);
     QString leftBotRes = QStringLiteral("Left-Bot  Max at: ") + QString::number(leftImagePeaks.second);
     m_leftOverlay->addLabel(2, 586, leftBotRes, QColor("magenta"), 3);

     QString rightTopRes = QStringLiteral("Right-Top  Max at: ") + QString::number(rightImagePeaks.first);
     m_rightOverlay->addLabel(20, 10, rightTopRes, QColor("magenta"), 3);
     QString rightBotRes = QStringLiteral("Right-Bot  Max at: ") + QString::number(rightImagePeaks.second);
     m_rightOverlay->addLabel(20, 586, rightBotRes, QColor("magenta"), 3);

     int rangeLow = 170;
     int rangeHight = 185;

     auto getBalance = [](int a, int b) {
            int n = std::abs(a - b);
            return 100.0 * double(2*n) / double(a + b);
     };

     m_results.peakLeftTop = leftImagePeaks.first;
     m_results.peakLeftBottom = leftImagePeaks.second;
     m_results.peakRightTop = rightImagePeaks.first;
     m_results.peakRightBottom = rightImagePeaks.second;

     m_results.topBalance = getBalance(leftImagePeaks.first, rightImagePeaks.first);
     m_results.bottomBalance = getBalance(leftImagePeaks.second, rightImagePeaks.second);;
     m_results.leftBalance = getBalance(leftImagePeaks.first, leftImagePeaks.second);
     m_results.rightBalance = getBalance(rightImagePeaks.first, rightImagePeaks.second);

     moveToZero();

}

void PhotoCalibrationWorker::updateCalFile(){

}

bool PhotoCalibrationWorker::qualifyImagePair(const cv::Mat& l, const cv::Mat& r) {
    return true;
}

void PhotoCalibrationWorker::storeImagePair( const std::string& path ) {

}

QImage PhotoCalibrationWorker::matToQImage(const cv::Mat &mat) {
    switch (mat.type()) {
    case CV_8UC1: {
        QImage img(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8);
        return img.copy();
    }
    case CV_8UC3: { // BGR -> RGB
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage img(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
        return img.copy();
    }
    case CV_8UC4: { // BGRA -> RGBA
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        QImage img(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step), QImage::Format_RGBA8888);
        return img.copy();
    }
    default:
        qWarning("Unsupported cv::Mat format for QImage conversion");
        return QImage();
    }
}
