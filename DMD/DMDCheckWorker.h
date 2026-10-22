#pragma once
#include <QObject>
#include "../interfaces/IImageService.h"
#include "../interfaces/IServerProxyService.h"
#include "DMD/dmdchecker.h"
#include "headers/calibrationStorage.h"
#include "DMD/DMDCalibrationResults.h"

using namespace imaging;

struct PointData{
    double m_plane;
    std::vector<cv::Point3f> m_checkPointsIA;
    std::vector<cv::Point2f> m_checkPointsDA;
    std::vector<cv::Point3f> m_results;
    std::vector<double> m_results2dError;
    std::vector<double> m_results3dError;

    void clear(){
        m_checkPointsIA.clear();
        m_checkPointsDA.clear();
        m_results.clear();
        m_results2dError.clear();
        m_results3dError.clear();
    }
};

class DMDCheckWorker : public QObject
{
    Q_OBJECT

public:
    DMDCheckWorker(
        std::shared_ptr<IServerProxyService> proxyService,
        bool mockedHW,
        std::vector<double> zpositions,
        std::pair<double,double> checkerDimentions,
        cv::Size patternSize,
        std::string savedImageFolder,
        double maxError,
        double maxError3d,
        QObject* parent = nullptr);

    void updateCalFile();
    bool qualifyImagePair(const cv::Mat& , const cv::Mat& );
    void storeImagePair( const std::string& path );

    QJsonObject getZpositionsObject( const std::string& calConfigParams );

    enum class State
    {
        Idle,
        WaitingForCheckerboardImages,
        WaitingForUVOnBlankImages,
        WaitingForDMDImages,
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
    void resultsReady(bool success);

private:
    std::shared_ptr<IServerProxyService> m_proxyService;
    std::unique_ptr<DMDChecker> m_dmdChecker;
    int m_current{0};
    cv::Mat m_leftNoUVImg, m_rightNoUVImg;
    int m_index{0};
    int m_imageIndex{0};
    std::vector<double> m_zPlanes;
    int m_totalSteps;
    State m_state{State::Idle};
    double m_maxError, m_maxError3d;

    std::filesystem::path m_saveImageFolderBase;
    std::filesystem::path m_saveImageFolder;
    std::pair<double,double> m_trainCheckerDimensions;
    bool m_movingToZero{false};
    bool m_mockedHW;
    int m_checkPointIndex;
    PointData m_points;
    cv::Ptr<cv::CLAHE> m_clahe;
    std::vector<PointData> m_results;

    const double m_leftGain{7.0}, m_rightGain{7.0};
    const double m_exposureUsec{5000.0};
    const nirLedControls m_nirSettings{30, 30};
    const cameraTrigger m_triggerSettings{60,6};
    const std::pair<uint32_t, uint32_t> m_imageDimensions{2016, 1452};

    void startStep();
    void moveToZero();
    void saveResults();
};

