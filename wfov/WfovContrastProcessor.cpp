#include "WfovContrastProcessor.h"

#include "interfaces/IImageService.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>
#include <QtDebug>

#include <algorithm>
#include <cmath>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace {

constexpr int kChartThreshold = 80;

constexpr int kRoiOffsetX = -8;
constexpr int kRoiOffsetY = 41;
constexpr int kRoiWidth = 28;
constexpr int kRoiHeight = 14;

// Set false once diagnostics are no longer needed. 
// Helps to see the cropped images when still in dev phase.
constexpr bool kWriteDebugImages = false;

QString debugFolderPath()
{
    QString basePath = QStandardPaths::writableLocation(
        QStandardPaths::DesktopLocation);

    if (basePath.isEmpty()) {
        basePath = QStandardPaths::writableLocation(
            QStandardPaths::TempLocation);
    }

    QDir folder(
        basePath + QStringLiteral("/WFOV Contrast Debug"));

    folder.mkpath(QStringLiteral("."));

    return folder.absolutePath();
}

void saveDebugImage(
    const QString& fileName,
    const QImage& image)
{
    if (!kWriteDebugImages || image.isNull()) {
        return;
    }

    const QString path = QDir(debugFolderPath()).filePath(fileName);

    if (!image.save(path)) {
        qWarning() << "[WfovContrastProcessor] Failed to save debug image:"
                   << path;
    }
}

void saveDebugMat(
    const QString& fileName,
    const cv::Mat& image)
{
    if (!kWriteDebugImages || image.empty()) {
        return;
    }

    const QString path = QDir(debugFolderPath()).filePath(fileName);

    if (!cv::imwrite(path.toStdString(), image)) {
        qWarning() << "[WfovContrastProcessor] Failed to save debug image:"
                   << path;
    }
}

void saveHistogramCsv(
    const QString& fileName,
    const cv::Mat& leftImage,
    const cv::Mat& rightImage)
{
    if (!kWriteDebugImages ||
        leftImage.empty() ||
        rightImage.empty() ||
        leftImage.type() != CV_8UC1 ||
        rightImage.type() != CV_8UC1) {
        return;
    }

    const int histogramSize = 256;
    const float range[] = {0.0f, 256.0f};
    const float* ranges[] = {range};

    cv::Mat leftHist;
    cv::Mat rightHist;

    cv::calcHist(
        &leftImage,
        1,
        nullptr,
        cv::Mat(),
        leftHist,
        1,
        &histogramSize,
        ranges,
        true,
        false);

    cv::calcHist(
        &rightImage,
        1,
        nullptr,
        cv::Mat(),
        rightHist,
        1,
        &histogramSize,
        ranges,
        true,
        false);

    QFile file(
        QDir(debugFolderPath()).filePath(fileName));

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Truncate |
            QIODevice::Text)) {
        qWarning()
            << "[WfovContrastProcessor] Failed to save histogram CSV:"
            << file.fileName();
        return;
    }

    QTextStream stream(&file);

    stream << "bin,left_count,right_count\n";

    for (int i = 0; i < histogramSize; ++i) {
        const int leftCount =
            static_cast<int>(
                std::lround(leftHist.at<float>(i)));

        const int rightCount =
            static_cast<int>(
                std::lround(rightHist.at<float>(i)));

        stream << i << ","
               << leftCount << ","
               << rightCount << "\n";
    }

    file.close();
}

void saveHistogramImage(
    const QString& fileName,
    const cv::Mat& image)
{
    if (!kWriteDebugImages ||
        image.empty() ||
        image.type() != CV_8UC1) {
        return;
    }

    cv::Mat histogram;

    const int histogramSize = 256;

    const float intensityRange[] = {
        0.0f,
        256.0f
    };

    const float* histogramRange = intensityRange;

    cv::calcHist(
        &image,
        1,
        nullptr,
        cv::Mat(),
        histogram,
        1,
        &histogramSize,
        &histogramRange,
        true,
        false);

    double maximumCount = 0.0;

    cv::minMaxLoc(
        histogram,
        nullptr,
        &maximumCount);

    constexpr int histogramWidth = 1024;
    constexpr int histogramHeight = 512;

    cv::Mat plot(
        histogramHeight,
        histogramWidth,
        CV_8UC3,
        cv::Scalar(255, 255, 255));

    if (maximumCount <= 0.0) {
        saveDebugMat(fileName, plot);
        return;
    }

    std::vector<cv::Point> points;
    points.reserve(histogramSize);

    for (int intensity = 0;
         intensity < histogramSize;
         ++intensity) {
        const int x =
            cvRound(
                static_cast<double>(intensity) *
                static_cast<double>(histogramWidth - 1) /
                static_cast<double>(histogramSize - 1));

        const int y =
            histogramHeight -
            cvRound(
                (histogram.at<float>(intensity) /
                 maximumCount) *
                (histogramHeight - 20));

        points.emplace_back(x, y);
    }

    // Draw the line connecting each histogram-bin point.
    for (int intensity = 1;
         intensity < histogramSize;
         ++intensity) {
        cv::line(
            plot,
            points[intensity - 1],
            points[intensity],
            cv::Scalar(0, 0, 0),
            2,
            cv::LINE_AA);
    }

    // Draw a visible dot for every pixel-intensity bin.
    for (const cv::Point& point : points) {
        cv::circle(
            plot,
            point,
            3,
            cv::Scalar(0, 0, 255),
            cv::FILLED,
            cv::LINE_AA);
    }

    cv::line(
        plot,
        cv::Point(0, histogramHeight - 1),
        cv::Point(histogramWidth - 1, histogramHeight - 1),
        cv::Scalar(0, 0, 0),
        1);

    cv::putText(
        plot,
        "Pixel intensity",
        cv::Point(430, histogramHeight - 5),
        cv::FONT_HERSHEY_SIMPLEX,
        0.7,
        cv::Scalar(0, 0, 0),
        2,
        cv::LINE_AA);

    cv::putText(
        plot,
        "Pixel count",
        cv::Point(10, 30),
        cv::FONT_HERSHEY_SIMPLEX,
        0.7,
        cv::Scalar(0, 0, 0),
        2,
        cv::LINE_AA);

    saveDebugMat(fileName, plot);
}

double averageValues(
    const std::deque<double>& values)
{
    if (values.empty()) {
        return 0.0;
    }

    double sum = 0.0;

    for (const double value : values) {
        sum += value;
    }

    return sum / static_cast<double>(values.size());
}

QJsonArray valuesToJson(
    const std::deque<double>& values)
{
    QJsonArray result;

    for (const double value : values) {
        result.append(value);
    }

    return result;
}

double percentileU8(
    const cv::Mat& roi,
    double percentile)
{
    if (roi.empty() ||
        roi.type() != CV_8UC1) {
        return 0.0;
    }

    std::vector<uchar> values;

    values.reserve(
        static_cast<std::size_t>(
            roi.total()));

    for (int r = 0; r < roi.rows; ++r) {
        const uchar* row = roi.ptr<uchar>(r);

        for (int c = 0; c < roi.cols; ++c) {
            const uchar value = row[c];

            if (value != 0) {
                values.push_back(value);
            }
        }
    }

    if (values.empty()) {
        return 0.0;
    }

    percentile = std::clamp(
        percentile,
        0.0,
        100.0);

    const std::size_t count = values.size();

    const std::size_t index =
        static_cast<std::size_t>(
            std::round(
                (percentile / 100.0) *
                static_cast<double>(count - 1)));

    std::nth_element(
        values.begin(),
        values.begin() + index,
        values.end());

    return static_cast<double>(
        values[index]);
}

} // namespace

WfovContrastProcessor::WfovContrastProcessor(
    std::shared_ptr<imaging::IImageService> imageService,
    QObject* parent)
    : QObject(parent),
      m_imageService(std::move(imageService))
{
    qDebug()
        << "[WfovContrastProcessor] Constructing";

    qDebug()
        << "[WfovContrastProcessor] Image service valid:"
        << (m_imageService != nullptr);

    if (!m_imageService) {
        return;
    }

    const bool connected = QObject::connect(
        m_imageService.get(),
        &imaging::IImageService::wideFrameReady,
        this,
        &WfovContrastProcessor::onWideFrameReady);

    qDebug()
        << "[WfovContrastProcessor] wideFrameReady connected:"
        << connected;
}

int WfovContrastProcessor::requiredFrameCount() const
{
    return kRequiredFrameCount;
}

int WfovContrastProcessor::validFrameCount() const
{
    return m_validFrameCount;
}

bool WfovContrastProcessor::measuring() const
{
    return m_measuring;
}

bool WfovContrastProcessor::measurementReady() const
{
    return m_measurementReady;
}

bool WfovContrastProcessor::saved() const
{
    return m_saved;
}

QString WfovContrastProcessor::statusText() const
{
    return m_statusText;
}

double WfovContrastProcessor::leftGlobalContrastPercent() const
{
    return m_leftGlobalContrastPercent;
}

double WfovContrastProcessor::rightGlobalContrastPercent() const
{
    return m_rightGlobalContrastPercent;
}

double WfovContrastProcessor::globalContrastPercent() const
{
    return m_globalContrastPercent;
}

double WfovContrastProcessor::leftLinearContrastPercent() const
{
    return m_leftLinearContrastPercent;
}

double WfovContrastProcessor::rightLinearContrastPercent() const
{
    return m_rightLinearContrastPercent;
}

double WfovContrastProcessor::linearContrastPercent() const
{
    return m_linearContrastPercent;
}

void WfovContrastProcessor::startMeasurement()
{
    qDebug()
        << "[WfovContrastProcessor] Starting measurement";

    qDebug()
        << "[WfovContrastProcessor] Debug images will be written to:"
        << debugFolderPath();

    m_leftGlobalHistory.clear();
    m_rightGlobalHistory.clear();
    m_leftLinearHistory.clear();
    m_rightLinearHistory.clear();

    m_validFrameCount = 0;

    clearResults();

    setSaved(false);
    setMeasurementReady(false);
    setMeasuring(true);

    setStatus(
        QStringLiteral("Collecting frame 0/%1")
            .arg(kRequiredFrameCount));

    emit validFrameCountChanged();

    if (m_imageService) {
        m_imageService->stopWideVideo();
        m_imageService->setWideExposure(50.0);
        m_imageService->startWideVideo();
        m_imageService->setWideMeasurementActive(true);
    }
}

void WfovContrastProcessor::resetMeasurement()
{
    qDebug()
        << "[WfovContrastProcessor] Resetting measurement";

    if (m_imageService) {
        m_imageService->setWideMeasurementActive(false);
    }

    m_leftGlobalHistory.clear();
    m_rightGlobalHistory.clear();
    m_leftLinearHistory.clear();
    m_rightLinearHistory.clear();

    m_validFrameCount = 0;

    clearResults();

    setMeasuring(false);
    setMeasurementReady(false);
    setSaved(false);

    setStatus(QStringLiteral("Ready"));

    emit validFrameCountChanged();
}

void WfovContrastProcessor::confirmAndSave()
{
    qDebug()
        << "[WfovContrastProcessor] Confirm and save";

    if (!m_measurementReady) {
        setStatus(
            QStringLiteral("Complete a measurement before saving"));
        return;
    }

    if (saveMeasurement()) {
        setSaved(true);
        setStatus(QStringLiteral("Measurement saved"));
    } else {
        setSaved(false);
        setStatus(QStringLiteral("Failed to save measurement"));
    }
}

void WfovContrastProcessor::onWideFrameReady(
    const QImage& image)
{
    if (!m_measuring) {
        return;
    }

    if (image.isNull()) {
        qWarning()
            << "[WfovContrastProcessor] Received null WFOV frame";
        return;
    }

    qDebug()
        << "[WfovContrastProcessor] Received measurement frame:"
        << "format =" << image.format()
        << "width =" << image.width()
        << "height =" << image.height()
        << "bytesPerLine =" << image.bytesPerLine();

    FrameResult result;

    if (!processFrame(image, result)) {
        setStatus(
            QStringLiteral("Target not found - adjust camera or chart"));
        return;
    }

    m_lastFrameResult = result;

    m_leftGlobalHistory.push_back(result.leftGlobal);
    m_rightGlobalHistory.push_back(result.rightGlobal);

    m_leftLinearHistory.push_back(result.leftLinear);
    m_rightLinearHistory.push_back(result.rightLinear);

    while (m_leftGlobalHistory.size() > kRequiredFrameCount) {
        m_leftGlobalHistory.pop_front();
    }

    while (m_rightGlobalHistory.size() > kRequiredFrameCount) {
        m_rightGlobalHistory.pop_front();
    }

    while (m_leftLinearHistory.size() > kRequiredFrameCount) {
        m_leftLinearHistory.pop_front();
    }

    while (m_rightLinearHistory.size() > kRequiredFrameCount) {
        m_rightLinearHistory.pop_front();
    }

    ++m_validFrameCount;

    m_leftGlobalContrastPercent =
        averageValues(m_leftGlobalHistory) * 100.0;

    m_rightGlobalContrastPercent =
        averageValues(m_rightGlobalHistory) * 100.0;

    m_globalContrastPercent =
        (m_leftGlobalContrastPercent +
         m_rightGlobalContrastPercent) / 2.0;

    m_leftLinearContrastPercent =
        averageValues(m_leftLinearHistory) * 100.0;

    m_rightLinearContrastPercent =
        averageValues(m_rightLinearHistory) * 100.0;

    m_linearContrastPercent =
        (m_leftLinearContrastPercent +
         m_rightLinearContrastPercent) / 2.0;

    qDebug()
        << "[WfovContrastProcessor] Valid frame"
        << m_validFrameCount
        << "/"
        << kRequiredFrameCount
        << "global:"
        << m_globalContrastPercent
        << "linear:"
        << m_linearContrastPercent;

    emit validFrameCountChanged();
    emit resultsChanged();

    if (m_validFrameCount >= kRequiredFrameCount) {
        if (m_imageService) {
            m_imageService->setWideMeasurementActive(false);
        }

        setMeasuring(false);
        setMeasurementReady(true);
        setStatus(QStringLiteral("Measurement complete"));

        qDebug()
            << "[WfovContrastProcessor] Measurement complete"
            << "linear contrast:"
            << m_linearContrastPercent;
    } else {
        setStatus(
            QStringLiteral("Collecting frame %1/%2")
                .arg(m_validFrameCount)
                .arg(kRequiredFrameCount));
    }
}

bool WfovContrastProcessor::processFrame(
    const QImage& image,
    FrameResult& result) const
{
    saveDebugImage(
        QStringLiteral("00_raw.png"),
        image);

    const cv::Mat gray =
        imageToGrayMat(image);

    if (gray.empty()) {
        qWarning()
            << "[WfovContrastProcessor] processFrame failed:"
            << "gray image conversion produced an empty cv::Mat";
        return false;
    }

    double minValue = 0.0;
    double maxValue = 0.0;

    cv::minMaxLoc(
        gray,
        &minValue,
        &maxValue);

    qDebug()
        << "[WfovContrastProcessor] Gray image:"
        << gray.cols << "x" << gray.rows
        << "channels =" << gray.channels()
        << "min =" << minValue
        << "max =" << maxValue;

    saveDebugMat(
        QStringLiteral("01_gray.png"),
        gray);

    QRect chartRect;

    if (!findChartRect(gray, chartRect)) {
        qWarning()
            << "[WfovContrastProcessor] Chart NOT found."
            << "Inspect 01_gray.png and 02_chart_mask.png.";
        return false;
    }

    qDebug()
        << "[WfovContrastProcessor] Chart found:"
        << chartRect;

    QRect squareRect;

    if (!findDarkSquare(gray, chartRect, squareRect)) {
        qWarning()
            << "[WfovContrastProcessor] Dark square NOT found."
            << "Inspect 03_chart_crop.png and 04_dark_square_mask.png.";
        return false;
    }

    qDebug()
        << "[WfovContrastProcessor] Dark square found:"
        << squareRect;

    QRect roiRect;

    if (!findMeasurementRoi(gray, squareRect, roiRect)) {
        qWarning()
            << "[WfovContrastProcessor] Measurement ROI NOT found.";
        return false;
    }

    qDebug()
        << "[WfovContrastProcessor] Measurement ROI:"
        << roiRect;

    const cv::Rect fullRoi(
        roiRect.x(),
        roiRect.y(),
        roiRect.width(),
        roiRect.height());

    if (fullRoi.x < 0 ||
        fullRoi.y < 0 ||
        fullRoi.x + fullRoi.width > gray.cols ||
        fullRoi.y + fullRoi.height > gray.rows) {
        qWarning()
            << "[WfovContrastProcessor] Measurement ROI is outside"
            << "the raw image bounds."
            << "ROI =" << roiRect
            << "image =" << gray.cols << "x" << gray.rows;
        return false;
    }

    const cv::Mat roi = gray(fullRoi);

    saveDebugMat(
        QStringLiteral("05_measurement_roi.png"),
        roi);

    const int leftWidth =
        roiRect.width() / 2;

    const int rightWidth =
        roiRect.width() - leftWidth;

    if (leftWidth <= 0 ||
        rightWidth <= 0) {
        qWarning()
            << "[WfovContrastProcessor] Invalid split width."
            << "left =" << leftWidth
            << "right =" << rightWidth;
        return false;
    }

    const cv::Mat roiLeft =
        roi(
            cv::Rect(
                0,
                0,
                leftWidth,
                roiRect.height()));

    const cv::Mat roiRight =
        roi(
            cv::Rect(
                leftWidth,
                0,
                rightWidth,
                roiRect.height()));

    saveDebugMat(
        QStringLiteral("06_left_half.png"),
        roiLeft);

    saveDebugMat(
        QStringLiteral("07_right_half.png"),
        roiRight);

    cv::Mat croppedLeft;
    cv::Mat croppedRight;

    QRect leftLocalRect;
    QRect rightLocalRect;

    if (!shrinkwrapCrop(
            roiLeft,
            croppedLeft,
            leftLocalRect)) {
        qWarning()
            << "[WfovContrastProcessor] Left ROI shrink-wrap failed."
            << "Inspect 06_left_half.png.";
        return false;
    }

    if (!shrinkwrapCrop(
            roiRight,
            croppedRight,
            rightLocalRect)) {
        qWarning()
            << "[WfovContrastProcessor] Right ROI shrink-wrap failed."
            << "Inspect 07_right_half.png.";
        return false;
    }

    saveDebugMat(
        QStringLiteral("09_right_crop.png"),
        croppedRight);

    // Histogram data and histogram images are still generated for
    // the left and right stripe pixel intensities.
    saveHistogramCsv(
        QStringLiteral("histogram_lr.csv"),
        croppedLeft,
        croppedRight);

    saveHistogramImage(
        QStringLiteral("histogram_left.png"),
        croppedLeft);

    saveHistogramImage(
        QStringLiteral("histogram_right.png"),
        croppedRight);

    const double leftGlobal =
        globalContrastPercentile(croppedLeft);

    const double rightGlobal =
        globalContrastPercentile(croppedRight);

    double mn = 0.0;
    double mx = 0.0;

    cv::minMaxLoc(
        croppedRight,
        &mn,
        &mx);

    const double p1 =
        percentileU8(
            croppedRight,
            1.0);

    const double p99 =
        percentileU8(
            croppedRight,
            99.0);

    Q_UNUSED(mn);
    Q_UNUSED(mx);
    Q_UNUSED(p1);
    Q_UNUSED(p99);

    // Linear contrast:
    // left = vertical columns
    // right = horizontal rows
    const double leftLinear =
        averageLinearMichelsonContrast(
            croppedLeft,
            false);

    const double rightLinear =
        averageLinearMichelsonContrast(
            croppedRight,
            true);

    result.leftGlobal = leftGlobal;
    result.rightGlobal = rightGlobal;

    result.globalAverage =
        (leftGlobal + rightGlobal) / 2.0;

    result.leftLinear = leftLinear;
    result.rightLinear = rightLinear;

    result.linearAverage =
        (leftLinear + rightLinear) / 2.0;

    result.chartRect = chartRect;
    result.squareRect = squareRect;

    result.leftRoiRect =
        QRect(
            roiRect.x() + leftLocalRect.x(),
            roiRect.y() + leftLocalRect.y(),
            leftLocalRect.width(),
            leftLocalRect.height());

    result.rightRoiRect =
        QRect(
            roiRect.x() +
                leftWidth +
                rightLocalRect.x(),
            roiRect.y() +
                rightLocalRect.y(),
            rightLocalRect.width(),
            rightLocalRect.height());

    result.rawImage =
        image.copy();

    result.leftRoiImage =
        matToGrayImage(croppedLeft);

    result.rightRoiImage =
        matToGrayImage(croppedRight);

    qDebug()
        << "[WfovContrastProcessor] Frame success:"
        << "left global =" << leftGlobal
        << "right global =" << rightGlobal
        << "left linear =" << leftLinear
        << "right linear =" << rightLinear;

    return true;
}

bool WfovContrastProcessor::findChartRect(
    const cv::Mat& gray,
    QRect& chartRect) const
{
    if (gray.empty() ||
        gray.channels() != 1) {
        qWarning()
            << "[WfovContrastProcessor] findChartRect invalid gray input.";
        return false;
    }

    cv::Mat binary;

    cv::threshold(
        gray,
        binary,
        kChartThreshold,
        255,
        cv::THRESH_BINARY);

    const cv::Mat kernel =
        cv::getStructuringElement(
            cv::MORPH_RECT,
            cv::Size(9, 9));

    cv::morphologyEx(
        binary,
        binary,
        cv::MORPH_CLOSE,
        kernel);

    cv::morphologyEx(
        binary,
        binary,
        cv::MORPH_OPEN,
        kernel);

    saveDebugMat(
        QStringLiteral("02_chart_mask.png"),
        binary);

    std::vector<std::vector<cv::Point>> contours;

    cv::findContours(
        binary,
        contours,
        cv::RETR_EXTERNAL,
        cv::CHAIN_APPROX_SIMPLE);

    qDebug()
        << "[WfovContrastProcessor] Chart contour count:"
        << static_cast<int>(contours.size())
        << "threshold:"
        << kChartThreshold;

    if (contours.empty()) {
        return false;
    }

    const auto largest =
        std::max_element(
            contours.begin(),
            contours.end(),
            [](const auto& lhs,
               const auto& rhs) {
                return cv::contourArea(lhs) <
                       cv::contourArea(rhs);
            });

    const double area =
        cv::contourArea(*largest);

    const double imageArea =
        static_cast<double>(gray.cols) *
        static_cast<double>(gray.rows);

    const cv::Rect rect =
        cv::boundingRect(*largest);

    qDebug()
        << "[WfovContrastProcessor] Largest chart contour:"
        << "area =" << area
        << "required area >" << (0.01 * imageArea)
        << "bounds =" << rect.x
        << rect.y
        << rect.width
        << rect.height;

    if (area <= 0.01 * imageArea) {
        return false;
    }

    chartRect =
        QRect(
            rect.x,
            rect.y,
            rect.width,
            rect.height);

    return !chartRect.isEmpty();
}

bool WfovContrastProcessor::findDarkSquare(
    const cv::Mat& gray,
    const QRect& chartRect,
    QRect& squareRect) const
{
    if (gray.empty() ||
        chartRect.isEmpty()) {
        qWarning()
            << "[WfovContrastProcessor] findDarkSquare invalid input.";
        return false;
    }

    const cv::Rect chart(
        chartRect.x(),
        chartRect.y(),
        chartRect.width(),
        chartRect.height());

    if (chart.x < 0 ||
        chart.y < 0 ||
        chart.x + chart.width > gray.cols ||
        chart.y + chart.height > gray.rows) {
        qWarning()
            << "[WfovContrastProcessor] Chart rect out of bounds:"
            << chartRect;
        return false;
    }

    cv::Mat roi =
        gray(chart).clone();

    cv::Mat binary;

    cv::threshold(
        roi,
        binary,
        0,
        255,
        cv::THRESH_BINARY_INV |
            cv::THRESH_OTSU);

    const cv::Mat kernel =
        cv::getStructuringElement(
            cv::MORPH_RECT,
            cv::Size(3, 3));

    cv::morphologyEx(
        binary,
        binary,
        cv::MORPH_CLOSE,
        kernel);

    cv::morphologyEx(
        binary,
        binary,
        cv::MORPH_OPEN,
        kernel);

    saveDebugMat(
        QStringLiteral("04_dark_square_mask.png"),
        binary);

    std::vector<std::vector<cv::Point>> contours;

    cv::findContours(
        binary,
        contours,
        cv::RETR_EXTERNAL,
        cv::CHAIN_APPROX_SIMPLE);

    const int reference =
        std::min(
            chartRect.width(),
            chartRect.height());

    const double minimumSize =
        reference * 0.05;

    const double maximumSize =
        reference * 0.30;

    qDebug()
        << "[WfovContrastProcessor] Dark-square contour count:"
        << static_cast<int>(contours.size())
        << "reference:"
        << reference
        << "size range:"
        << minimumSize
        << "to"
        << maximumSize;

    double bestScore = 0.0;
    QRect bestRect;

    for (const auto& contour : contours) {
        const double area =
            cv::contourArea(contour);

        if (area < 50.0) {
            continue;
        }

        const cv::Rect rect =
            cv::boundingRect(contour);

        if (!(rect.width > minimumSize &&
              rect.width < maximumSize &&
              rect.height > minimumSize &&
              rect.height < maximumSize)) {
            continue;
        }

        const double aspect =
            static_cast<double>(rect.width) /
            std::max(
                1.0,
                static_cast<double>(rect.height));

        if (!(aspect > 0.7 &&
              aspect < 1.4)) {
            continue;
        }

        const double score =
            area *
            (1.0 - std::abs(1.0 - aspect));

        if (score > bestScore) {
            bestScore = score;

            bestRect =
                QRect(
                    chartRect.x() + rect.x,
                    chartRect.y() + rect.y,
                    rect.width,
                    rect.height);
        }
    }

    if (bestRect.isEmpty()) {
        qWarning()
            << "[WfovContrastProcessor] No valid dark square candidate found.";
        return false;
    }

    squareRect = bestRect;

    qDebug()
        << "[WfovContrastProcessor] Selected dark square:"
        << squareRect
        << "score:"
        << bestScore;

    return true;
}

bool WfovContrastProcessor::findMeasurementRoi(
    const cv::Mat& gray,
    const QRect& squareRect,
    QRect& roiRect) const
{
    if (gray.empty() ||
        squareRect.isEmpty()) {
        qWarning()
            << "[WfovContrastProcessor] findMeasurementRoi invalid input.";
        return false;
    }

    if (gray.cols < kRoiWidth ||
        gray.rows < kRoiHeight) {
        qWarning()
            << "[WfovContrastProcessor] Raw image is smaller than"
            << "the required"
            << kRoiWidth
            << "x"
            << kRoiHeight
            << "measurement ROI.";
        return false;
    }

    const int requestedX =
        squareRect.x() +
        kRoiOffsetX;

    const int requestedY =
        squareRect.y() +
        squareRect.height() +
        kRoiOffsetY;

    if (squareRect.width() > gray.cols * 0.7 ||
        squareRect.height() > gray.rows * 0.7) {
        return false;
    }

    const int clampedX =
        std::clamp(
            requestedX,
            0,
            gray.cols - kRoiWidth);

    const int clampedY =
        std::clamp(
            requestedY,
            0,
            gray.rows - kRoiHeight);

    roiRect =
        QRect(
            clampedX,
            clampedY,
            kRoiWidth,
            kRoiHeight);

    qDebug()
        << "[WfovContrastProcessor] Fixed-offset ROI:"
        << "requested ="
        << requestedX
        << requestedY
        << "clamped ="
        << roiRect
        << "offset ="
        << kRoiOffsetX
        << kRoiOffsetY
        << "size ="
        << kRoiWidth
        << kRoiHeight;

    return !roiRect.isEmpty();
}

bool WfovContrastProcessor::shrinkwrapCrop(
    const cv::Mat& roi,
    cv::Mat& cropped,
    QRect& localRect) const
{
    if (roi.empty()) {
        return false;
    }

    cv::Mat binary;

    cv::threshold(
        roi,
        binary,
        0,
        175,
        cv::THRESH_BINARY_INV |
            cv::THRESH_OTSU);

    std::vector<std::vector<cv::Point>> contours;

    cv::findContours(
        binary,
        contours,
        cv::RETR_EXTERNAL,
        cv::CHAIN_APPROX_SIMPLE);

    int minX = roi.cols;
    int minY = roi.rows;
    int maxX = 0;
    int maxY = 0;

    bool found = false;

    for (const auto& contour : contours) {
        const double area =
            cv::contourArea(contour);

        if (area < 2.0) {
            continue;
        }

        const cv::Rect rect =
            cv::boundingRect(contour);

        minX =
            std::min(
                minX,
                rect.x);

        minY =
            std::min(
                minY,
                rect.y);

        maxX =
            std::max(
                maxX,
                rect.x + rect.width);

        maxY =
            std::max(
                maxY,
                rect.y + rect.height);

        found = true;
    }

    if (!found ||
        minX >= maxX ||
        minY >= maxY) {
        return false;
    }

    const cv::Rect cropRect(
        minX,
        minY,
        maxX - minX,
        maxY - minY);

    cropped =
        roi(cropRect).clone();

    localRect =
        QRect(
            minX,
            minY,
            cropRect.width,
            cropRect.height);

    return !cropped.empty();
}

double WfovContrastProcessor::michelsonContrast(
    const cv::Mat& roi) const
{
    if (roi.empty()) {
        return 0.0;
    }

    double minimum = 0.0;
    double maximum = 0.0;

    cv::minMaxLoc(
        roi,
        &minimum,
        &maximum);

    const double denominator =
        std::max(
            maximum + minimum,
            1.0e-9);

    return (maximum - minimum) /
           denominator;
}

double WfovContrastProcessor::globalContrastPercentile(
    const cv::Mat& roi) const
{
    if (roi.empty() ||
        roi.type() != CV_8UC1) {
        return 0.0;
    }

    constexpr double kLowPercentile = 1.0;
    constexpr double kHighPercentile = 99.0;

    const double low =
        percentileU8(
            roi,
            kLowPercentile);

    const double high =
        percentileU8(
            roi,
            kHighPercentile);

    if (high <= 0.0) {
        return 0.0;
    }

    const double contrast =
        (high - low) /
        high;

    return std::clamp(
        contrast,
        0.0,
        1.0);
}

double WfovContrastProcessor::averageLinearMichelsonContrast(
    const cv::Mat& roi,
    bool horizontal) const
{
    if (roi.empty()) {
        return 0.0;
    }

    std::vector<double> values;

    if (horizontal) {
        for (int row = 0;
             row < roi.rows;
             ++row) {
            values.push_back(
                michelsonContrast(
                    roi.row(row)));
        }
    } else {
        for (int column = 0;
             column < roi.cols;
             ++column) {
            values.push_back(
                michelsonContrast(
                    roi.col(column)));
        }
    }

    if (values.size() <= 2) {
        return 0.0;
    }

    double sum = 0.0;

    for (std::size_t index = 1;
         index + 1 < values.size();
         ++index) {
        sum += values[index];
    }

    return sum /
           static_cast<double>(
               values.size() - 2);
}

cv::Mat WfovContrastProcessor::imageToGrayMat(
    const QImage& image) const
{
    if (image.isNull()) {
        return {};
    }

    if (image.format() == QImage::Format_Grayscale8) {
        const QImage gray = image;

        return cv::Mat(
            gray.height(),
            gray.width(),
            CV_8UC1,
            const_cast<uchar*>(gray.bits()),
            gray.bytesPerLine()).clone();
    }

    const QImage rgba =
        image.convertToFormat(
            QImage::Format_RGBA8888);

    cv::Mat rgbaMat(
        rgba.height(),
        rgba.width(),
        CV_8UC4,
        const_cast<uchar*>(rgba.bits()),
        rgba.bytesPerLine());

    cv::Mat green;

    cv::extractChannel(
        rgbaMat,
        green,
        1);

    return green.clone();
}

QImage WfovContrastProcessor::matToGrayImage(
    const cv::Mat& mat) const
{
    if (mat.empty() ||
        mat.type() != CV_8UC1) {
        return {};
    }

    QImage image(
        mat.data,
        mat.cols,
        mat.rows,
        static_cast<int>(mat.step),
        QImage::Format_Grayscale8);

    return image.copy();
}

void WfovContrastProcessor::clearResults()
{
    m_leftGlobalContrastPercent = 0.0;
    m_rightGlobalContrastPercent = 0.0;
    m_globalContrastPercent = 0.0;

    m_leftLinearContrastPercent = 0.0;
    m_rightLinearContrastPercent = 0.0;
    m_linearContrastPercent = 0.0;

    m_lastFrameResult =
        FrameResult{};

    emit resultsChanged();
}

void WfovContrastProcessor::setStatus(
    const QString& status)
{
    if (m_statusText == status) {
        return;
    }

    m_statusText = status;

    emit statusTextChanged();
}

void WfovContrastProcessor::setMeasuring(
    bool value)
{
    if (m_measuring == value) {
        return;
    }

    m_measuring = value;

    emit measuringChanged();
}

void WfovContrastProcessor::setMeasurementReady(
    bool value)
{
    if (m_measurementReady == value) {
        return;
    }

    m_measurementReady = value;

    emit measurementReadyChanged();
}

void WfovContrastProcessor::setSaved(
    bool value)
{
    if (m_saved == value) {
        return;
    }

    m_saved = value;

    emit savedChanged();
}

bool WfovContrastProcessor::saveMeasurement() const
{
    if (m_lastFrameResult.rawImage.isNull()) {
        return false;
    }

    const QString desktopPath =
        QStandardPaths::writableLocation(
            QStandardPaths::DesktopLocation);

    const QString date =
        QDate::currentDate().toString(
            QStringLiteral("yyyy-MM-dd"));

    const QString timestamp =
        QDateTime::currentDateTime().toString(
            QStringLiteral("HH_mm_ss"));

    QDir folder(
        desktopPath +
        QStringLiteral("/WFOV Camera Data ") +
        date);

    if (!folder.mkpath(QStringLiteral("."))) {
        return false;
    }

    const QString rawPath =
        folder.filePath(
            QStringLiteral("Raw_%1.png")
                .arg(timestamp));

    const QString leftPath =
        folder.filePath(
            QStringLiteral("Stripes_Left_%1.png")
                .arg(timestamp));

    const QString rightPath =
        folder.filePath(
            QStringLiteral("Stripes_Right_%1.png")
                .arg(timestamp));

    if (!m_lastFrameResult.rawImage.save(rawPath)) {
        return false;
    }

    if (!m_lastFrameResult.leftRoiImage.save(leftPath)) {
        return false;
    }

    if (!m_lastFrameResult.rightRoiImage.save(rightPath)) {
        return false;
    }

    QJsonObject root;

    root.insert(
        QStringLiteral("time"),
        timestamp);

    root.insert(
        QStringLiteral("date"),
        date);

    root.insert(
        QStringLiteral("unix_timestamp"),
        QDateTime::currentSecsSinceEpoch());

    root.insert(
        QStringLiteral("valid_frame_count"),
        m_validFrameCount);

    root.insert(
        QStringLiteral("left_global_contrast_percent"),
        m_leftGlobalContrastPercent);

    root.insert(
        QStringLiteral("right_global_contrast_percent"),
        m_rightGlobalContrastPercent);

    root.insert(
        QStringLiteral("global_contrast_percent"),
        m_globalContrastPercent);

    root.insert(
        QStringLiteral("left_linear_contrast_percent"),
        m_leftLinearContrastPercent);

    root.insert(
        QStringLiteral("right_linear_contrast_percent"),
        m_rightLinearContrastPercent);

    root.insert(
        QStringLiteral("linear_contrast_percent"),
        m_linearContrastPercent);

    root.insert(
        QStringLiteral("left_global_history"),
        valuesToJson(
            m_leftGlobalHistory));

    root.insert(
        QStringLiteral("right_global_history"),
        valuesToJson(
            m_rightGlobalHistory));

    // JSON remains unchanged: linear histories are still included.
    root.insert(
        QStringLiteral("left_linear_history"),
        valuesToJson(
            m_leftLinearHistory));

    root.insert(
        QStringLiteral("right_linear_history"),
        valuesToJson(
            m_rightLinearHistory));

    QJsonObject chart;

    chart.insert(
        QStringLiteral("x"),
        m_lastFrameResult.chartRect.x());

    chart.insert(
        QStringLiteral("y"),
        m_lastFrameResult.chartRect.y());

    chart.insert(
        QStringLiteral("width"),
        m_lastFrameResult.chartRect.width());

    chart.insert(
        QStringLiteral("height"),
        m_lastFrameResult.chartRect.height());

    QJsonObject square;

    square.insert(
        QStringLiteral("x"),
        m_lastFrameResult.squareRect.x());

    square.insert(
        QStringLiteral("y"),
        m_lastFrameResult.squareRect.y());

    square.insert(
        QStringLiteral("width"),
        m_lastFrameResult.squareRect.width());

    square.insert(
        QStringLiteral("height"),
        m_lastFrameResult.squareRect.height());

    QJsonObject leftRoi;

    leftRoi.insert(
        QStringLiteral("x"),
        m_lastFrameResult.leftRoiRect.x());

    leftRoi.insert(
        QStringLiteral("y"),
        m_lastFrameResult.leftRoiRect.y());

    leftRoi.insert(
        QStringLiteral("width"),
        m_lastFrameResult.leftRoiRect.width());

    leftRoi.insert(
        QStringLiteral("height"),
        m_lastFrameResult.leftRoiRect.height());

    QJsonObject rightRoi;

    rightRoi.insert(
        QStringLiteral("x"),
        m_lastFrameResult.rightRoiRect.x());

    rightRoi.insert(
        QStringLiteral("y"),
        m_lastFrameResult.rightRoiRect.y());

    rightRoi.insert(
        QStringLiteral("width"),
        m_lastFrameResult.rightRoiRect.width());

    rightRoi.insert(
        QStringLiteral("height"),
        m_lastFrameResult.rightRoiRect.height());

    QJsonObject locations;

    locations.insert(
        QStringLiteral("chart"),
        chart);

    locations.insert(
        QStringLiteral("tracking_square"),
        square);

    locations.insert(
        QStringLiteral("left_roi"),
        leftRoi);

    locations.insert(
        QStringLiteral("right_roi"),
        rightRoi);

    root.insert(
        QStringLiteral("locations"),
        locations);

    root.insert(
        QStringLiteral("raw_image"),
        QFileInfo(rawPath).fileName());

    root.insert(
        QStringLiteral("left_roi_image"),
        QFileInfo(leftPath).fileName());

    root.insert(
        QStringLiteral("right_roi_image"),
        QFileInfo(rightPath).fileName());

    const QString jsonPath =
        folder.filePath(
            QStringLiteral("CaptureData_%1.json")
                .arg(timestamp));

    QFile jsonFile(jsonPath);

    if (!jsonFile.open(
            QIODevice::WriteOnly |
            QIODevice::Truncate)) {
        return false;
    }

    jsonFile.write(
        QJsonDocument(root)
            .toJson(
                QJsonDocument::Indented));

    jsonFile.close();

    const QString csvPath =
        folder.filePath(
            QStringLiteral("CaptureData_%1.csv")
                .arg(timestamp));

    QFile csvFile(csvPath);

    if (!csvFile.open(
            QIODevice::WriteOnly |
            QIODevice::Truncate)) {
        return false;
    }

    QTextStream csv(&csvFile);

    csv << "Metric,Value\n";

    csv << "Left Global Contrast (%),"
        << m_leftGlobalContrastPercent
        << "\n";

    csv << "Right Global Contrast (%),"
        << m_rightGlobalContrastPercent
        << "\n";

    csv << "Average Global Contrast (%),"
        << m_globalContrastPercent
        << "\n";

    csv << "Left Linear Contrast (%),"
        << m_leftLinearContrastPercent
        << "\n";

    csv << "Right Linear Contrast (%),"
        << m_rightLinearContrastPercent
        << "\n";

    csv << "Average Linear Contrast (%),"
        << m_linearContrastPercent
        << "\n";

    csv << "Valid Frames,"
        << m_validFrameCount
        << "\n";

    csvFile.close();

    return true;
}