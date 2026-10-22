#pragma once
#include "interfaces/IServerProxyService.h"
#include "interfaces/IImageService.h"
#include "photometricanalyzer.h"
#include "headers/calibrationStorage.h"
#include "models/UIMessageModel.h"
#include "models/ShapeModel.h"
#include "PhotometricResults.h"
#include "PhotometricParameters.h"

class PhotoCalibrationWorker : public QObject
{
    Q_OBJECT

public:
    PhotoCalibrationWorker(
        std::shared_ptr<IServerProxyService> proxyService,
        bool mockedImageService,
        UIMessageModel* uiMessage,
        ShapeModel* m_leftOverlay,
        ShapeModel* m_rightOverlay,
        const PhotometricParameters defaultParams,
        //const std::vector<double> m_cameraGainValues,
        QObject* parent = nullptr);

    void updateCalFile();
    bool qualifyImagePair(const cv::Mat& , const cv::Mat& );
    void storeImagePair( const std::string& path );

    QImage matToQImage(const cv::Mat &mat);

    std::string m_savedImageFolder = "c:/calibration/apps/PhotometricCalibrationAnalysis";
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
    void acquireImagePairRequested(imaging::IImageService::Caller, int);
    void moveStepperRequested(double);
    void homeStepperRequested();

    void message(QString text, QColor color, QString level);
    void progressChanged(int current, int total);
    void finished(bool success = false);
    void failed(QString text);
    void statusChanged(QString text);
    void resultsReady(PhotometricResults results);

private:
    void startIteration(); // new
    void processImages(const QImage& left, const QImage& right);
    void evaluateResults();
    bool evaluateResults(const PhotometricResults& results);
    std::string currentDateString();
    void overlayResults();
    void resultsToFile(std::string filename);
    void tuneCalibration();

private:
    std::shared_ptr<IServerProxyService> m_proxyService;
    std::unique_ptr<PhotometricAnalyzer> m_photoAnalyzer;
    int m_index{0};
    int m_current{0};

    int m_leftOffX,
        m_leftOffY,
        m_rightOffX,
        m_rightOffY;

    double m_zPosition{11}; //mm
    int m_totalSteps;
    calibrationStorage  m_calStore;
    bool m_homedStepperOnStart{false};
    UIMessageModel* m_uiMessageModel = std::nullptr_t();
    ShapeModel* m_leftOverlay;
    ShapeModel* m_rightOverlay;
    State m_state{State::Idle};
    bool m_doProcessing{true};
    bool m_mockedHW;
    bool m_movingToZero{false};
    bool m_passed{false};

    const cameraTrigger m_triggerSettings{60,6};

    const std::pair<uint32_t, uint32_t> m_imageDimensions{2016, 1452};

    int m_iteration{0};
    int m_maxIterationsInRange;
    int m_maxIterations;
    //two full range runs are allowed
    int m_runNumber{1};

    PhotometricParameters m_currentParams;
    PhotometricParameters m_defaultParams;
    PhotometricResults m_results;

    std::string m_testFilename;
    bool m_isStageSet{false};

    void moveToZero();

};
