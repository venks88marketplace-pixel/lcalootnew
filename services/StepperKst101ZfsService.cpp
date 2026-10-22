#include "StepperKst101ZfsService.h"
#include "Thorlabs.MotionControl.KCube.StepperMotor.h"
#include <QtDebug>
#include <Windows.h>
#include <chrono>
#include <cstring>

using namespace sps;

StepperKst101ZfsService::StepperKst101ZfsService( const std::string& portNumber,
                    std::shared_ptr<ISerialPortService> serialportservice,
                    QObject* parent)
    : IStepperControllerService(parent),
        m_serialportservice(std::move(serialportservice)),
        m_portNumber(portNumber)
{
    if (!getDeviceSerialNumber() ) {
        qDebug() << "KST101 device NOT found!";
        m_KST101IsOpen = false;
        return;
    }
    qDebug() << "KST device found! S/N: " << m_serialNumber;

    short err = SCC_Open(m_serialNumber);
    if (err == 0) {
        SCC_LoadSettings(m_serialNumber);
        // start the device polling at 200ms intervals.
        //make sure to stop polling on application close
        SCC_StartPolling(m_serialNumber, 200);
        Sleep(3000);
        qDebug() << "KST101 device is open and polling.";
        m_KST101IsOpen = true;
    }
}

void StepperKst101ZfsService::moveToPosition(double mm)
{
   qDebug() << "move to position received!";
   int devUnits;
   short err{0};
   SCC_GetDeviceUnitFromRealValue(
       m_serialNumber,
       mm,
       &devUnits,
       0);
   qDebug() << "Device moving to position:"
            << mm << "devUnits:" << devUnits;
    SCC_ClearMessageQueue(m_serialNumber);
   err = SCC_MoveToPosition(
       m_serialNumber,
       devUnits);

   WORD messageType = 2;
   WORD messageId = 1;
   waitForMoveComplete(
       m_serialNumber,
       messageType,
       messageId,
       seconds(20));
   emit moveCompleted();
}

void StepperKst101ZfsService::moveHome() {
    qDebug() << "move home received!";
    if (!m_KST101IsOpen) {
        //log
        //throw exception
        return;
    }
    SCC_ClearMessageQueue(m_serialNumber);

    if (SCC_CanHome(m_serialNumber)) {
        qDebug() << "starting Device homing " << m_serialNumber;
        SCC_Home(m_serialNumber);
    }
    else{
        //throw an exception;
        return;
    }
    WORD messageType = 2;
    WORD messageId = 0;
    waitForMoveComplete(m_serialNumber,
                        messageType,
                        messageId,
                        seconds(45));
    emit moveHomeCompleted();
}

double StepperKst101ZfsService::getCurrentPosition() {
    return 0.0;
}

bool StepperKst101ZfsService::waitForMoveComplete(
                            char* serialNo,
                            WORD msgType,
                             WORD msgId,
                            std::chrono::seconds timeout) {
    WORD messageType;
    WORD messageId;
    DWORD messageData;
    auto startTime = getCurrentTime();
    SCC_WaitForMessage(serialNo,
                       &messageType,
                       &messageId,
                       &messageData);
    while (messageType != msgType || messageId != msgId) {
        SCC_WaitForMessage(serialNo,
                           &messageType,
                           &messageId,
                           &messageData);
        qDebug() << "Waiting in while loop: MessageType: : "
                 << messageType << "  MessageId: " << messageId;
        float elapsed = (getCurrentTime() - startTime).count();
        if (elapsed > timeout.count()) {
            qDebug() << "Waiting in while loop TIMEOUT";
            return false;
        }
        qDebug() << "waiting: " << elapsed;
    }
    qDebug() << "Command complete.";
    return true;
}

bool StepperKst101ZfsService::getDeviceSerialNumber(){
    char sns[1000];
    short err;
    char* firstNumber;
    try {
    if ((err = TLI_BuildDeviceList()) == 0) {
        if ((err = TLI_GetDeviceListExt(sns, sizeof(sns))) == 0) {
            if (sns[0] == '\0') {
                m_serialNumber = NULL;
                return false;
            }
            char delimiters[] = ",";
            char* context = nullptr;
            //get the first one - only one is expected
            firstNumber = strtok_s(sns,
                                      delimiters,
                                      &context);
            if (firstNumber == NULL) {
                qDebug() << "testSerialNo is null = " << err;
                //throw device not found
                return false;
            }
            size_t tokenSize = strlen(firstNumber);
            m_serialNumber = (char*)malloc(tokenSize+1);
            for (int i = 0; i < strlen(firstNumber); i++)
                m_serialNumber[i] = firstNumber[i];
            m_serialNumber[tokenSize] = '\0';
        }
        else {
            qDebug() << "TLI_GetDeviceListExt = " << err;
            return false;
        }
    }
    else {
        qDebug() << "TLI_BuildDeviceList = " << err;
        return false;
    }
    }
    catch(std::exception &ex) {
        std::string err = ex.what();
        throw ex;
    }
    return true;
}

bool StepperKst101ZfsService::isOpen() {
    return m_KST101IsOpen;
}
