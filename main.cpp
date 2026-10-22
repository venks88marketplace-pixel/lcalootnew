#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QLoggingCategory>
#include <QtGlobal>
#include <QtQml>
#include <QQmlContext>
#include <QThread>
#include <QDir>
#include <QSerialPortInfo>
#include <QtQml/qqml.h>
#include "src/core/appcontext.h"
#include "src/services/StepperKst101ZfsService.h"
#include "src/services/DepthStepperKst101ZfsService.h"
#include "pixmapimage.h"
#include "src/core/serviceregistry.h"
#include "src/models/Session.h"
#include "src/models/MetaData.h"
#include "src/models/ShapeModel.h"
#include "src/AppConfig.h"
#include "src/configloader.h"
#include "src/core/servicebootstrap.h"

using namespace sps;

static void msgHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const char* t =
        type == QtDebugMsg ? "DBG" :
            type == QtInfoMsg ? "INF" :
            type == QtWarningMsg ? "WRN" :
            type == QtCriticalMsg ? "CRT" : "FTL";

    QString where;
    if (ctx.file) where = QString("%1:%2 ").arg(ctx.file).arg(ctx.line);
    fprintf(stderr, "[%s] %s%s\n", t, where.toUtf8().constData(), msg.toUtf8().constData());
}

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);

    qInstallMessageHandler(msgHandler);
    // Optional: turn on useful QML logging categories
    QLoggingCategory::setFilterRules(
        "qt.qml.import=true\n"
        "qt.qml.module=true\n"
        "qt.qml.binding.removal=false\n"
        );
    QQmlApplicationEngine engine;

    //test
    for (const auto& info : QSerialPortInfo::availablePorts())
    {
        qDebug() << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << info.portName()
        << info.description()
        << info.manufacturer()
        <<info.serialNumber();
    }

    QString fsConfig =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/appconfig.json";

    const QString configPath =
        qEnvironmentVariableIsSet("USE_QRC_CONFIG") || !QFile::exists(fsConfig)
            ? QStringLiteral(":/config/default.json")
            : fsConfig;

    AppConfig config(loadConfig(configPath));
    ServiceBootstrap::registerFactories(config.json());
    ServiceRegistry registry;
    registry.configure(config.json());

    AppContext appContext(registry);
    engine.rootContext()->setContextProperty("appContext", &appContext);

    qmlRegisterType<Session>("App", 1, 0, "Session");
    qmlRegisterType<MetaData>("App", 1, 0, "MetaData");

    qmlRegisterType<StepperKst101ZfsService>("App", 1, 0, "StepperKst101ZfsService");
    qmlRegisterType<DepthStepperKst101ZfsService>("App", 1, 0, "DepthStepperKst101ZfsService");
    qmlRegisterType<PixmapImage>("App", 1, 0, "PixmapImage");
    qmlRegisterType<ShapeModel>("App", 1, 0, "ShapeModel");
    qmlRegisterUncreatableType<imaging::IImageService>("App", 1, 0, "IImageService", "Interface only");

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
         &app, [&](QObject *obj, const QUrl &objUrl) {
             if (!obj) {
                 qCritical() << "Root object creation failed. url:" << objUrl;
                 QCoreApplication::exit(-1);
             }
         }, Qt::QueuedConnection);

    engine.loadFromModule("App", "App");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    return app.exec();
}