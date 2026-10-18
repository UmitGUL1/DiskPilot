#include "controller.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
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
    const auto arguments=app.arguments();
    const auto capture=arguments.indexOf("--capture-screen");
    const auto root=arguments.indexOf("--capture-root");
    if(capture>=0&&capture+1<arguments.size()){
        if(root>=0&&root+1<arguments.size()){
            controller.setTarget(arguments[root+1]);
            controller.scan();
        }
        const auto output=arguments[capture+1];
        QTimer::singleShot(5000,&app,[&engine,output]{
            auto* window=engine.rootObjects().isEmpty()?nullptr:qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
            QCoreApplication::exit(window&&window->grabWindow().save(output,"PNG")?0:2);
        });
    }
    return app.exec();
}

