#include "AuthService.h"
#include "LoggerService.h"
#include <QUuid>
#include <QDebug>
#include <QString>

void AuthService::signIn(const QString& username, const QString&) {
    if (username.trimmed().isEmpty()) {
        emit signInFailed("Username is empty.");
        qDebug("Sign-in failed: empty username");
        return;
    }
    emit signedIn();
}

