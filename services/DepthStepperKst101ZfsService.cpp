#include "DepthStepperKst101ZfsService.h"
#include "Thorlabs.MotionControl.KCube.StepperMotor.h"
#include <QtDebug>
#include <Windows.h>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cmath>

using namespace sps;
using namespace std::chrono;

DepthStepperKst101ZfsService::DepthStepperKst101ZfsService(const std::string& portNumber,
                                                 std::shared_ptr<ISerialPortService> serialportservice,
                                                 QObject* parent)
    : IStepperControllerService(parent),
      m_serialportservice(std::move(serialportservice)),
      m_portNumber(portNumber)
{
    if (!getDeviceSerialNumber()) {
        qWarning() << "[KST101-DEPTH] Device NOT found (serial lookup failed)";
        m_KST101IsOpen = false;
        return;
    }

    qDebug() << "[KST101-DEPTH] Device S/N:" << m_serialNumber;

    short err = SCC_Open(m_serialNumber);
    if (err != 0) {
        qWarning() << "[KST101-DEPTH] SCC_Open failed, err =" << err;
        m_KST101IsOpen = false;
        return;
    }

    short loadErr = SCC_LoadSettings(m_serialNumber);
    if (loadErr != 0) {
        qWarning() << "[KST101-DEPTH] SCC_LoadSettings failed, err =" << loadErr;
        // Continue anyway; some setups still operate.
    }

    short pollErr = SCC_StartPolling(m_serialNumber, 200);
    if (pollErr == 0) {
        qDebug() << "[KST101-DEPTH] Polling started at 200 ms";
    } else {
        qWarning() << "[KST101-DEPTH] SCC_StartPolling failed, err =" << pollErr;
        // Continue if open; movement may still work depending on SDK behavior.
    }

    m_KST101IsOpen = true;
    qDebug() << "[KST101-DEPTH] Device open state =" << m_KST101IsOpen;
}

void DepthStepperKst101ZfsService::moveToPosition(double mm)
{
    qDebug() << "[MOVE-DEPTH] Request received. target mm =" << mm;

    if (!m_KST101IsOpen || m_serialNumber == nullptr) {
        const QString err = "Depth stepper not open or serial null";
        qWarning().noquote() << "[MOVE-DEPTH]" << err;
        emit moveFailed(err);
        return;
    }

    // Fallback conversion using your measured calibration:
    // 5,605,250 raw = 2.5659 mm
    static constexpr double kRawPerMm = 5605250.0 / 2.5659;

    const int currentDev = SCC_GetPosition(m_serialNumber);
    const int targetDev  = static_cast<int>(std::llround(mm * kRawPerMm));

    qDebug() << "[MOVE-DEPTH] Current device units =" << currentDev;
    qDebug() << "[MOVE-DEPTH] targetDev =" << targetDev
             << "deltaDev =" << (targetDev - currentDev)
             << "(manual mm->dev conversion)";

    SCC_ClearMessageQueue(m_serialNumber);

    const short errMove = SCC_MoveToPosition(m_serialNumber, targetDev);
    if (errMove != 0) {
        const QString err = QString("SCC_MoveToPosition failed err=%1 targetDev=%2")
                                .arg(errMove)
                                .arg(targetDev);
        qWarning().noquote() << "[MOVE-DEPTH]" << err;
        emit moveFailed(err);
        return;
    }

    qDebug() << "[MOVE-DEPTH] Command sent. Waiting for completion message (type=2,id=1) ...";

    if (!waitForMoveComplete(m_serialNumber, 2, 1, std::chrono::seconds(30))) {
        const QString err = QString("Move timeout targetDev=%1").arg(targetDev);
        qWarning().noquote() << "[MOVE-DEPTH]" << err;
        emit moveFailed(err);
        return;
    }

    const int afterDev = SCC_GetPosition(m_serialNumber);
    qDebug() << "[MOVE-DEPTH] Completed. afterDev =" << afterDev
             << "actualDeltaDev =" << (afterDev - currentDev);

    emit moveCompleted();
}

void DepthStepperKst101ZfsService::moveHome()
{
    qDebug() << "[HOME-DEPTH] Request received";

    if (!m_KST101IsOpen || m_serialNumber == nullptr) {
        const QString err = "Depth stepper not open or serial null";
        qWarning().noquote() << "[HOME-DEPTH]" << err;
        emit moveFailed(err);
        return;
    }
    
    Sleep(3000);
    SCC_ClearMessageQueue(m_serialNumber);

    if (!SCC_CanHome(m_serialNumber)) {
        const QString err = "SCC_CanHome returned false";
        qWarning().noquote() << "[HOME-DEPTH]" << err;
        emit moveFailed(err);
        return;
    }

    qDebug() << "[HOME-DEPTH] Starting homing for serial" << m_serialNumber;

    const short homeErr = SCC_Home(m_serialNumber);
    if (homeErr != 0) {
        const QString err = QString("SCC_Home failed err=%1").arg(homeErr);
        qWarning().noquote() << "[HOME-DEPTH]" << err;
        emit moveFailed(err);
        return;
    }

    qDebug() << "[HOME-DEPTH] Command sent. Waiting for completion message (type=2,id=0) ...";

    // Home complete = (messageType=2, messageId=0)
    if (!waitForMoveComplete(m_serialNumber, 2, 0, seconds(60))) {
        const QString err = "Home timeout";
        qWarning().noquote() << "[HOME-DEPTH]" << err;
        emit moveFailed(err);
        return;
    }

    const int posAfterHome = SCC_GetPosition(m_serialNumber);
    qDebug() << "[HOME-DEPTH] Completed. positionDev =" << posAfterHome;

    emit moveHomeCompleted();
}

double DepthStepperKst101ZfsService::getCurrentPosition()
{
    if (!m_KST101IsOpen || m_serialNumber == nullptr) {
        qWarning() << "[POS-DEPTH] getCurrentPosition called while device unavailable";
        return 0.0;
    }

    const int pos = SCC_GetPosition(m_serialNumber); // raw device units
    qDebug() << "[POS-DEPTH] current device units =" << pos;
    return static_cast<double>(pos);
}

bool DepthStepperKst101ZfsService::waitForMoveComplete(char* serialNo,
                                                  WORD msgType,
                                                  WORD msgId,
                                                  std::chrono::seconds timeout)
{
    WORD messageType = 0;
    WORD messageId = 0;
    DWORD messageData = 0;

    auto startTime = getCurrentTime();

    while (true) {
        SCC_WaitForMessage(serialNo, &messageType, &messageId, &messageData);

        if (messageType == msgType && messageId == msgId) {
            qDebug() << "[WAIT-DEPTH] Command complete (type/id matched).";
            return true;
        }

        const float elapsed = (getCurrentTime() - startTime).count();
        if (elapsed > timeout.count()) {
            qWarning() << "[WAIT-DEPTH] TIMEOUT after" << elapsed << "seconds"
                       << "(expected type/id =" << msgType << "/" << msgId << ")";
            return false;
        }
    }
}

bool DepthStepperKst101ZfsService::getDeviceSerialNumber()
{
    char sns[1000];
    short err;

    if ((err = TLI_BuildDeviceList()) != 0) {
        qWarning() << "[KST101-DEPTH] TLI_BuildDeviceList failed err =" << err;
        return false;
    }

    if ((err = TLI_GetDeviceListExt(sns, sizeof(sns))) != 0) {
        qWarning() << "[KST101-DEPTH] TLI_GetDeviceListExt failed err =" << err;
        return false;
    }

    qDebug() << "[KST101-DEPTH] Raw serial list =" << sns;

    char* context = nullptr;
    char* firstNumber = strtok_s(sns, ",", &context);
    if (firstNumber == nullptr) {
        qWarning() << "[KST101-DEPTH] No serial found";
        return false;
    }

    const size_t tokenSize = strlen(firstNumber);
    m_serialNumber = static_cast<char*>(malloc(tokenSize + 1));
    if (!m_serialNumber) {
        qWarning() << "[KST101-DEPTH] malloc failed for serial";
        return false;
    }

    memcpy(m_serialNumber, firstNumber, tokenSize);
    m_serialNumber[tokenSize] = '\0';

    qDebug() << "[KST101-DEPTH] Selected serial =" << m_serialNumber;
    return true;
}

bool DepthStepperKst101ZfsService::isOpen()
{
    return m_KST101IsOpen;
}