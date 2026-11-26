#include <QDir>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <iostream>

#include "AppController.hpp"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Nightingale"));
    app.setApplicationName(QStringLiteral("Nightingale"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    AppController controller;
    if (!controller.initialize()) {
        std::cerr << "An error has occurred while trying to initialize Nightingale.\n";
        return 1;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);

    const QString uiDir = QStringLiteral(UI_SOURCE_DIR);
    const QString mainQmlPath = uiDir + QStringLiteral("/Main.qml");
    const QUrl mainQmlUrl = QUrl::fromLocalFile(mainQmlPath);

    std::cout << "[QML Hot-Reload] Vigilando: " << mainQmlPath.toStdString() << '\n';

    QFileSystemWatcher watcher;
    auto refreshWatchPaths = [&]() {
        if (!watcher.directories().isEmpty()) {
            watcher.removePaths(watcher.directories());
        }
        if (!watcher.files().isEmpty()) {
            watcher.removePaths(watcher.files());
        }

        if (QDir(uiDir).exists()) {
            watcher.addPath(uiDir);
        }

        QDirIterator it(
            uiDir,
            QStringList{QStringLiteral("*.qml")},
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
            QDirIterator::Subdirectories
        );
        while (it.hasNext()) {
            watcher.addPath(it.next());
        }
    };

    QObject* activeWindow = nullptr;

    auto reloadUi = [&]() {
        refreshWatchPaths();
        engine.clearComponentCache();

        QQmlComponent component(&engine, mainQmlUrl, QQmlComponent::PreferSynchronous);
        if (component.isError()) {
            std::cerr << "\n[QML Hot-Reload] Error de compilación (se mantiene la ventana anterior):\n";
            for (const QQmlError& err : component.errors()) {
                std::cerr << "  " << err.toString().toStdString() << '\n';
            }
            return;
        }

        QObject* newWindow = component.create(engine.rootContext());
        if (!newWindow) {
            std::cerr << "[QML Hot-Reload] No se pudo instanciar Main.qml.\n";
            return;
        }

        QMetaObject::invokeMethod(newWindow, "show");

        if (activeWindow) {
            activeWindow->deleteLater();
        }
        activeWindow = newWindow;
        std::cout << "[QML Hot-Reload] UI recargada.\n";
    };

    QTimer reloadTimer;
    reloadTimer.setSingleShot(true);
    reloadTimer.setInterval(120);
    QObject::connect(&reloadTimer, &QTimer::timeout, &engine, reloadUi);

    QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged, &reloadTimer, qOverload<>(&QTimer::start));
    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, &reloadTimer, qOverload<>(&QTimer::start));

    reloadUi();
    if (!activeWindow) {
        return -1;
    }

    return app.exec();
}