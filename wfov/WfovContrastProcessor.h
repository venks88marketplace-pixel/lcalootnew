#pragma once

#include <QObject>
#include <QImage>
#include <QRect>

#include <deque>
#include <memory>

#include <opencv2/core.hpp>

namespace imaging {
class IImageService;
}

class WfovContrastProcessor final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int requiredFrameCount
               READ requiredFrameCount
               CONSTANT)

    Q_PROPERTY(int validFrameCount
               READ validFrameCount
               NOTIFY validFrameCountChanged)

    Q_PROPERTY(bool measuring
               READ measuring
               NOTIFY measuringChanged)

    Q_PROPERTY(bool measurementReady
               READ measurementReady
               NOTIFY measurementReadyChanged)

    Q_PROPERTY(bool saved
               READ saved
               NOTIFY savedChanged)

    Q_PROPERTY(QString statusText
               READ statusText
               NOTIFY statusTextChanged)

    Q_PROPERTY(double leftGlobalContrastPercent
               READ leftGlobalContrastPercent
               NOTIFY resultsChanged)

    Q_PROPERTY(double rightGlobalContrastPercent
               READ rightGlobalContrastPercent
               NOTIFY resultsChanged)

    Q_PROPERTY(double globalContrastPercent
               READ globalContrastPercent
               NOTIFY resultsChanged)

    Q_PROPERTY(double leftLinearContrastPercent
               READ leftLinearContrastPercent
               NOTIFY resultsChanged)

    Q_PROPERTY(double rightLinearContrastPercent
               READ rightLinearContrastPercent
               NOTIFY resultsChanged)

    Q_PROPERTY(double linearContrastPercent
               READ linearContrastPercent
               NOTIFY resultsChanged)

public:
    explicit WfovContrastProcessor(
        std::shared_ptr<imaging::IImageService> imageService,
        QObject* parent = nullptr);

    int requiredFrameCount() const;
    int validFrameCount() const;

    bool measuring() const;
    bool measurementReady() const;
    bool saved() const;

    QString statusText() const;

    double leftGlobalContrastPercent() const;
    double rightGlobalContrastPercent() const;
    double globalContrastPercent() const;

    double leftLinearContrastPercent() const;
    double rightLinearContrastPercent() const;
    double linearContrastPercent() const;

    Q_INVOKABLE void startMeasurement();
    Q_INVOKABLE void confirmAndSave();
    Q_INVOKABLE void resetMeasurement();

signals:
    void validFrameCountChanged();
    void measuringChanged();
    void measurementReadyChanged();
    void savedChanged();
    void statusTextChanged();
    void resultsChanged();

private slots:
    void onWideFrameReady(const QImage& image);

private:
    struct FrameResult {
        double leftGlobal = 0.0;
        double rightGlobal = 0.0;
        double globalAverage = 0.0;

        double leftLinear = 0.0;
        double rightLinear = 0.0;
        double linearAverage = 0.0;

        QRect chartRect;
        QRect squareRect;
        QRect leftRoiRect;
        QRect rightRoiRect;

        QImage rawImage;
        QImage leftRoiImage;
        QImage rightRoiImage;
    };

    bool processFrame(
        const QImage& image,
        FrameResult& result) const;

    bool findChartRect(
        const cv::Mat& gray,
        QRect& chartRect) const;

    bool findDarkSquare(
        const cv::Mat& gray,
        const QRect& chartRect,
        QRect& squareRect) const;

    bool findMeasurementRoi(
        const cv::Mat& gray,
        const QRect& squareRect,
        QRect& roiRect) const;

    bool shrinkwrapCrop(
        const cv::Mat& roi,
        cv::Mat& cropped,
        QRect& localRect) const;

    // Classic Michelson contrast:
    // (max - min) / (max + min)
    //
    // Used for the linear contrast calculation.
    double michelsonContrast(
        const cv::Mat& roi) const;

    // Percentile-based global contrast:
    // (99th percentile - 1st percentile) / 99th percentile
    double globalContrastPercentile(
        const cv::Mat& roi) const;

    double averageLinearMichelsonContrast(
        const cv::Mat& roi,
        bool horizontal) const;

    cv::Mat imageToGrayMat(
        const QImage& image) const;

    QImage matToGrayImage(
        const cv::Mat& mat) const;

    void clearResults();

    void setStatus(
        const QString& status);

    void setMeasuring(
        bool value);

    void setMeasurementReady(
        bool value);

    void setSaved(
        bool value);

    bool saveMeasurement() const;

private:
    std::shared_ptr<imaging::IImageService> m_imageService;

    static constexpr int kRequiredFrameCount = 50;

    int m_validFrameCount = 0;

    bool m_measuring = false;
    bool m_measurementReady = false;
    bool m_saved = false;

    QString m_statusText = QStringLiteral("Ready");

    double m_leftGlobalContrastPercent = 0.0;
    double m_rightGlobalContrastPercent = 0.0;
    double m_globalContrastPercent = 0.0;

    double m_leftLinearContrastPercent = 0.0;
    double m_rightLinearContrastPercent = 0.0;
    double m_linearContrastPercent = 0.0;

    std::deque<double> m_leftGlobalHistory;
    std::deque<double> m_rightGlobalHistory;
    std::deque<double> m_leftLinearHistory;
    std::deque<double> m_rightLinearHistory;

    FrameResult m_lastFrameResult;
};