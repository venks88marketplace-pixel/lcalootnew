#pragma once
#include <QObject>
#include "headers/calibrationStorage.h"
#include "BaseProcessor.h"
#include "DMD/DMDCalibrationResults.h"
#include <winsock2.h>
#include <cstdint>

class DMDCalibrationProcessor : public BaseProcessor {
    Q_OBJECT

    // Q_PROPERTY(bool passed
    //             READ passed
    //             NOTIFY passedChanged)

    Q_PROPERTY(ResultStatus dmdCalibrationStatus
                    READ dmdCalibrationStatus
                    NOTIFY dmdCalibrationStatusChanged)

    Q_PROPERTY(QString dmdCalibrationStatusText
                    READ dmdCalibrationStatusText
                    NOTIFY dmdCalibrationStatusTextChanged)
public:
    explicit DMDCalibrationProcessor(QString name,
                std::shared_ptr<ILoggerService> logger,
                std::shared_ptr<IServerProxyService> proxyService,
                std::shared_ptr<imaging::IImageService> imageService,
                std::shared_ptr<IStepperControllerService> stepperService,
                QObject* parent=nullptr);

    Q_PROPERTY(double progress
                READ progress
                NOTIFY progressChanged);

    Q_PROPERTY(QString statusMessage
                READ statusMessage
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
            emit setStatusMessage(m_statusMessage);
        }
    }

    //bool passed() const { return m_passed; }
    ResultStatus dmdCalibrationStatus() const { return m_dmdCalibrationStatus; }
    QString dmdCalibrationStatusText() const { return m_dmdCalibrationStatusText; }

public slots:
    void onResultsReady( DMDCalibrationResults results);

signals:
    void progressChanged();
    void busyChanged();
    void message(QString text, QColor color, QString level);
    void statusMessageChanged();

    void setSucceeded(const QString& message);
    void setFailed(const QString& message);
    void xyzMotorsHomed();

    //void passedChanged();
    void dmdCalibrationStatusChanged();
    void dmdCalibrationStatusTextChanged();

private:
    void sendImageToDmd();
    QString calibrationStatusText();
    //std::string m_savedImageFolder="c:/calibration/apps/dmdCalibrationAnalysis";
    double m_progress{0.0};
    bool m_busy{false};
    QString m_statusMessage;
    ResultStatus m_dmdCalibrationStatus;
    QString m_dmdCalibrationStatusText;


    bool m_isHWConfigured{false};
    bool m_isImageServiceMock{false};


    calibrationStorage m_calStore;
};
