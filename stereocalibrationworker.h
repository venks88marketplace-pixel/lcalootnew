#pragma once
#include <QObject>
#include "../interfaces/IImageService.h"
#include "../interfaces/IStepperControllerService.h"
#include "../interfaces/IServerProxyService.h"
#include "stereocameracalibrator.h"
#include "headers/calibrationStorage.h"
#include "StereoCalibrationResults.h"

using namespace imaging;
class StereoCalibrationWorker : public QObject
{
    Q_OBJECT

public:
    StereoCalibrationWorker(
        std::shared_ptr<IServerProxyService> proxyService,
        bool mockedHW,
        std::vector<double> zpositions,
        std::pair<double,double> checkerDimentions,
        cv::Size patternSize,
        bool verifyMode = false,
        QObject* parent = nullptr);

    void updateCalFile();
    bool qualifyImagePair(const cv::Mat& , const cv::Mat& );
    void storeImagePair( const std::string& path );



    QJsonObject getZpositionsObject( const std::string& calConfigParams );

    enum class State
    {
        Idle,
        MovingStepper,
        WaitingForImages,
        ProcessingImages,
        Finished,
        Error
    };

public slots:
    void run();
    void onImagesReady(QPixmap imageL, QPixmap imageR);
    void onStepperMoveCompleted();
    void onHomeMoveCompleted();

signals:
    void acquireImagePairRequested(IImageService::Caller, int);
    void moveStepperRequested(double);
    void homeStepperRequested();

    void message(QString text, QColor color, QString level);
    void progressChanged(int current, int total);
    void finished(bool success = false);
    void failed(QString text);
    void statusChanged(QString text);
    void resultsReady(StereoCalibrationResults results);

private:
    std::shared_ptr<IServerProxyService> m_proxyService;
    std::unique_ptr<StereoCameraCalibrator> m_stereoCalibrator;
    int m_current{0};
    cv::Mat combo;
    int m_index{0};
    std::vector<double> m_zPlanes;
    int m_totalSteps;
    calibrationStorage  m_calStore;
    bool m_homedStepperOnStart{false};
    State m_state{State::Idle};

    std::string m_savedImageFolder = "c:/calibration/stereoCalibrationAnalysis";
    std::pair<double,double> m_trainCheckerDimensions;
    bool m_verifyMode;
    bool m_movingToZero{false};
    bool m_mockedHW;


    const double m_leftGain{7.0}, m_rightGain{7.0};
    const nirLedControls m_nirSettings{30, 30};
    const cameraTrigger m_triggerSettings{60,6};
    const std::pair<uint32_t, uint32_t> m_imageDimensions{2016, 1452};

    void startStep();
    void moveToZero();

};

