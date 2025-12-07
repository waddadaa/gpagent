#include "gpagent/ui/qml_register.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QIcon>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("GPAgent");
    app.setOrganizationName("GPAgent");
    app.setOrganizationDomain("gpagent.local");

    // Set application icon
    app.setWindowIcon(QIcon(":/images/icon.png"));

    // Set style
    QQuickStyle::setStyle("Basic");

    // Register QML types
    gpagent::ui::registerQmlTypes();

    QQmlApplicationEngine engine;

    // Add QML import paths
    engine.addImportPath("qrc:/");
    engine.addImportPath(":/Qml");

    const QUrl url(QStringLiteral("qrc:/QmlContent/App.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
