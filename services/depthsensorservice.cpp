#include "depthsensorservice.h"

#include <QDebug>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QVector>
#include <algorithm>

// protobuf
#include "neo-calibration-server-api/calibration_server_api.pb.h"

// Calibrated from stepper motor turns per move (raw) data to mm:
// 5,605,250 raw = 2.5659 mm
static constexpr double kMmPerRawUnit = 2.5659 / 5605250.0;
static constexpr int kSampleTimeoutMs = 5000;
static constexpr int kInterPointDelayMs = 2000;
static constexpr int kIfpSettleDelayMs = 3000;

// Median capture config for ALL points
static constexpr int kMedianSampleCount = 9;   // total single-shot captures
static constexpr int kDiscardFirstN = 2;       // discard initial captures as transient
static constexpr int kInterMedianShotDelayMs = 60;

DepthSensorService::DepthSensorService(std::shared_ptr<INetworkService> network,
                                       std::shared_ptr<IStepperControllerService> stepper,
                                       QObject* parent)
    : IDepthSensorService(parent),
      m_network(std::move(network)),
      m_stepper(std::move(stepper))
{
    if (m_stepper) {
        connect(m_stepper.get(), &IStepperControllerService::moveFailed, this,
                [this](const QString& err) {
                    qWarning().noquote() << "[DepthSensorService] moveFailed:" << err;
                    setStatusText("Status: Fail");
                });

        connect(m_stepper.get(), &IStepperControllerService::moveCompleted, this,
                []() { qDebug() << "[DepthSensorService] moveCompleted signal received"; });

        connect(m_stepper.get(), &IStepperControllerService::moveHomeCompleted, this,
                []() { qDebug() << "[DepthSensorService] moveHomeCompleted signal received"; });
    }
}

void DepthSensorService::configure(const QJsonObject& settings)
{
    Q_UNUSED(settings);
}

void DepthSensorService::setBusy(bool b)
{
    if (m_busy == b) return;
    m_busy = b;
    emit busyChanged();
}

void DepthSensorService::setProgress(double p)
{
    if (qFuzzyCompare(m_progress, p)) return;
    m_progress = p;
    emit progressChanged();
}

void DepthSensorService::setStatusText(const QString& s)
{
    if (m_statusText == s) return;
    m_statusText = s;
    emit statusTextChanged();
}

void DepthSensorService::setOutputFilePath(const QString& p)
{
    if (m_outputFilePath == p) return;
    m_outputFilePath = p;
    emit outputFilePathChanged();
}

double DepthSensorService::rawToMm(double raw) const
{
    return raw * kMmPerRawUnit;
}

static bool sendStartRanging(INetworkService* net)
{
    CalibrationApi::ToServer msg;
    msg.mutable_distance_sensor_start_ranging();
    QByteArray data(static_cast<int>(msg.ByteSizeLong()), '\0');
    if (!msg.SerializeToArray(data.data(), data.size())) return false;
    net->sendMessage(data);
    return true;
}

static bool sendStopRanging(INetworkService* net)
{
    CalibrationApi::ToServer msg;
    msg.mutable_distance_sensor_stop_ranging();
    QByteArray data(static_cast<int>(msg.ByteSizeLong()), '\0');
    if (!msg.SerializeToArray(data.data(), data.size())) return false;
    net->sendMessage(data);
    return true;
}

static QString waitForSingleRangingResult(INetworkService* net, int timeoutMs)
{
    if (!net || !net->isConnected()) return "NET_DISCONNECTED";

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QString out = "TIMEOUT";

    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        out = "TIMEOUT";
        loop.quit();
    });

    QMetaObject::Connection c = QObject::connect(
        net, &INetworkService::messageReceived, &loop,
        [&](const QByteArray& bytes) {
            CalibrationApi::FromServer fromServer;
            if (!fromServer.ParseFromArray(bytes.constData(), bytes.size())) return;
            if (!fromServer.has_distance_sensor_ranging_result()) return;

            const double d = fromServer.distance_sensor_ranging_result().distance_to_target_mm();
            if (d < 0.0) return; // ignore sentinel -1

            out = QString::number(d, 'f', 6);
            loop.quit();
        },
        Qt::QueuedConnection);

    if (!sendStartRanging(net)) {
        QObject::disconnect(c);
        return "START_SEND_FAIL";
    }

    timeout.start(timeoutMs);
    loop.exec();

    sendStopRanging(net);
    QObject::disconnect(c);
    return out;
}

// Median built on repeated single-shot captures (start + wait + stop per shot).
static QString waitForRangingMedian(INetworkService* net,
                                    int timeoutMsPerShot,
                                    int sampleCount,
                                    int discardFirstN)
{
    if (!net || !net->isConnected()) return "NET_DISCONNECTED";
    if (sampleCount <= 0) return "INVALID_SAMPLE_COUNT";
    if (discardFirstN < 0) discardFirstN = 0;
    if (discardFirstN >= sampleCount) return "INVALID_DISCARD_COUNT";

    QVector<double> kept;
    kept.reserve(sampleCount - discardFirstN);

    for (int i = 0; i < sampleCount; ++i) {
        const QString shot = waitForSingleRangingResult(net, timeoutMsPerShot);

        bool ok = false;
        const double v = shot.toDouble(&ok);
        if (!ok) return shot; // propagate any of these TIMEOUT / START_SEND_FAIL / NET_DISCONNECTED

        if (i >= discardFirstN) kept.push_back(v);

        if (i + 1 < sampleCount)
            QThread::msleep(kInterMedianShotDelayMs);
    }

    if (kept.isEmpty()) return "NO_VALID_SAMPLES";

    std::sort(kept.begin(), kept.end());
    const int n = kept.size();
    const double median = (n % 2 == 1)
        ? kept[n / 2]
        : 0.5 * (kept[n / 2 - 1] + kept[n / 2]);

    return QString::number(median, 'f', 6);
}

void DepthSensorService::startCalibrationRun()
{
    if (m_busy) return;

    if (!m_stepper || !m_stepper->isOpen()) {
        setStatusText("Status: Fail");
        emit runCompleted(false, "Stepper not available/open");
        setStatusText("");
        return;
    }

    if (!m_network || !m_network->isConnected()) {
        setStatusText("Status: Fail");
        emit runCompleted(false, "Network not connected");
        setStatusText("");
        return;
    }

    setBusy(true);
    setProgress(0.0);
    setStatusText("Status: Homing...");

    QStringList lines;
    lines << "Depth Sensor Sweep Run";
    lines << QString("Timestamp: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    lines << "Flow: Home -> IFP(11mm) -> Up(+1mm x10) -> Home -> IFP(11mm) -> Down(-1mm x10)";
    lines << QString("Ranging capture: median(%1 shots, discard first %2, %3 ms gap)")
                 .arg(kMedianSampleCount)
                 .arg(kDiscardFirstN)
                 .arg(kInterMedianShotDelayMs);
    lines << "";

    m_stepper->moveHome();
    const double rawAfterHome = m_stepper->getCurrentPosition();
    const double mmAfterHome = rawToMm(rawAfterHome);
    lines << QString("HOME,raw=%1,mm=%2").arg(rawAfterHome, 0, 'g', 10).arg(mmAfterHome, 0, 'f', 6);
    QThread::msleep(kInterPointDelayMs);

    const double ifpMm = 11.0;
    setStatusText("Status: Moving to IFP (11.0 mm)...");
    m_stepper->moveToPosition(ifpMm);

    const double ifpRaw = m_stepper->getCurrentPosition();
    const double ifpMmMeasured = rawToMm(ifpRaw);

    setStatusText("Status: Stabilizing at IFP (3s)...");
    QThread::msleep(kIfpSettleDelayMs);

    setStatusText("Status: Capturing IFP ranging (median)...");
    const QString ifpRangingMm = waitForRangingMedian(m_network.get(), kSampleTimeoutMs, kMedianSampleCount, kDiscardFirstN);

    lines << QString("IFP,targetMm=%1,raw=%2,mm=%3,rangingMm=%4")
                .arg(ifpMm, 0, 'f', 4)
                .arg(ifpRaw, 0, 'g', 10)
                .arg(ifpMmMeasured, 0, 'f', 6)
                .arg(ifpRangingMm);

    int completedPoints = 1;
    const int totalPoints = 22;
    setProgress(double(completedPoints) / double(totalPoints));
    QThread::msleep(kInterPointDelayMs);

    lines << "";
    lines << "[UP SWEEP]";
    for (int i = 1; i <= 10; ++i) {
        const double targetMm = ifpMm + i;
        setStatusText(QString("Status: Up sweep %1/10 to %2 mm...").arg(i).arg(targetMm, 0, 'f', 4));
        m_stepper->moveToPosition(targetMm);

        const double rawPos = m_stepper->getCurrentPosition();
        const double mmPos = rawToMm(rawPos);

        setStatusText(QString("Status: Capturing up sweep %1/10 (median)...").arg(i));
        const QString rangingMm = waitForRangingMedian(m_network.get(), kSampleTimeoutMs, kMedianSampleCount, kDiscardFirstN);

        lines << QString("UP,step=%1,targetMm=%2,raw=%3,mm=%4,rangingMm=%5")
                    .arg(i)
                    .arg(targetMm, 0, 'f', 4)
                    .arg(rawPos, 0, 'g', 10)
                    .arg(mmPos, 0, 'f', 6)
                    .arg(rangingMm);

        ++completedPoints;
        setProgress(double(completedPoints) / double(totalPoints));
        QThread::msleep(kInterPointDelayMs);
    }

    lines << "";
    lines << "[DOWN SWEEP]";

    setStatusText("Status: Re-homing before down sweep...");
    m_stepper->moveHome();
    const double rawAfterSecondHome = m_stepper->getCurrentPosition();
    const double mmAfterSecondHome = rawToMm(rawAfterSecondHome);
    lines << QString("HOME_2,raw=%1,mm=%2")
                .arg(rawAfterSecondHome, 0, 'g', 10)
                .arg(mmAfterSecondHome, 0, 'f', 6);

    QThread::msleep(kInterPointDelayMs);

    setStatusText("Status: Moving to IFP (11.0 mm) for down sweep...");
    m_stepper->moveToPosition(ifpMm);

    const double ifp2Raw = m_stepper->getCurrentPosition();
    const double ifp2MmMeasured = rawToMm(ifp2Raw);

    setStatusText("Status: Stabilizing at IFP before down sweep (3s)...");
    QThread::msleep(kIfpSettleDelayMs);

    setStatusText("Status: Capturing IFP ranging before down sweep (median)...");
    const QString ifp2RangingMm = waitForRangingMedian(m_network.get(), kSampleTimeoutMs, kMedianSampleCount, kDiscardFirstN);

    lines << QString("IFP_DOWN_START,targetMm=%1,raw=%2,mm=%3,rangingMm=%4")
                .arg(ifpMm, 0, 'f', 4)
                .arg(ifp2Raw, 0, 'g', 10)
                .arg(ifp2MmMeasured, 0, 'f', 6)
                .arg(ifp2RangingMm);

    ++completedPoints;
    setProgress(double(completedPoints) / double(totalPoints));
    QThread::msleep(kInterPointDelayMs);

    for (int i = 1; i <= 10; ++i) {
        const double targetMm = ifpMm - i;
        setStatusText(QString("Status: Down sweep %1/10 to %2 mm...").arg(i).arg(targetMm, 0, 'f', 4));
        m_stepper->moveToPosition(targetMm);

        const double rawPos = m_stepper->getCurrentPosition();
        const double mmPos = rawToMm(rawPos);

        setStatusText(QString("Status: Capturing down sweep %1/10 (median)...").arg(i));
        const QString rangingMm = waitForRangingMedian(m_network.get(), kSampleTimeoutMs, kMedianSampleCount, kDiscardFirstN);

        lines << QString("DOWN,step=%1,targetMm=%2,raw=%3,mm=%4,rangingMm=%5")
                    .arg(i)
                    .arg(targetMm, 0, 'f', 4)
                    .arg(rawPos, 0, 'g', 10)
                    .arg(mmPos, 0, 'f', 6)
                    .arg(rangingMm);

        ++completedPoints;
        setProgress(double(completedPoints) / double(totalPoints));
        QThread::msleep(kInterPointDelayMs);
    }

    const QString fileName = QString("depth_sweep_%1.txt")
                                 .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString outPath = QDir::current().filePath(fileName);

    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatusText("Status: Fail");
        setBusy(false);
        emit runCompleted(false, QString("Failed to write output file: %1").arg(outPath));
        setStatusText("");
        return;
    }

    QTextStream ts(&outFile);
    for (const QString& line : lines) ts << line << "\n";
    outFile.close();

    setOutputFilePath(outPath);
    setProgress(1.0);
    setStatusText("Status: Pass");
    setBusy(false);
    emit runCompleted(true, QString("Home/IFP/up/home/IFP/down sweep completed (median sampling all points). Output: %1").arg(outPath));
    setStatusText("");
}