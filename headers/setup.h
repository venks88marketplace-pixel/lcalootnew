#pragma once
#include <QObject>
#include "src/core/ServiceLocator.h"


class QQmlApplicationEngine;

class Setup : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString appName READ appName CONSTANT)

public:
    explicit Setup(QObject *parent = nullptr);

    QString appName() const { return m_appName;}

    Q_INVOKABLE void runSetup();


private:
    QQmlApplicationEngine& m_engine;
    explicit Setup(const Setup& rhs) = delete;
    Setup& operator= (const Setup& rhs) = delete;

    QString m_appName;

    ServiceLocator m_serviceLocator;
signals:


};


