#include "UVAlignmentProcessor.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/matx.hpp>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QImage>

using imaging::IImageService;

namespace {
// Save a CV_8UC1 or CV_8UC3 (BGR) Mat as PNG using Qt
bool saveMatPng(const cv::Mat& m, const QString& path)
{
    if (m.empty()) return false;
    QImage q;
    if (m.type() == CV_8UC1) {
        q = QImage(m.data, m.cols, m.rows, static_cast<int>(m.step), QImage::Format_Grayscale8).copy();
    } else if (m.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(m, rgb, cv::COLOR_BGR2RGB);
        q = QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    } else {
        return false;
    }
    return q.save(path, "PNG");
}
} // namespace

UVAlignmentProcessor::UVAlignmentProcessor(QString name,
                                           std::shared_ptr<ILoggerService> logger,
                                           std::shared_ptr<IServerProxyService> proxyService,
                                           std::shared_ptr<IImageService> imageService,
                                           std::shared_ptr<IStepperControllerService> stepperService,
                                           QObject* parent)
    : BaseProcessor(name, std::move(logger), std::move(proxyService), std::move(imageService),
                   std::move(stepperService), parent)
{
    m_name = "UVAlignmentProcessor";
    m_status = "Initialized";
    precomputeLUT();
    qDebug() << "UVAlignmentProcessor CREATED";
}

void UVAlignmentProcessor::execute(const QString& action) {
    Q_UNUSED(action);
    IImageService* svc = qobject_cast<IImageService*>(imageService());
    if (!svc) {
        qWarning() << "IImageService is not available";
        return;
    }
    processFrame(svc->leftImage());
}

void UVAlignmentProcessor::save() {
    saveData();
}

void UVAlignmentProcessor::processFrame(const QPixmap& frame) {
    if (frame.isNull()) {
        m_statusMessage = "Frame is null";
        emit statusMessageChanged();
        return;
    }

    if (m_throttle.isValid() && m_throttle.elapsed() < MIN_FRAME_INTERVAL_MS) {
        return;
    }
    m_throttle.restart();

    cv::Mat img8 = toMono8(QPixmapToCvMat(frame));
    if (img8.empty()) return;
    m_lastRawFrame = img8.clone();

    try {
        if (!processPipeline(img8)) {
            abortProcessing(img8);
        }
    } catch (const std::exception& e) {
        qWarning() << "Processing error:" << e.what();
        abortProcessing(img8);
    }
    emit dataUpdated();
    emit statusMessageChanged();
}

bool UVAlignmentProcessor::processPipeline(const cv::Mat& img8) {
    cv::Mat calc = img8.clone();
    cv::Mat calc_small = resizeImage(img8, RESIZE_FACTOR);

    //  Centroid 
    auto c = findCentroid(calc_small);
    if (c.first < 0 || c.second < 0) {
        m_statusMessage = "Hexagon not found (no centroid)";
        return false;
    }
    const int icx = static_cast<int>(c.first * RESIZE_FACTOR);
    const int icy = static_cast<int>(c.second * RESIZE_FACTOR);
    m_centroidX = icx;
    m_centroidY = icy;

    //  Edges 
    cv::Mat blur, edges;
    cv::GaussianBlur(calc_small, blur, cv::Size(GAUSS_KSIZE_X, GAUSS_KSIZE_Y), GAUSS_SIGMA);
    cv::Canny(blur, edges, CANNY_LO, CANNY_HI);

    //  Line detection / MTF 
    cv::Point2d center_small(static_cast<double>(icx) / RESIZE_FACTOR,
                             static_cast<double>(icy) / RESIZE_FACTOR);
    cv::Vec4d line = pickUpperRightSimple(edges, 15.0, 20.0, 60, &center_small);

    m_mtf = 0.0;
    bool hasLine = line[0] >= 0;
    int lx1 = 0, ly1 = 0, lx2 = 0, ly2 = 0, lmx = 0, lmy = 0;
    cv::Mat cropView;

    if (hasLine) {
        lx1 = static_cast<int>(line[0] * RESIZE_FACTOR);
        ly1 = static_cast<int>(line[1] * RESIZE_FACTOR);
        lx2 = static_cast<int>(line[2] * RESIZE_FACTOR);
        ly2 = static_cast<int>(line[3] * RESIZE_FACTOR);
        lmx = (lx1 + lx2) / 2;
        lmy = (ly1 + ly2) / 2;

        cv::Mat rotated = rotateVertical(calc, cv::Vec4d(lx1, ly1, lx2, ly2), cv::Point2d(lmx, lmy));
        cropView = cropCenter(rotated, CROP_HALF_W, CROP_HALF_H);
        MtfResult r = computeMtfFromCrop(cropView);
        if (!r.mtf.empty()) {
            m_mtf = mtfAtLpmm(r.f_lpmm, r.mtf, TARGET_LPMM);
        }
    }

    //  Mean filter around the ROI (accurate, full-res) 
    if (m_applyMeanFilter) {
        if (m_meanKernel.empty()) m_meanKernel = makeDiscKernel(KERNEL_RADIUS);
        const int half = UNIFORMITY_CIRCLE_RADIUS_B + KERNEL_RADIUS + 2;
        cv::Rect roi = cv::Rect(icx - half, icy - half, 2 * half, 2 * half)
                       & cv::Rect(0, 0, calc.cols, calc.rows);
        if (roi.area() > 0) {
            cv::Mat patch = calc(roi).clone();
            cv::filter2D(patch, patch, -1, m_meanKernel);
            patch.copyTo(calc(roi));
        }
    }
    m_lastFilteredFrame = calc.clone();

    //  Uniformity 
    UniformityResult u = computeUniformity(calc, cv::Point(icx, icy), UNIFORMITY_CIRCLE_RADIUS_B);
    m_uniformityPkPk = u.p2p;
    m_uniformityRMS = u.rms;
    m_uniformityRadius = UNIFORMITY_CIRCLE_RADIUS_B;

    // ++++ Display frame +++
    cv::Mat disp = resizeImage(img8, DOWNSCALE_FACTOR);

    if (m_applyMeanFilter) {
        if (m_cosmeticKernel.empty()) m_cosmeticKernel = makeDiscKernel(KERNEL_RADIUS / DOWNSCALE_FACTOR);
        const int rf = (UNIFORMITY_CIRCLE_RADIUS_B + KERNEL_RADIUS + 2) / DOWNSCALE_FACTOR;
        cv::Rect r = cv::Rect(icx / DOWNSCALE_FACTOR - rf, icy / DOWNSCALE_FACTOR - rf, 2 * rf, 2 * rf)
                     & cv::Rect(0, 0, disp.cols, disp.rows);
        if (r.area() > 0) {
            cv::Mat p = disp(r).clone();
            cv::filter2D(p, p, -1, m_cosmeticKernel);
            p.copyTo(disp(r));
        }
    }

    cv::Mat dispBgr;
    cv::cvtColor(disp, dispBgr, cv::COLOR_GRAY2BGR);

    if (m_applyMeanFilter) {
        // border around the filtered region
        cv::rectangle(dispBgr,
                      cv::Point((icx - UNIFORMITY_CIRCLE_RADIUS_B) / DOWNSCALE_FACTOR,
                                (icy - UNIFORMITY_CIRCLE_RADIUS_B) / DOWNSCALE_FACTOR),
                      cv::Point((icx + UNIFORMITY_CIRCLE_RADIUS_B) / DOWNSCALE_FACTOR,
                                (icy + UNIFORMITY_CIRCLE_RADIUS_B) / DOWNSCALE_FACTOR),
                      cv::Scalar(0, 0, 0), 1);
    }

    if (m_applyFalseColor) {
        double maxVal = 0;
        cv::Mat scaled = normalizeScale(dispBgr, maxVal);
        cv::Mat colored;
        cv::LUT(scaled, m_customLUT, colored);
        dispBgr = colored;
        cv::putText(dispBgr, "Scaled to maxval: " + std::to_string(static_cast<int>(maxVal)),
                cv::Point(5, 25), cv::FONT_HERSHEY_SIMPLEX, 0.35,
                cv::Scalar(255, 255, 255), 0.25, cv::LINE_AA);
    }

    // Overlays
    if (hasLine) {
        dashedLine(dispBgr,
                   cv::Point(lx1 / DOWNSCALE_FACTOR, ly1 / DOWNSCALE_FACTOR),
                   cv::Point(lx2 / DOWNSCALE_FACTOR, ly2 / DOWNSCALE_FACTOR),
                   cv::Scalar(0, 255, 255), 2, 12.0, 0.3);
        cv::circle(dispBgr, cv::Point(lmx / DOWNSCALE_FACTOR, lmy / DOWNSCALE_FACTOR),
                   4, cv::Scalar(0, 0, 255), -1);
    }
    cv::circle(dispBgr, cv::Point(icx / DOWNSCALE_FACTOR, icy / DOWNSCALE_FACTOR),
               4, cv::Scalar(0, 0, 255), -1);
    cv::circle(dispBgr, cv::Point(icx / DOWNSCALE_FACTOR, icy / DOWNSCALE_FACTOR),
               UNIFORMITY_CIRCLE_RADIUS_B / DOWNSCALE_FACTOR, cv::Scalar(255, 90, 90), 2);

    m_lastDisplayFrame = dispBgr.clone();
    emit displayFrameReady(matToPixmap(dispBgr));

    // Crop inset
    if (hasLine && !cropView.empty()) {
        emit cropFrameReady(matToPixmap(cropView));
    } else {
        emit cropFrameReady(matToPixmap(cv::Mat::zeros(CROP_HALF_H * 2, CROP_HALF_W * 2, CV_8UC1)));
    }

    m_statusMessage = QString("Centroid: (%1, %2) | MTF: %3 | RMS: %4%")
                         .arg(icx).arg(icy)
                         .arg(m_mtf, 0, 'f', 2)
                         .arg(m_uniformityRMS, 0, 'f', 2);
    return true;
}

void UVAlignmentProcessor::abortProcessing(const cv::Mat& img8) {
    m_centroidX = -99.0;
    m_centroidY = -99.0;
    m_mtf = 0.0;
    m_uniformityPkPk = 0.0;
    m_uniformityRMS = 0.0;
    m_statusMessage = "Hexagon not found";

    if (!img8.empty()) {
        cv::Mat disp = resizeImage(img8, DOWNSCALE_FACTOR);
        cv::Mat bgr;
        cv::cvtColor(disp, bgr, cv::COLOR_GRAY2BGR);
        cv::putText(bgr, "Hexagon Not Found!", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                    0.8, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
        m_lastDisplayFrame = bgr.clone();
        emit displayFrameReady(matToPixmap(bgr));
        emit cropFrameReady(matToPixmap(cv::Mat::zeros(CROP_HALF_H * 2, CROP_HALF_W * 2, CV_8UC1)));
    }
}

cv::Mat UVAlignmentProcessor::QPixmapToCvMat(const QPixmap& pixmap) {
    QImage img = pixmap.toImage().convertToFormat(QImage::Format_Grayscale8);
    if (img.isNull()) return cv::Mat();
    return cv::Mat(img.height(), img.width(), CV_8U,
                  const_cast<uchar*>(img.bits()), static_cast<size_t>(img.bytesPerLine())).clone();
}

QPixmap UVAlignmentProcessor::matToPixmap(const cv::Mat& m) {
    if (m.empty()) return QPixmap();
    if (m.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(m, rgb, cv::COLOR_BGR2RGB);
        QImage q(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
        return QPixmap::fromImage(q.copy());
    }
    if (m.type() == CV_8UC1) {
        QImage q(m.data, m.cols, m.rows, static_cast<int>(m.step), QImage::Format_Grayscale8);
        return QPixmap::fromImage(q.copy());
    }
    return QPixmap();
}

cv::Mat UVAlignmentProcessor::toMono8(const cv::Mat& img) {
    if (img.channels() == 3) {
        cv::Mat gray;
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
        return gray;
    }
    if (img.type() == CV_8U) {
        return img;
    }
    double minVal, maxVal;
    cv::minMaxLoc(img, &minVal, &maxVal);
    if (maxVal > minVal) {
        cv::Mat scaled;
        img.convertTo(scaled, CV_8U, 255.0 / (maxVal - minVal), -255.0 * minVal / (maxVal - minVal));
        return scaled;
    }
    return cv::Mat::zeros(img.size(), CV_8U);
}

std::pair<double, double> UVAlignmentProcessor::findCentroid(const cv::Mat& img) {
    cv::Mat binary;
    cv::threshold(img, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        return {-99, -99};
    }

    auto max_contour = *std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        });

    cv::Moments M = cv::moments(max_contour);
    if (M.m00 == 0) return {-99, -99};
    return {M.m10 / M.m00, M.m01 / M.m00};
}

double UVAlignmentProcessor::foldedAngleDeg(double dx, double dy) {
    double ang = std::abs(std::atan2(dy, dx) * 180.0 / M_PI);
    ang = std::fmod(ang, 180.0);
    return ang > 90.0 ? 180.0 - ang : ang;
}

double UVAlignmentProcessor::interp1D(const cv::Mat& x, const cv::Mat& y, double xi) {
    if (xi <= x.at<float>(0, 0)) return y.at<float>(0, 0);
    if (xi >= x.at<float>(0, x.cols - 1)) return y.at<float>(0, y.cols - 1);

    for (int i = 0; i < x.cols - 1; ++i) {
        float x0 = x.at<float>(0, i);
        float x1 = x.at<float>(0, i + 1);
        if (xi >= x0 && xi <= x1) {
            float y0 = y.at<float>(0, i);
            float y1 = y.at<float>(0, i + 1);
            float t = static_cast<float>((xi - x0) / (x1 - x0));
            return y0 + t * (y1 - y0);
        }
    }
    return y.at<float>(0, y.cols - 1);
}

cv::Vec4d UVAlignmentProcessor::pickUpperRightSimple(const cv::Mat& edges, double target_deg,
                                                      double angle_tol, double min_len_px,
                                                      const cv::Point2d* require_right_of) {
    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(edges, lines, HOUGH_RHO, HOUGH_THETA, HOUGH_THRESH,
                   HOUGH_MIN_LINE, HOUGH_MAX_GAP);

    if (lines.empty()) return cv::Vec4d(-1, -1, -1, -1);

    struct Candidate {
        cv::Vec4d line;
        double len;
        double my;
    };
    std::vector<Candidate> candidates;

    for (const auto& l : lines) {
        double x1 = l[0], y1 = l[1], x2 = l[2], y2 = l[3];
        double dx = x2 - x1, dy = y2 - y1;
        double seg_len = std::hypot(dx, dy);
        if (seg_len < min_len_px) continue;

        double ang = foldedAngleDeg(dx, dy);
        if (std::abs(ang - target_deg) > angle_tol) continue;

        double mx = 0.5 * (x1 + x2), my = 0.5 * (y1 + y2);
        if (require_right_of && mx <= require_right_of->x) continue;

        candidates.push_back({cv::Vec4d(x1, y1, x2, y2), seg_len, my});
    }

    if (candidates.empty()) return cv::Vec4d(-1, -1, -1, -1);

    std::sort(candidates.begin(), candidates.end(),
             [](const Candidate& a, const Candidate& b) { return a.len > b.len; });

    size_t top2_count = std::min(size_t(2), candidates.size());
    std::vector<Candidate> best(candidates.begin(), candidates.begin() + top2_count);
    std::sort(best.begin(), best.end(),
             [](const Candidate& a, const Candidate& b) { return a.my < b.my; });

    return best[0].line;
}

UVAlignmentProcessor::MtfResult UVAlignmentProcessor::computeMtfFromCrop(const cv::Mat& crop) {
    MtfResult result;
    if (crop.empty() || crop.cols < 4) return result;

    // ESF: mean down each column
    cv::Mat esf = cv::Mat::zeros(1, crop.cols, CV_32F);
    for (int x = 0; x < crop.cols; ++x) {
        esf.at<float>(0, x) = static_cast<float>(cv::mean(crop.col(x))[0]);
    }
    result.esf = esf.clone();

    // LSF: np.gradient equivalent
    cv::Mat lsf = cv::Mat::zeros(1, crop.cols, CV_32F);
    lsf.at<float>(0, 0) = esf.at<float>(0, 1) - esf.at<float>(0, 0);
    for (int i = 1; i < crop.cols - 1; ++i) {
        lsf.at<float>(0, i) = (esf.at<float>(0, i + 1) - esf.at<float>(0, i - 1)) / 2.0f;
    }
    lsf.at<float>(0, crop.cols - 1) = esf.at<float>(0, crop.cols - 1) - esf.at<float>(0, crop.cols - 2);

    double maxAbsLsf = 0.0;
    for (int i = 0; i < lsf.cols; ++i) {
        maxAbsLsf = std::max(maxAbsLsf, static_cast<double>(std::abs(lsf.at<float>(0, i))));
    }
    if (maxAbsLsf > 1e-6) {
        lsf /= static_cast<float>(maxAbsLsf);
    }
    result.lsf = lsf.clone();

    fftReal1D(lsf, result.mtf);

    int n = lsf.cols;
    result.f_lpmm = cv::Mat::zeros(1, result.mtf.cols, CV_32F);
    for (int k = 0; k < result.mtf.cols; ++k) {
        result.f_lpmm.at<float>(0, k) = static_cast<float>((static_cast<double>(k) / n) / PIXEL_PITCH_MM);
    }

    return result;
}

void UVAlignmentProcessor::fftReal1D(const cv::Mat& input, cv::Mat& magnitude) {
    if (input.empty() || input.cols < 2) {
        magnitude = cv::Mat();
        return;
    }

    int n = input.cols;
    cv::Mat complexMat;
    cv::dft(input, complexMat, cv::DFT_COMPLEX_OUTPUT);

    std::vector<cv::Mat> planes;
    cv::split(complexMat, planes);
    magnitude = cv::Mat::zeros(1, n / 2 + 1, CV_32F);
    for (int i = 0; i <= n / 2 && i < planes[0].cols; ++i) {
        float re = planes[0].at<float>(0, i);
        float im = planes[1].at<float>(0, i);
        magnitude.at<float>(0, i) = std::sqrt(re * re + im * im);
    }

    if (magnitude.at<float>(0, 0) > 1e-6f) {
        magnitude /= magnitude.at<float>(0, 0);
    }
}

double UVAlignmentProcessor::mtfAtLpmm(const cv::Mat& f_lpmm, const cv::Mat& mtf, double target_lpmm) {
    if (f_lpmm.empty() || mtf.empty() || f_lpmm.cols < 2) return 0.0;
    return interp1D(f_lpmm, mtf, target_lpmm);
}

UVAlignmentProcessor::UniformityResult UVAlignmentProcessor::computeUniformity(
    const cv::Mat& img, cv::Point center, int r) {
    UniformityResult result;
    const double nan = std::nan("");

    cv::Rect roi = cv::Rect(center.x - r, center.y - r, 2 * r + 1, 2 * r + 1)
                   & cv::Rect(0, 0, img.cols, img.rows);
    if (roi.area() <= 0) {
        result.p2p = nan; result.rms = nan;
        return result;
    }

    cv::Mat mask = cv::Mat::zeros(roi.size(), CV_8U);
    cv::circle(mask, cv::Point(center.x - roi.x, center.y - roi.y), r, 255, -1);
    if (cv::countNonZero(mask) == 0) {
        result.p2p = nan; result.rms = nan;
        return result;
    }

    cv::Mat sub = img(roi);
    double pmin = 0, pmax = 0;
    cv::minMaxLoc(sub, &pmin, &pmax, nullptr, nullptr, mask);
    cv::Scalar mean, stddev;
    cv::meanStdDev(sub, mean, stddev, mask);

    result.pmax = pmax;
    result.pmin = pmin;
    result.pstd = stddev[0];

    if (pmax < 1e-6) {
        result.p2p = nan; result.rms = nan;
    } else {
        result.p2p = (pmax - pmin) / pmax * 100.0;
        result.rms = (stddev[0] / pmax) * 100.0;
    }
    return result;
}

cv::Mat UVAlignmentProcessor::resizeImage(const cv::Mat& img, int factor) {
    if (factor <= 1) return img;
    cv::Mat resized;
    cv::resize(img, resized, cv::Size(img.cols / factor, img.rows / factor), 0, 0, cv::INTER_AREA);
    return resized;
}

cv::Mat UVAlignmentProcessor::rotateVertical(const cv::Mat& img, const cv::Vec4d& line, cv::Point2d pivot) {
    double angle = std::atan2(line[3] - line[1], line[2] - line[0]) * 180.0 / M_PI;
    double rot_deg = 90.0 + angle;

    int h = img.rows, w = img.cols;
    int cx = w / 2, cy = h / 2;

    cv::Mat M = cv::getRotationMatrix2D(pivot, rot_deg, 1.0);
    M.at<double>(0, 2) += (cx - pivot.x);
    M.at<double>(1, 2) += (cy - pivot.y);

    cv::Mat rotated;
    cv::warpAffine(img, rotated, M, cv::Size(w, h), cv::INTER_LINEAR);
    return rotated;
}

cv::Mat UVAlignmentProcessor::cropCenter(const cv::Mat& img, int half_w, int half_h) {
    int cx = img.cols / 2, cy = img.rows / 2;
    cv::Rect r = cv::Rect(cx - half_w, cy - half_h, 2 * half_w, 2 * half_h)
                 & cv::Rect(0, 0, img.cols, img.rows);
    if (r.area() <= 0) return cv::Mat();
    return img(r).clone();
}

cv::Mat UVAlignmentProcessor::cropAroundPoint(const cv::Mat& img, int x, int y, int half_w, int half_h) {
    cv::Rect r = cv::Rect(x - half_w, y - half_h, 2 * half_w, 2 * half_h)
                 & cv::Rect(0, 0, img.cols, img.rows);
    if (r.area() <= 0) return cv::Mat();
    return img(r).clone();
}

cv::Mat UVAlignmentProcessor::makeDiscKernel(int radius) {
    cv::Mat k = cv::Mat::zeros(radius * 2 + 2, radius * 2 + 2, CV_32F);
    cv::circle(k, cv::Point(radius + 1, radius + 1), radius, cv::Scalar(1.0), -1);
    double s = cv::sum(k)[0];
    if (s > 0) k /= s;
    return k;
}

void UVAlignmentProcessor::dashedLine(cv::Mat& img, cv::Point p1, cv::Point p2, const cv::Scalar& color,
                                      int thickness, double period, double duty) {
    double dist = std::hypot(static_cast<double>(p2.x - p1.x), static_cast<double>(p2.y - p1.y));
    if (dist < 1.0) return;
    double steps = dist / period;
    double sx = (p2.x - p1.x) / steps;
    double sy = (p2.y - p1.y) / steps;
    for (int i = 0; i < static_cast<int>(std::ceil(steps)); ++i) {
        cv::Point s(static_cast<int>(p1.x + sx * i), static_cast<int>(p1.y + sy * i));
        cv::Point e(static_cast<int>(s.x + sx * duty), static_cast<int>(s.y + sy * duty));
        cv::line(img, s, e, color, thickness);
    }
}

// Scale so the brightest pixel becomes 255
cv::Mat UVAlignmentProcessor::normalizeScale(const cv::Mat& img, double& max_val) {
    double minVal = 0, maxVal = 0;
    cv::minMaxLoc(img.reshape(1), &minVal, &maxVal);
    max_val = maxVal;
    cv::Mat out;
    if (maxVal > 0) {
        img.convertTo(out, CV_8U, 255.0 / maxVal);
    } else {
        out = img.clone();
    }
    return out;
}

void UVAlignmentProcessor::precomputeLUT() {
    // Modified JET colormap: ramp = (i/256)^15
    cv::Mat ramp(1, 256, CV_8U);
    for (int i = 0; i < 256; ++i) {
        double v = std::pow(i / 256.0, 15.0);
        ramp.at<uchar>(0, i) = static_cast<uchar>(std::min(255.0, v * 255.0));
    }
    cv::Mat colored;
    cv::applyColorMap(ramp, colored, cv::COLORMAP_JET);
    m_customLUT = colored;
}

// Currently this is single threaded so the entire app hangs while its saving for 2~4 seconds.
// Needs fixing or some GUI waiting status that can mask this problem.
QString UVAlignmentProcessor::saveData() {
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString folderName = QString("UVLA Uniformity Data %1").arg(QDate::currentDate().toString("yyyy-MM-dd"));
    QString folderPath = desktopPath + "/" + folderName;
    QDir().mkpath(folderPath);

    QString timestamp = QDateTime::currentDateTime().toString("HH_mm_ss");

    // CSV
    QFile csvFile(folderPath + "/CaptureData_" + timestamp + ".csv");
    if (csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&csvFile);
        stream << "Time,Date,Camera Serial Number,Centroid (px,px),MTF (lp/mm),"
               << "70 Percent of 11mm Diameter: RMS Homogeneity (%),"
               << "70 Percent of 11mm Diameter: Peak-to-Peak Homogeneity (%)\n";
        stream << timestamp << "," << QDate::currentDate().toString("yyyy-MM-dd") << ",0,"
               << "\"(" << static_cast<int>(m_centroidX) << ", " << static_cast<int>(m_centroidY) << ")\","
               << m_mtf << "," << m_uniformityRMS << "," << m_uniformityPkPk << "\n";
        csvFile.close();
    }

    // JSON
    QJsonObject jsonData;
    jsonData["time"] = timestamp;
    jsonData["date"] = QDate::currentDate().toString("yyyy-MM-dd");
    jsonData["unix_timestamp"] = static_cast<double>(QDateTime::currentDateTime().toSecsSinceEpoch());
    jsonData["camera"] = "0";
    jsonData["centroid_x"] = m_centroidX;
    jsonData["centroid_y"] = m_centroidY;
    jsonData["mtf"] = m_mtf;
    jsonData["uniformity_rms"] = m_uniformityRMS;
    jsonData["uniformity_pkpk"] = m_uniformityPkPk;
    jsonData["uniformity_radius_px"] = m_uniformityRadius;

    QFile jsonFile(folderPath + "/CaptureData_" + timestamp + ".json");
    if (jsonFile.open(QIODevice::WriteOnly)) {
        jsonFile.write(QJsonDocument(jsonData).toJson());
        jsonFile.close();
    }

    // Images (PNG via Qt: grayscale, heatmap, filtered, plus the on-screen view)
    try {
        if (!m_lastRawFrame.empty()) {
            // 1) Grayscale
            saveMatPng(m_lastRawFrame, folderPath + "/Raw_Gray_" + timestamp + ".png");

            // 2) Heatmap (full-res, scaled to max, same LUT as the display)
            cv::Mat gray3;
            cv::cvtColor(m_lastRawFrame, gray3, cv::COLOR_GRAY2BGR);
            double maxVal = 0;
            cv::Mat scaled = normalizeScale(gray3, maxVal);
            cv::Mat heat;
            cv::LUT(scaled, m_customLUT, heat);
            cv::putText(heat, "Scaled to maxval: " + std::to_string(static_cast<int>(maxVal)),
                        cv::Point(20, 60), cv::FONT_HERSHEY_SIMPLEX, 2.0,
                        cv::Scalar(255, 255, 255), 4, cv::LINE_AA);
            saveMatPng(heat, folderPath + "/Heatmap_" + timestamp + ".png");

            // 3) Mean-filtered (full-res, around the beam centroid)
            if (m_centroidX >= 0 && m_centroidY >= 0) {
                if (m_meanKernel.empty()) m_meanKernel = makeDiscKernel(KERNEL_RADIUS);
                cv::Mat filtered = m_lastRawFrame.clone();
                const int icx = static_cast<int>(m_centroidX);
                const int icy = static_cast<int>(m_centroidY);
                const int half = UNIFORMITY_CIRCLE_RADIUS_B + KERNEL_RADIUS + 2;
                cv::Rect roi = cv::Rect(icx - half, icy - half, 2 * half, 2 * half)
                               & cv::Rect(0, 0, filtered.cols, filtered.rows);
                if (roi.area() > 0) {
                    cv::Mat patch = filtered(roi).clone();
                    cv::filter2D(patch, patch, -1, m_meanKernel);
                    patch.copyTo(filtered(roi));
                }
                saveMatPng(filtered, folderPath + "/Filtered_" + timestamp + ".png");
            }
        }
        if (!m_lastDisplayFrame.empty()) {
            saveMatPng(m_lastDisplayFrame, folderPath + "/FullScreen_" + timestamp + ".png");
        }
    } catch (const std::exception& e) {
        qWarning() << "[UVAlignment] Image save failed:" << e.what();
    }

    m_statusMessage = QString("Saved to %1").arg(folderPath);
    emit statusMessageChanged();
    qInfo() << "[UVAlignment] Saved to" << folderPath;
    return folderPath;
}

void UVAlignmentProcessor::setApplyFalseColor(bool val) {
    if (m_applyFalseColor != val) {
        m_applyFalseColor = val;
        qInfo() << "[UVAlignment] False color:" << (val ? "ON" : "OFF");
        emit togglesChanged();
    }
}

void UVAlignmentProcessor::setApplyMeanFilter(bool val) {
    if (m_applyMeanFilter != val) {
        m_applyMeanFilter = val;
        qInfo() << "[UVAlignment] Mean filter:" << (val ? "ON" : "OFF");
        emit togglesChanged();
    }
}