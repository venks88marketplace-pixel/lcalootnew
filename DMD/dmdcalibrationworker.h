#pragma once
#include <QObject>
#include "interfaces/IImageService.h"
#include "interfaces/IServerProxyService.h"
#include "dmdcalibrator.h"
#include "headers/calibrationStorage.h"
#include "DMD/DMDCalibrationResults.h"

using namespace imaging;

class DMDCalibrationWorker : public QObject
{
    Q_OBJECT

public:
    DMDCalibrationWorker(std::shared_ptr<IServerProxyService> proxyService, bool mockedImageService, QObject* parent = nullptr);

    QJsonObject getZpositionsObject( const std::string& calConfigParams );


    std::string m_savedImageFolder = "c:/calibration/apps/dmdCalibrationAnalysis";

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
    void resultsReady(DMDCalibrationResults results);

private:
    std::unique_ptr<DMDCalibrator> m_dmdCalibrator;
    std::shared_ptr<IServerProxyService> m_proxyService;

    State m_state{State::Idle};
    int m_current{0};
    cv::Mat combo;
    int m_index{0};
    std::vector<double> m_zPlanes;
    int m_totalSteps;
    calibrationStorage  m_calStore;
    DMDCalibrationResults m_results;
    bool m_mockedHW;
    bool m_movingToZero{false};

    const nirLedControls m_nirSettings{10, 10};
    const double m_leftGain{7.0}, m_rightGain{7.0};
    const int m_exposure{5000};
    const int m_uvPower{3000};
    const int m_uvDuration{300000}; // fixed 5 minutes
    const cameraTrigger m_triggerSettings{60,6};
    std::pair<uint32_t, uint32_t> m_imageDimensions{2016, 1452};

    void startStep();
    void moveToZero();
};

