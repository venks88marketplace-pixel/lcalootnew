#include "stepperservicemock.h"

StepperServiceMock::StepperServiceMock(QObject* parent)
: IStepperControllerService(parent)
{}

void StepperServiceMock::moveToPosition(double position)
{
    m_currentPosition = position;
    emit moveCompleted();
}
void StepperServiceMock::moveHome()
{
    emit moveHomeCompleted();
}
double StepperServiceMock::getCurrentPosition()
{
    return m_currentPosition;
}
bool StepperServiceMock::isOpen()
{
    return true;
}
