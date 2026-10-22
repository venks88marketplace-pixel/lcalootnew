#pragma once

#include "BaseProcessor.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QPixmap>
#include <QStandardPaths>
#include <QDir>
#include <complex>
#include <vector>

class UVAlignmentProcessor : public BaseProcessor {
    Q_OBJECT

    Q_PROPERTY(double centroidX READ centroidX NOTIFY dataUpdated)
    Q_PROPERTY(double centroidY READ centroidY NOTIFY dataUpdated)
    Q_PROPERTY(double mtf READ mtf NOTIFY dataUpdated)
    Q_PROPERTY(double uniformityPkPk READ uniformityPkPk NOTIFY dataUpdated)
    Q_PROPERTY(double uniformityRMS READ uniformityRMS NOTIFY dataUpdated)
    Q_PROPERTY(int uniformityRadius READ uniformityRadius NOTIFY dataUpdated)
    Q_PROPERTY(bool applyFalseColor READ applyFalseColor WRITE setApplyFalseColor NOTIFY togglesChanged)
    Q_PROPERTY(bool applyMeanFilter READ applyMeanFilter WRITE setApplyMeanFilter NOTIFY togglesChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit UVAlignmentProcessor(QString name,
                                  std::shared_ptr<ILoggerService> logger,
                                  std::shared_ptr<IServerProxyService> proxyService,
                                  std::shared_ptr<imaging::IImageService> imageService,
                                  std::shared_ptr<IStepperControllerService> stepperService,
                                  QObject* parent = nullptr);

    ~UVAlignmentProcessor() override = default;

    Q_INVOKABLE virtual void execute(const QString& action) override;
    Q_INVOKABLE virtual void save() override;
    Q_INVOKABLE void processFrame(const QPixmap& frame);
    Q_INVOKABLE void toggleFalseColor() { setApplyFalseColor(!applyFalseColor()); }
    Q_INVOKABLE void toggleMeanFilter() { setApplyMeanFilter(!applyMeanFilter()); }
    Q_INVOKABLE QString saveData();

    // Getters
    double centroidX() const { return m_centroidX; }
    double centroidY() const { return m_centroidY; }
    double mtf() const { return m_mtf; }
    double uniformityPkPk() const { return m_uniformityPkPk; }
    double uniformityRMS() const { return m_uniformityRMS; }
    int uniformityRadius() const { return m_uniformityRadius; }
    bool applyFalseColor() const { return m_applyFalseColor; }
    bool applyMeanFilter() const { return m_applyMeanFilter; }
    QString statusMessage() const { return m_statusMessage; }

    // Setters
    void setApplyFalseColor(bool val);
    void setApplyMeanFilter(bool val);

signals:
    void dataUpdated();
    void togglesChanged();
    void statusMessageChanged();
    void displayFrameReady(const QPixmap& image);   // main annotated view
    void cropFrameReady(const QPixmap& image);      // rotated edge strip used for MTF

private:
    // Constants
    static constexpr int RESIZE_FACTOR = 5;
    static constexpr int GAUSS_KSIZE_X = 5;
    static constexpr int GAUSS_KSIZE_Y = 5;
    static constexpr double GAUSS_SIGMA = 1.0;
    static constexpr int CANNY_LO = 0;
    static constexpr int CANNY_HI = 50;
    static constexpr int HOUGH_RHO = 1;
    static constexpr double HOUGH_THETA = M_PI / 180.0;
    static constexpr int HOUGH_THRESH = 120;
    static constexpr int HOUGH_MIN_LINE = 100;
    static constexpr int HOUGH_MAX_GAP = 50;
    static constexpr int CROP_HALF_H = 150;
    static constexpr int CROP_HALF_W = 50;
    static constexpr double PIXEL_PITCH_MM = 0.00274; // 2.74 um
    static constexpr double TARGET_LPMM = 1.5;
    static constexpr int UNIFORMITY_CIRCLE_RADIUS_B = 946; // 5.189mm diameter / PIXEL_PITCH_MM / 2
    static constexpr int KERNEL_RADIUS = 277;
    static constexpr int DOWNSCALE_FACTOR = 7;
    static constexpr qint64 MIN_FRAME_INTERVAL_MS = 66;     // ~15 fps analysis

    // Image conversion
    cv::Mat QPixmapToCvMat(const QPixmap& pixmap);
    QPixmap matToPixmap(const cv::Mat& m);
    cv::Mat toMono8(const cv::Mat& img);

    // Core processing
    bool processPipeline(const cv::Mat& img8);
    void abortProcessing(const cv::Mat& img8);

    // Centroid detection
    std::pair<double, double> findCentroid(const cv::Mat& img);

    // Edge detection & line finding
    cv::Vec4d pickUpperRightSimple(const cv::Mat& edges, double target_deg = 15.0,
                                   double angle_tol = 20.0, double min_len_px = 60,
                                   const cv::Point2d* require_right_of = nullptr);
    static double foldedAngleDeg(double dx, double dy);
    static double interp1D(const cv::Mat& x, const cv::Mat& y, double xi);

    // MTF computation
    struct MtfResult {
        cv::Mat esf;
        cv::Mat lsf;
        cv::Mat f_lpmm;
        cv::Mat mtf;
    };
    MtfResult computeMtfFromCrop(const cv::Mat& crop);
    double mtfAtLpmm(const cv::Mat& f_lpmm, const cv::Mat& mtf, double target_lpmm);
    void fftReal1D(const cv::Mat& input, cv::Mat& magnitude);

    // Uniformity
    struct UniformityResult {
        double p2p = 0.0;
        double rms = 0.0;
        double pmax = 0.0;
        double pmin = 0.0;
        double pstd = 0.0;
    };
    UniformityResult computeUniformity(const cv::Mat& img, cv::Point center, int r);

    // Image processing helpers
    cv::Mat resizeImage(const cv::Mat& img, int factor);
    cv::Mat rotateVertical(const cv::Mat& img, const cv::Vec4d& line, cv::Point2d pivot);
    cv::Mat cropCenter(const cv::Mat& img, int half_w, int half_h);
    cv::Mat cropAroundPoint(const cv::Mat& img, int x, int y, int half_w, int half_h);

    // Visualization
    static cv::Mat makeDiscKernel(int radius);
    static void dashedLine(cv::Mat& img, cv::Point p1, cv::Point p2, const cv::Scalar& color,
                           int thickness, double period, double duty);
    cv::Mat normalizeScale(const cv::Mat& img, double& max_val);
    void precomputeLUT();

    // Member variables
    double m_centroidX = -99.0;
    double m_centroidY = -99.0;
    double m_mtf = 0.0;
    double m_uniformityPkPk = 0.0;
    double m_uniformityRMS = 0.0;
    int m_uniformityRadius = UNIFORMITY_CIRCLE_RADIUS_B;
    bool m_applyFalseColor = false;
    bool m_applyMeanFilter = false;
    QString m_statusMessage;

    cv::Mat m_customLUT;       // 256x1 BGR colormap
    cv::Mat m_meanKernel;      // accurate kernel (full res)
    cv::Mat m_cosmeticKernel;  // display kernel (downscaled)

    cv::Mat m_lastRawFrame;
    cv::Mat m_lastFilteredFrame;
    cv::Mat m_lastDisplayFrame;  // BGR

    QElapsedTimer m_throttle;
    QJsonObject m_lastData;
};