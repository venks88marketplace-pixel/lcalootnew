#include "photocalibrationworker.h"
#include <interfaces/IServerProxyService.h>
#include "headers/calibrationStorage.h"
#include <fstream>
#include <QThread>
#include <QDir>
#include "utils.h"

PhotoCalibrationWorker::PhotoCalibrationWorker(
    std::shared_ptr<IServerProxyService> proxyService,
    bool mockedHW,
    UIMessageModel* uiMessageModel,
    ShapeModel* leftOverlay,
    ShapeModel* rightOverlay,
    const PhotometricParameters defaultParams,
    QObject* parent)
    : QObject(parent),
    m_defaultParams(defaultParams),
    m_proxyService(std::move(proxyService)),
    m_mockedHW(mockedHW),
    m_uiMessageModel(uiMessageModel),
    m_leftOverlay(leftOverlay),
    m_rightOverlay(rightOverlay){
        m_photoAnalyzer = std::make_unique<PhotometricAnalyzer>(parent);
        m_currentParams = m_defaultParams;
        m_testFilename = m_savedImageFolder + "/TEST_" + currentDateString() + ".txt";
        std::ofstream out(m_testFilename, std::ios::in | std::ios::out | std::ios::trunc);
        if (!out) {
            qFatal("Photometric: Could not open test file for writing!");
        }
        out << std::left << std::setw(8) << "iter"
            << std::right << std::setw(15) << "Peak LT"
            << std::right << std::setw(15) << "Peak LB"
            << std::right << std::setw(15) << "Gain L"
            << std::right << std::setw(15) << "Peak RT"
            << std::right << std::setw(15) << "Peak RB"
            << std::right << std::setw(15) << "Gain " << "\n";
        out << std::string(75, '-') << "\n";
        out.close();
}

void PhotoCalibrationWorker::run(){
    calibrationStorage  calstore;
    std::string dataRootPhotometric = calstore.calDataRoot().string()
                                      + "PhotometricCalibrationAnalysis//";
    m_photoAnalyzer->calDataRootPhotometric(dataRootPhotometric);

    m_iteration = 0;
    m_runNumber = 1;

    qDebug() << "Photometric calibration: starting run 1";
    m_maxIterationsInRange = static_cast<int>((m_currentParams.cameraGainRange.second -
                                               m_currentParams.cameraGainRange.first)/
                                       m_currentParams.gainIncrement);
    m_maxIterations = m_maxIterationsInRange * 2; //two full range iterations cap
    m_results.passed = false;

    if (!m_mockedHW) {
        int leftOffsetX,leftOffsetY, rightOffsetX,rightOffsetY;
        m_calStore.getOffsets(leftOffsetX, leftOffsetY, rightOffsetX, rightOffsetY);

        m_proxyService->setCameraROIs(
                    m_imageDimensions.first,
                    m_imageDimensions.second,
                    leftOffsetX,
                    leftOffsetY,
                    rightOffsetX,
                    rightOffsetY);

        m_proxyService->setNIR(
                    m_currentParams.topNir,
                    m_currentParams.bottomNir);

        m_proxyService->setCameraTrigger(true,
                    m_triggerSettings.frequency_hz,
                    m_triggerSettings.duration_ms);

        emit statusChanged(QString("Homing XYZ motors..."));
        m_proxyService->homeStageMotors(); //xyz motors
        //need to pause here for the motors to settle
        QThread::sleep(8);
    }

    if (fs::exists(m_photoAnalyzer->calDataRootPhotometric()) == false) {
        fs::create_directories(fs::path(m_photoAnalyzer->calDataRootPhotometric()));
    }
    emit statusChanged(QString("Homing Thorlabs stepper motor..."));
    emit progressChanged(1, 30);
    emit homeStepperRequested();
}

void PhotoCalibrationWorker::startIteration(){
    qDebug() << "Photometric iteration:" << m_iteration;
    //only gain values will be tuned
    double cameraGain = static_cast<double>(m_currentParams.cameraGainRange.first +
                                            m_currentParams.gainIncrement *  m_iteration);
    m_currentParams.leftCameraGain = m_currentParams.rightCameraGain = cameraGain;

    if (!m_mockedHW) {
        m_proxyService->setCameraGainsAndExposure(
            m_currentParams.leftCameraGain,
            m_currentParams.rightCameraGain,
            m_currentParams.cameraExposure);
    }
    emit moveStepperRequested(m_currentParams.zPosition_mm);
}

void PhotoCalibrationWorker::moveToZero() {
    m_movingToZero = true;
    emit statusChanged(QString("Moving motor to position 0 mm"));
    emit moveStepperRequested(0);
}

void PhotoCalibrationWorker::onStepperMoveCompleted() {
    qDebug() << "stepper move completed";
    if (!m_isStageSet){
        emit statusChanged(QString("Z stage set at: %1").arg(m_currentParams.zPosition_mm));
        m_isStageSet = true;
    }
    emit acquireImagePairRequested(imaging::IImageService::Caller::PhotometricCal, m_iteration);
    m_state = State::WaitingForImages;
}

void PhotoCalibrationWorker::onHomeMoveCompleted() {
    startIteration();
}

void PhotoCalibrationWorker::onImagesReady(QPixmap imageL, QPixmap imageR) {
    if (m_state != State::WaitingForImages)
        return;
    m_state = State::ProcessingImages;
    qDebug() << "Photometric: image ready received";
    emit statusChanged(QString("Processing images at iteration: %1").arg(m_iteration));
    int startPoint = m_runNumber == 1 ? (4 + m_iteration) : ( 4 + m_maxIterationsInRange + m_iteration);
    emit progressChanged(startPoint, m_maxIterations + 4);
    std::string fileNameBase = m_photoAnalyzer->currentDateString();

    cv::Mat limage = calibration::QPixmapToGrayMat(imageL);
    cv::Mat rimage = calibration::QPixmapToGrayMat(imageR);

    if (limage.rows == 0 || rimage.rows == 0) {
        qDebug() << "invalid images";
        return;
    }
    cv::Mat lRes =
        m_photoAnalyzer->evaluateIntensitySimilarity(limage, ImageSide::Left);
    cv::Mat rRes =
        m_photoAnalyzer->evaluateIntensitySimilarity(rimage, ImageSide::Right);

    overlayResults();

    auto getBalance = [](int a, int b) {
        int n = std::abs(a - b);
        return 100.0 * double(2*n) / double(a + b);
    };

    m_results.peakLeftTop = m_photoAnalyzer->m_topBotPeaksLeft.first;
    m_results.peakLeftBottom = m_photoAnalyzer->m_topBotPeaksLeft.second;
    m_results.peakRightTop = m_photoAnalyzer->m_topBotPeaksRight.first;
    m_results.peakRightBottom = m_photoAnalyzer->m_topBotPeaksRight.second;

    m_results.topBalance = static_cast<double>(getBalance(
        m_results.peakLeftTop,  m_results.peakRightTop));
    m_results.bottomBalance = static_cast<double>(getBalance(
        m_results.peakLeftBottom, m_results.peakRightBottom));
    m_results.leftBalance = static_cast<double>(getBalance(
        m_results.peakLeftTop, m_results.peakLeftBottom));
    m_results.rightBalance = static_cast<double>(getBalance(
        m_results.peakRightTop, m_results.peakRightBottom));

    bool res = evaluateResults(m_results);

    //for testing: record responses to gain change
    resultsToFile(m_testFilename);

    if (res) {
        updateCalFile();
        //save images
        emit resultsReady(std::move(m_results));
        emit statusChanged(QString("Moving to zero"));
        moveToZero();
        emit finished(true);
        return;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (++m_iteration >= m_maxIterationsInRange){
        if ( m_runNumber == 1 ){
            qDebug() << "Photometric calibration: starting run 2";
            ++m_runNumber;
            m_iteration = 0;
            m_currentParams.leftCameraGain = m_defaultParams.cameraGainRange.first;
            m_currentParams.rightCameraGain = m_defaultParams.cameraGainRange.first;
            m_iteration = 0;
            m_currentParams.topNir = m_defaultParams.topNir +
                                     m_currentParams.nirIncrement;
            m_currentParams.bottomNir = m_defaultParams.bottomNir +
                                     m_currentParams.nirIncrement;

            m_proxyService->setNIR(
                m_currentParams.topNir,
                m_currentParams.bottomNir);

            m_results.peakLeftTop = 0;
            m_results.peakLeftBottom = 0;
            m_results.peakRightTop = 0;
            m_results.peakRightBottom = 0;
            m_results.leftBalance = 0;
            m_results.rightBalance = 0;
        }
        else {
            qDebug() << "Photometric calibration failed after two runs.";
            emit statusChanged(QString("Moving to zero"));
            moveToZero();
            m_results.passed = false;
            emit resultsReady(std::move(m_results));
            emit finished(false);
            return;
        }
    }

    tuneCalibration();

    m_rightOverlay->clear();
    m_rightOverlay->clear();
    m_leftOverlay->clear();
    m_leftOverlay->clear();

    m_photoAnalyzer->m_polylineLT.clear();
    m_photoAnalyzer->m_polylineLB.clear();
    m_photoAnalyzer->m_polylineRT.clear();
    m_photoAnalyzer->m_polylineRB.clear();

    startIteration();
}

bool PhotoCalibrationWorker::evaluateResults(const PhotometricResults& results) {
    qDebug() << "evaluationg photometric results";
    int minIntensity = static_cast<int>(m_currentParams.acceptanceLeftRightPeakBracket.first * 1.06);
    int maxIntensity = m_currentParams.acceptanceLeftRightPeakBracket.second;

    if ((m_results.peakLeftTop > minIntensity
        && m_results.peakLeftTop < maxIntensity
        && m_results.topBalance <= m_currentParams.acceptanceLeftRightDiff) ||
        (m_results.peakRightTop > minIntensity
         && m_results.peakRightTop < maxIntensity
         && m_results.topBalance <= m_currentParams.acceptanceLeftRightDiff)){
        qDebug() << "------ Photometric cal succeeded!";
        m_results.passed = true;
    }
    return m_results.passed;
}

void PhotoCalibrationWorker::tuneCalibration() {
    int minIntensity = m_currentParams.acceptanceLeftRightPeakBracket.first;
    int maxIntensity = m_currentParams.acceptanceLeftRightPeakBracket.second;
    //match L-R
    if (m_results.topBalance > m_currentParams.acceptanceLeftRightDiff) {
        if (m_results.peakLeftTop > m_results.peakRightTop)
            m_currentParams.rightCameraGain += m_currentParams.gainIncrement;
        else
            m_currentParams.leftCameraGain += m_currentParams.gainIncrement;
    }
    //tune peak values
    else if (m_results.topBalance <= m_currentParams.acceptanceLeftRightDiff) {
        if (m_results.peakLeftTop < minIntensity) {
            m_currentParams.rightCameraGain += m_currentParams.gainIncrement;
            m_currentParams.leftCameraGain += m_currentParams.gainIncrement;
        }
        else if (m_results.peakLeftTop > maxIntensity) {
            m_currentParams.rightCameraGain -= m_currentParams.gainIncrement;
            m_currentParams.leftCameraGain -= m_currentParams.gainIncrement;
        }
    }
}
void PhotoCalibrationWorker::updateCalFile(){
    m_calStore.updateIRParameters(m_currentParams.topNir,
                                  m_currentParams.bottomNir);
    m_calStore.updatePhtometricGain(m_currentParams.leftCameraGain,
                                    m_currentParams.rightCameraGain,
                                    m_currentParams.cameraExposure);
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

std::string PhotoCalibrationWorker::currentDateString() {
    auto p = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(p);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::stringstream ss;
    ss << std::put_time(&tm,"%Y_%b_%d_%H_%M_%S");
    return ss.str();
}

void PhotoCalibrationWorker::overlayResults(){
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

    qDebug() << "LEFT shapes:" << m_leftOverlay->rowCount(QModelIndex());
    qDebug() << "RIGHT shapes:" << m_rightOverlay->rowCount(QModelIndex());

    //overlay results to UI
    QString leftTopRes = QStringLiteral("Left-Top  Max at: ") +
                         QString::number(m_photoAnalyzer->m_topBotPeaksLeft.first);
    m_leftOverlay->addLabel(2, 10, leftTopRes, QColor("white"), 3);
    QString leftBotRes = QStringLiteral("Left-Bot  Max at: ") +
                         QString::number(m_photoAnalyzer->m_topBotPeaksLeft.second);
    m_leftOverlay->addLabel(2, 686, leftBotRes, QColor("white"), 3);

    QString rightTopRes = QStringLiteral("Right-Top  Max at: ") +
                          QString::number(m_photoAnalyzer->m_topBotPeaksRight.first);
    m_rightOverlay->addLabel(2, 10, rightTopRes, QColor("white"), 3);
    QString rightBotRes = QStringLiteral("Right-Bot  Max at: ") +
                          QString::number(m_photoAnalyzer->m_topBotPeaksRight.second);
    m_rightOverlay->addLabel(2, 686, rightBotRes, QColor("white"), 3);
}

void PhotoCalibrationWorker::resultsToFile(std::string m_testFilename){
    std::ofstream out(m_testFilename, std::ios::app);
    out << std::left << std::setw(8) << m_iteration
        << std::right << std::setw(15) << m_results.peakLeftTop
        << std::right << std::setw(15) << m_results.peakLeftBottom
        << std::right << std::setw(15) << m_currentParams.leftCameraGain
        << std::right << std::setw(15) << m_results.peakRightTop
        << std::right << std::setw(15) << m_results.peakRightBottom
        << std::right << std::setw(15) << m_currentParams.rightCameraGain
        << "\n";
    out.close();
}
