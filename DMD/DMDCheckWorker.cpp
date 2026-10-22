#include <QThread>
#include "DMDCheckWorker.h"
#include <interfaces/IImageService.h>
#include "utils.h"
#include <iostream>
#include <fstream> // Required header for std::ofstream


void write(cv::FileStorage& fs, const std::string&, const PointData& data) {
    fs << "{"
       << "Z plane"        << data.m_plane
       << "CheckPointsIA"  << data.m_checkPointsIA
       << "CheckPointsDA"  << data.m_checkPointsDA
       << "Results"        << data.m_results
       << "Error 2D"       << data.m_results2dError
       << "Error 3D"       << data.m_results3dError
       << "}";
}

DMDCheckWorker::DMDCheckWorker(
    std::shared_ptr<IServerProxyService> proxyService,
    bool mockedHW,
    std::vector<double> zPositions,
    std::pair<double,double> checkerDimentions,
    cv::Size patternSize,
    std::string saveImageFolder,
    double maxError,
    double maxError3d,
    QObject* parent)
    : QObject(parent),
    m_proxyService(std::move(proxyService)),
    m_mockedHW{mockedHW},
    m_zPlanes{std::move(zPositions)},
    m_saveImageFolderBase{saveImageFolder},
    m_maxError{maxError},
    m_maxError3d{maxError3d},
    m_trainCheckerDimensions{checkerDimentions} {
    m_dmdChecker = std::make_unique<DMDChecker>(patternSize, parent);

    m_clahe = cv::createCLAHE(4.0, cv::Size(3,3));
    qDebug() << "StereoCalibratorWorker CREATED";
}

void DMDCheckWorker::run(){

    qDebug() << "run entered:"
             << QThread::currentThread();
    m_imageIndex = 0;
    int leftOffsetX,leftOffsetY, rightOffsetX,rightOffsetY;
    calibrationStorage calStore;
    calStore.getOffsets(leftOffsetX, leftOffsetY, rightOffsetX, rightOffsetY);
    if (!m_mockedHW) {
        m_proxyService->setCameraGainsAndExposure( m_leftGain, m_rightGain, m_exposureUsec);
        m_proxyService->setCameraROIs( m_imageDimensions.first, m_imageDimensions.second, leftOffsetX, leftOffsetY, rightOffsetX, rightOffsetY);
        m_proxyService->setNIR(m_nirSettings.top_intensity_percent, m_nirSettings.bottom_intensity_percent);
        m_proxyService->setCameraTrigger(true, m_triggerSettings.frequency_hz, m_triggerSettings.duration_ms);
        m_proxyService->setUVOff();

        emit statusChanged(QString("Homming XYZ motors..."));
        m_proxyService->homeStageMotors(); //xyz motors
        //need to pause here for the motors to settle
        QThread::sleep(9);
    }

    try{
        m_saveImageFolder = m_saveImageFolderBase / calibration::CurrentDateString();
        if (!std::filesystem::exists(m_saveImageFolder))
        {
            std::filesystem::create_directories(m_saveImageFolder);
        }
    }catch(std::filesystem::filesystem_error& ex){
        qCritical() << "Error creating image folder: " << m_saveImageFolder.string() << " " << ex.what();
    }

    m_totalSteps = m_zPlanes.size() * 6 + 3;
    emit statusChanged(QString("Homming Thorlabs stepper motor..."));
    emit progressChanged(m_current,  m_totalSteps);
    emit homeStepperRequested();
}

void DMDCheckWorker::startStep() {
    emit statusChanged(QString("Moving motor to position %1 mm").arg(m_zPlanes[m_index]));
    emit moveStepperRequested(m_zPlanes[m_index]);
}

void DMDCheckWorker::moveToZero() {
    m_movingToZero = true;

    emit statusChanged(QString("Moving motor to position 0 mm"));
    emit moveStepperRequested(0);
}

void DMDCheckWorker::onStepperMoveCompleted() {
    qDebug() << "stepper move completed";
    if (m_movingToZero) {
        m_current++;
        emit progressChanged(m_current, m_totalSteps);
        emit statusChanged(QString("Calibration Completed."));
        emit resultsReady(m_state != State::Error);
        emit finished(true);
    }else {
        emit acquireImagePairRequested(IImageService::Caller::DMDVerification,  m_imageIndex++);
        m_state = State::WaitingForCheckerboardImages;
    }
}

void DMDCheckWorker::onHomeMoveCompleted() {
    qDebug() << "Homing stepper move completed";
    m_current++;
    emit progressChanged(m_current, m_totalSteps);
    emit statusChanged(QString("Home completed"));
    startStep();
}

void DMDCheckWorker::onImagesReady(QPixmap imageL, QPixmap imageR) {

    qDebug() << "image ready received";
    cv::Mat limage = calibration::QPixmapToGrayMat(imageL);
    cv::Mat rimage = calibration::QPixmapToGrayMat(imageR);
    if (limage.rows == 0 || rimage.rows == 0) {
        qDebug() << "invalid images";
        m_state = State::Error;
    }

    try{

        std::ostringstream ss ;
        if (m_state == State::WaitingForCheckerboardImages) {
            ss << m_zPlanes[m_index] << "_checkers";
        }
        else if (m_state == State::WaitingForUVOnBlankImages) {
            ss << m_zPlanes[m_index] << "_blank";
        } else {
            ss << m_zPlanes[m_index] << "_" << m_checkPointIndex;
        }

        std::filesystem::path leftFname = m_saveImageFolder / (ss.str() + "-left.png");
        std::filesystem::path rightFname = m_saveImageFolder / (ss.str() + "-right.png");

        bool saveResult;
        saveResult = cv::imwrite(leftFname.string(), limage);
        if(!saveResult)
            qCritical() << "storing left image failed";
        saveResult = cv::imwrite(rightFname.string(), rimage);
        if(!saveResult)
            qCritical() << "storing right image failed";

        std::ofstream indexFile(m_saveImageFolder / "ImageList3D.csv", std::ios::app);
        indexFile << m_imageIndex << "," << ss.str() << std::endl;
        indexFile.close();

    }catch(...)
    {
        qCritical() << "Error saving files";
    }
    switch(m_state){
    case State::WaitingForCheckerboardImages:
    {
        emit statusChanged(QString("Processing images for position: %1").arg(m_zPlanes[m_index]));
        m_points.m_plane = m_zPlanes[m_index];
        cv::Mat left,right;
        //improve contrast
        m_clahe->apply(limage, left);
        m_clahe->apply(rimage, right);

        m_points.m_checkPointsIA = m_dmdChecker->findChckerboardAndCreateCheckPoints(left, right);
        if(m_points.m_checkPointsIA.size() > 0) {

            m_points.m_checkPointsDA = m_dmdChecker->convertPointsToDA(m_points.m_checkPointsIA);
            m_checkPointIndex = 0;

            QImage blank(1024, 768, QImage::Format_Grayscale8);
            blank.fill(0);

            m_proxyService->setUV(6000, 600000);
            m_state = State::WaitingForUVOnBlankImages;
            m_proxyService->setDmdImage(blank);
            m_proxyService->setNIR(0,0);
            QThread::sleep(1);

            emit acquireImagePairRequested(IImageService::Caller::DMDVerification,  m_imageIndex++);
        } else {
            qDebug() << "detected 0 points on checkerboard";
            m_proxyService->setUVOff();
            m_state = State::Error;
            emit statusChanged(QString("Moving to zero"));
            moveToZero();
        }
    }
    break;
    case State::WaitingForUVOnBlankImages:
    {
        m_leftNoUVImg = std::move(limage);
        m_rightNoUVImg = std::move(rimage);

        auto p = m_points.m_checkPointsDA[m_checkPointIndex];

        std::vector<cv::Point2f> point{p};
        auto img = m_dmdChecker->createImage(point, 3);

        m_state = State::WaitingForDMDImages;
        m_proxyService->setDmdImage(img);
        QThread::sleep(1);

        emit acquireImagePairRequested(IImageService::Caller::DMDVerification,  m_imageIndex++);

    }
    break;
    case State::WaitingForDMDImages:
    {
        auto left = calibration::stretch(limage - m_leftNoUVImg);
        auto right = calibration::stretch(rimage - m_rightNoUVImg);

        auto results = m_dmdChecker->detectUVSpots(left, right);
        if(results.size() == 1)
        {
            m_points.m_results.push_back(results[0]);

            qDebug() << "Detected UV point IA: [" << results[0].x << "," << results[0].y << "," << results[0].z << "]";
            cv::Point2f np{results[0].x, results[0].y};
            cv::Point2f op{m_points.m_checkPointsIA[m_checkPointIndex].x, m_points.m_checkPointsIA[m_checkPointIndex].y};
            auto diff2d = cv::norm(op - np);
            auto diff3d = cv::norm(m_points.m_checkPointsIA[m_checkPointIndex] - results[0]);
            m_points.m_results2dError.push_back(diff2d);
            m_points.m_results3dError.push_back(diff3d);

            qDebug() << "Point " << m_checkPointIndex <<  " at " << m_zPlanes[m_index] << " error 2d: " << diff2d << " error 3d: " << diff3d;

            if (diff2d > m_maxError || diff3d > m_maxError3d )
            {
                qDebug() << "Point " << m_checkPointIndex << " 2D error is " << diff2d << " 3D Error is " << diff3d;
                m_state = State::Error ;
                break;
            }
        }
        else{
            qDebug() << "Detected " << results.size() <<  " UV points instead of one";
            m_state = State::Error;
            emit statusChanged(QString("Moving to zero"));
            moveToZero();
            break;
        }
        m_checkPointIndex++;
        if (m_checkPointIndex < m_points.m_checkPointsIA.size()){
            auto p = m_points.m_checkPointsIA[m_checkPointIndex];
            qDebug() << "Projecting UV point IA: [" << p.x << "," << p.y << "," << p.z << "]";
            std::vector<cv::Point3f> point{p};
            auto checkPointsDA = m_dmdChecker->convertPointsToDA(point);
            qDebug() << "Projecting UV point DA: [" << checkPointsDA[0].x << "," << checkPointsDA[0].y << "]";
            auto img = m_dmdChecker->createImage(checkPointsDA, 3);
            m_proxyService->setDmdImage(img);
            QThread::sleep(1);
            emit acquireImagePairRequested(IImageService::Caller::DMDVerification,  m_imageIndex++);
        }
        else
        {
            m_results.push_back(m_points);
            ++m_index;
            if (m_index < m_zPlanes.size()){
                // next plane

                m_points.clear();
                m_proxyService->setUVOff();
                m_state = State::WaitingForCheckerboardImages;
                m_proxyService->setNIR(m_nirSettings.top_intensity_percent, m_nirSettings.bottom_intensity_percent);
                startStep();
            }
            else{
                saveResults();
                m_proxyService->setUVOff();
                m_state = State::Idle;
                emit statusChanged(QString("Moving to zero"));
                moveToZero();
            }
            break;
        }
    }

    default:
        break;
    }

    if (m_state == State::Error)
    {
        m_results.push_back(m_points);
        saveResults();
        m_proxyService->setUVOff();
        emit statusChanged(QString("Error: Moving to zero"));
        moveToZero();
    }

    m_current++;
    emit progressChanged(m_current,  m_totalSteps);
}


void DMDCheckWorker::saveResults(){
    auto resultsFile = m_saveImageFolder / "results.json";
    cv::FileStorage fs(resultsFile.string(), cv::FileStorage::FORMAT_JSON |  cv::FileStorage::WRITE );
    fs << "results" << m_results;
    fs.release();
}

void DMDCheckWorker::updateCalFile(){

}

bool DMDCheckWorker::qualifyImagePair(const cv::Mat& l, const cv::Mat& r) {
    return true;
}

void DMDCheckWorker::storeImagePair( const std::string& path ) {

}


