#pragma once
#include "BaseProcessor.h"
#include "Photometric/PhotometricResults.h"
#include "Photometric/PhotometricParameters.h"

class PhotometricProcessor : public BaseProcessor {
    Q_OBJECT

    Q_PROPERTY(bool passed
                READ passed
                NOTIFY passedChanged)

    Q_PROPERTY(ResultStatus resultStatus
                READ resultStatus
                NOTIFY resultStatusChanged)

    Q_PROPERTY(QString resultStatusText
                READ resultStatusText
                NOTIFY resultStatusTextChanged)

    Q_PROPERTY(double progress
                   READ progress
                    NOTIFY progressChanged);

    Q_PROPERTY(QString statusMessage
                   READ statusMessage
                    WRITE setStatusMessage
                    NOTIFY statusMessageChanged)

public:
    explicit PhotometricProcessor(QString name,
        std::shared_ptr<ILoggerService> logger,
        std::shared_ptr<IServerProxyService> proxyService,
        std::shared_ptr<imaging::IImageService>imageService,
        std::shared_ptr<IStepperControllerService>stepperService,
        const PhotometricParameters defaultParams,
        QObject* parent=nullptr);

    Q_INVOKABLE virtual void execute(const QString& action) override;
    Q_INVOKABLE virtual void save() override {}
    Q_INVOKABLE void resetProgress();

    enum class ResultStatus {
        NotRun,
        Pass,
        Fail
    };
    Q_ENUM(ResultStatus)

    bool passed() const { return m_passed; }
    ResultStatus resultStatus() const { return m_resultStatus; }
    QString resultStatusText() const { return m_resultStatusText; }

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

public slots:
    void onResultsReady( PhotometricResults results);

signals:
    void progressChanged();
    void busyChanged();
    void message(QString text, QColor color, QString level);
    void statusMessageChanged();

    void setSucceeded(const QString& message);
    void setFailed(const QString& message);
    void passedChanged();
    void resultStatusChanged();
    void resultStatusTextChanged();

private:
    QString resultStatusText();
    double m_progress{0.0};
    bool m_busy{false};
    QString m_statusMessage;
    bool m_passed = false;
    ResultStatus m_resultStatus;
    QString m_resultStatusText;

    bool m_isHWConfigured{false};
    bool m_isImageServiceMock{false};
    PhotometricResults m_results;
    const PhotometricParameters m_defaultParams;
};
