#include "controller.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>

int main(int argc,char* argv[]) {
    QGuiApplication app(argc,argv);
    QCoreApplication::setOrganizationName("DiskPilot");
    QCoreApplication::setApplicationName("DiskPilot");
    QQuickStyle::setStyle("Basic");
    Controller controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("pilot",&controller);
    QObject::connect(&engine,&QQmlApplicationEngine::objectCreationFailed,&app,[]{QCoreApplication::exit(1);},Qt::QueuedConnection);
    engine.loadFromModule("DiskPilotUI","Main");
    if(app.arguments().contains("--smoke-test"))QTimer::singleShot(0,&app,[&engine]{QCoreApplication::exit(engine.rootObjects().isEmpty()?1:0);});
    return app.exec();
}
