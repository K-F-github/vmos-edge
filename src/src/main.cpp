#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QLocale>
#include <QFont>
#include <QSharedMemory>
#include <QLocalServer>
#include <QLocalSocket>
#include <QWindow>
#include <QCoreApplication>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWebEngine/QtWebEngine>
#else
#include <QtWebEngineQuick/QtWebEngineQuick>
#endif

#include "helper/Network.h"
#include "helper/utils.h"
#include "helper/TranslateHelper.h"
#include "helper/SettingsHelper.h"
#include "helper/ReportHelper.h"
#include "helper/keymappermodel.h"
#include "helper/windowsizehelper.h"
#include "helper/AccountModel.h"
#include "helper/DeviceScanner.h"
#include "helper/ImagesModel.h"
#include "helper/FileCopyManager.h"
#include "proxytester.h"

#include "sdk_wrapper/screenshot_image.h"
#include "sdk_wrapper/video_render_item.h"
#include "downloadhandler.h"
#include "deviceproxymodel.h"
#include "selectedlistmodel.h"
#include "treemodel.h"
#include "treeproxymodel.h"
#include "levelproxymodel.h"
#include "helper/TemplateModel.h"
#include "scrcpy_wrapper/devicemanager.h"
#include "scrcpy_wrapper/groupcontroller.h"


int main(int argc, char *argv[])
{
    QSharedMemory sharedMemory("vmoslocal-edge-unique-instance-key");
    // If we can't create the segment, another instance is running.
    if (!sharedMemory.create(1)) {
        qDebug() << "Another instance detected. Signaling and exiting.";
        QLocalSocket socket;
        socket.connectToServer("vmoslocal-edge-ipc-server");
        // 500ms timeout for connection
        if (socket.waitForConnected(500)) {
            qDebug() << "Connected to existing instance.";
            socket.disconnectFromServer();
        } else {
            qDebug() << "Failed to connect to existing instance:" << socket.errorString();
        }
        // Exit anyway
        return 0;
    }
    qDebug() << "This is the first instance.";

#ifdef Q_OS_WIN
    QSettings settings("config.ini", QSettings::IniFormat);
    QString channel = settings.value("CONFIG/channel", "pc").toString();
    QString icon = settings.value("CONFIG/icon", ":/res/vmosedge.ico").toString();
#endif
#ifdef Q_OS_MACOS
    QString channel = "mac";
    QString icon = ":/res/vmosedge.ico";
#endif
#ifdef Q_OS_LINUX
    QString channel = "linux";
    QString icon = ":/res/vmosedge.ico";
#endif
    qDebug() << channel << icon;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QtWebEngine::initialize();
#else
    QtWebEngineQuick::initialize();
#endif
    qDebug() << "platform" << QSysInfo::productType();
    qDebug() << "language" << QLocale::system().name();

    SettingsHelper::getInstance()->init(argv);
    QFont font("Microsoft YaHei", 8);
    QApplication::setFont(font);

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(icon));

    QQuickStyle::setStyle("Fusion");

    // 设置 Cookie
    QQuickWebEngineProfile *profile = QQuickWebEngineProfile::defaultProfile();
    profile->setOffTheRecord(false);
    profile->setStorageName("vmosedge");
    DownloadHandler downloadHandler;
    QObject::connect(profile, &QQuickWebEngineProfile::downloadRequested, &downloadHandler, &DownloadHandler::handleDownload);

    // 注册自定义组件
    const char *uri = "Utils";
    int major = 1;
    int minor = 0;
    qmlRegisterType<NetworkCallable>(uri, major, minor, "NetworkCallable");
    qmlRegisterType<NetworkParams>(uri, major, minor, "NetworkParams");
    qmlRegisterType<VideoRenderItem>(uri, major, minor, "VideoRenderItem");
    qmlRegisterType<DeviceProxyModel>(uri, major, minor, "DeviceProxyModel");
    qmlRegisterType<ScreenshotRenderItem>(uri, major, minor, "ScreenshotRenderItem");
    qmlRegisterType<TreeModel>(uri, major, minor, "TreeModel");
    qmlRegisterType<DeviceScanner>(uri, major, minor, "DeviceScanner");
    qmlRegisterType<DeviceManager>(uri, major, minor, "DeviceManager");
    qmlRegisterType<LevelProxyModel>(uri, major, minor, "LevelProxyModel");
    qmlRegisterType<ImagesModel>(uri, major, minor, "ImagesModel");
    qmlRegisterType<TemplateModel>(uri, major, minor, "TemplateModel");
    qmlRegisterType<ProxyTester>(uri, major, minor, "ProxyTester");



    qDebug() << "唯一ID" << QSysInfo::machineUniqueId();
    qDebug() << QSslSocket::sslLibraryBuildVersionString();
    qDebug() << QSslSocket::sslLibraryVersionString();

    KeyMapperModel keymapperModel;
    TreeModel treeModel;
    TreeProxyModel treeProxyModel;
    treeProxyModel.setSourceModel(&treeModel);
    treeProxyModel.setSortRole(DeviceRoles::NameRole);
    treeProxyModel.sort(0);

    SelectedListModel selectedListModel;
    selectedListModel.setSourceModel(&treeModel);
    DeviceProxyModel proxyModel;
    proxyModel.setSourceModel(&selectedListModel);
    proxyModel.setSortRole(DeviceRoles::NameRole);
    proxyModel.sort(0);
    WindowSizeHelper windowSizeHelper;
    AccountModel accountModel;
    DeviceManager deviceManager;
    ImagesModel imagesModel;
    TemplateModel tempLateModel;



    // 创建qml引擎，执行qml脚本
    QQmlApplicationEngine engine;
    TranslateHelper::getInstance()->init(&engine);
    engine.rootContext()->setContextProperty("channelName", channel);
    engine.rootContext()->setContextProperty("Network", Network::getInstance());
    engine.rootContext()->setContextProperty("Utils", Utils::getInstance());
    engine.rootContext()->setContextProperty("proxyModel", &proxyModel);
    engine.rootContext()->setContextProperty("TranslateHelper", TranslateHelper::getInstance());
    engine.rootContext()->setContextProperty("SettingsHelper", SettingsHelper::getInstance());
    engine.rootContext()->setContextProperty("ReportHelper", ReportHelper::getInstance());
    engine.rootContext()->setContextProperty("treeModel", &treeModel);
    engine.rootContext()->setContextProperty("treeProxyModel", &treeProxyModel);
    engine.rootContext()->setContextProperty("selectedListModel", &selectedListModel);
    engine.rootContext()->setContextProperty("keymapperModel", &keymapperModel);
    engine.rootContext()->setContextProperty("windowSizeHelper", &windowSizeHelper);
    engine.rootContext()->setContextProperty("accountModel", &accountModel);
    engine.rootContext()->setContextProperty("deviceManager", &deviceManager);
    engine.rootContext()->setContextProperty("groupController", &GroupController::instance());
    engine.rootContext()->setContextProperty("imagesModel", &imagesModel);
    engine.rootContext()->setContextProperty("fileCopyManager", FileCopyManager::instance());
    engine.rootContext()->setContextProperty("tempLateModel", &tempLateModel);
    engine.rootContext()->setContextProperty("appDirPath", QCoreApplication::applicationDirPath());

    const QUrl url(QStringLiteral("qrc:/qml/App.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);

    QWindow *mainWindow = nullptr;
    // Try to find the main window in a more robust way
    if (!QApplication::topLevelWindows().isEmpty()) {
        mainWindow = QApplication::topLevelWindows().first();
    } else if (!engine.rootObjects().isEmpty()) {
        // Fallback to engine's root objects if the first method fails
        mainWindow = qobject_cast<QWindow*>(engine.rootObjects().first());
    }

    if (mainWindow) {
        qDebug() << "Main window found.";
    } else {
        qDebug() << "Main window not found!";
    }

    QLocalServer server;
    // When a new instance connects, show the main window
    QObject::connect(&server, &QLocalServer::newConnection, [mainWindow, &server]() {
        qDebug() << "New connection received.";
        if (mainWindow) {
            qDebug() << "Activating main window.";
            mainWindow->showNormal();
            mainWindow->raise();
            mainWindow->requestActivate();
        } else {
            qDebug() << "Cannot activate, main window is null.";
        }
        QLocalSocket *socket = server.nextPendingConnection();
        if (socket) {
            socket->disconnectFromServer();
            socket->deleteLater();
        }
    });
    // Clean up any previous server instances and start listening
    QLocalServer::removeServer("vmoslocal-edge-ipc-server");
    if (!server.listen("vmoslocal-edge-ipc-server")) {
        qDebug() << "Failed to listen on local server:" << server.errorString();
    } else {
        qDebug() << "Listening for new instances.";
    }

    const int exec = app.exec();
    if (exec == 931) {
        QProcess::startDetached(qApp->applicationFilePath(), qApp->arguments());
#ifdef Q_OS_MAC
        QThread::msleep(500);
#endif
    }
    return exec;
}
