#pragma once
#include "headers/calibrationStorage.h"
#include "BaseProcessor.h"
#include "DMD/DMDCalibrationResults.h"
#include "interfaces/IServerProxyService.h"


class DMDCheckProcessor : public BaseProcessor {
    Q_OBJECT

    Q_PROPERTY(bool passed
                READ passed
                NOTIFY passedChanged)

    Q_PROPERTY(ResultStatus calibratinStatus
                READ calibrationStatus
                NOTIFY calibrationStatusChanged)

    Q_PROPERTY(QString calibrationStatusText
                READ calibrationStatusText
                NOTIFY calibrationStatusTextChanged)

public:
    explicit DMDCheckProcessor(QString name,
        std::shared_ptr<ILoggerService> logger,
        std::shared_ptr<IServerProxyService> proxyService,
        std::shared_ptr<imaging::IImageService> imageService,
        std::shared_ptr<IStepperControllerService> stepperService,
        std::vector<double> zPositions,
        std::pair<double,double> checkerDimentions,
        cv::Size patternSize,
        double maxError,
        double maxError3d,
        QObject* parent=nullptr);

    Q_PROPERTY(double progress
                READ progress
                NOTIFY progressChanged);

    Q_PROPERTY(QString statusMessage
                READ statusMessage
                WRITE setStatusMessage
                NOTIFY statusMessageChanged)

    Q_INVOKABLE void execute(const QString& action) override;
    Q_INVOKABLE void save() override {}
    Q_INVOKABLE void resetProgress();

    enum class ResultStatus {
        NotRun,
        Pass,
        Fail
    };
    Q_ENUM(ResultStatus)

    void startCalibration();

    double progress() const {
        return m_progress;
    }

    bool busy() const {
        return m_busy;
    }
    QString statusMessage() const {
        return m_statusMessage;
    }

    void setStatusMessage(const QString &statusMessage) {
        if (m_statusMessage != statusMessage) {
            m_statusMessage = statusMessage;
            emit statusMessageChanged(m_statusMessage);
        }
    }

    bool passed() const { return m_passed; }
    ResultStatus calibrationStatus() const { return m_calibrationStatus; }
    virtual void configure(const QJsonObject& settings) override ;

public slots:
    void onResultsReady( bool result);

signals:
    void progressChanged(double progress);
    void busyChanged();
    void message(QString text, QColor color, QString level);
    void statusMessageChanged(QString msg);

    void setSucceeded(const QString& message);
    void setFailed(const QString& message);

    void passedChanged();
    void calibrationStatusChanged();
    void calibrationStatusTextChanged();

private:
    QString calibrationStatusText();
    QJsonObject getZpositionsObject( const std::string& calConfigParams );
    std::string m_saveImageFolder;
    std::pair<double,double> m_trainCheckerDimensions;
    double m_progress{0.0};
    bool m_busy{false};
    QString m_statusMessage;
    bool m_passed = false;
    ResultStatus m_calibrationStatus;
    QString m_calibrationStatusText;

    calibrationStorage m_calStore;
    cv::Size m_patternSize;
    std::vector<double> m_zPositions;
    double m_maxError;
    double m_maxError3d;
};
