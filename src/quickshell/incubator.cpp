#include "incubator.h"

void QsQmlIncubator::statusChanged(QQmlIncubator::Status status)
{
    if (status == QQmlIncubator::Ready)
        emit this->completed();
    else if (status == QQmlIncubator::Error)
        emit this->failed();
}

void QsIncubationController::incubatingObjectCountChanged(int /*count*/)
{
}