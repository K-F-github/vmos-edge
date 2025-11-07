#pragma once

#include <QObject>
#include <QSize>
#include <QImage>
#include <QHash>
#include <QPointer>
#include <QSharedPointer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include "QtScrcpyCore.h"

class ScrcpyObserver;
class XapkInstaller;

class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager();

    Q_INVOKABLE void connectDevice(const QString &serial);
    Q_INVOKABLE void connectDeviceDirectTcp(const QString &serial, const QString &host, quint16 videoPort, quint16 audioPort, quint16 controlPort);
    Q_INVOKABLE void disconnectDevice(const QString &serial);
    Q_INVOKABLE bool hasDevice(const QString &serial) const;

    // user data & observer bridging
    Q_INVOKABLE bool setUserData(const QString &serial, QObject *userData);
    Q_INVOKABLE QObject* getUserData(const QString &serial) const;

    // device control methods (no device object returned to QML)
    Q_INVOKABLE void goBack(const QString &serial);
    Q_INVOKABLE void goHome(const QString &serial);
    Q_INVOKABLE void goMenu(const QString &serial);
    Q_INVOKABLE void appSwitch(const QString &serial);
    Q_INVOKABLE void power(const QString &serial);
    Q_INVOKABLE void volumeUp(const QString &serial);
    Q_INVOKABLE void volumeDown(const QString &serial);
    Q_INVOKABLE void textInput(const QString &serial, const QString &text);
    Q_INVOKABLE void pushFile(const QString &serial, const QString &file, const QString &devicePath = QString());
    Q_INVOKABLE void installApk(const QString &serial, const QString &apkFile);
    Q_INVOKABLE void installApk(const QString &serial, const QString &apkFile, const QString &adbDeviceAddress);
    Q_INVOKABLE void installXapk(const QString &serial, const QString &xapkFile);
    Q_INVOKABLE void installXapk(const QString &serial, const QString &xapkFile, const QString &adbDeviceAddress);
    Q_INVOKABLE void setDisplayPower(const QString &serial, bool on);
    Q_INVOKABLE void expandNotificationPanel(const QString &serial);
    Q_INVOKABLE void collapsePanel(const QString &serial);
    Q_INVOKABLE void requestDeviceClipboard(const QString &serial);
    Q_INVOKABLE void setDeviceClipboard(const QString &serial, bool pause = true);
    Q_INVOKABLE void clipboardPaste(const QString &serial);
    Q_INVOKABLE void showTouch(const QString &serial, bool show);

    // input events
    Q_INVOKABLE void sendMouseEvent(const QString &serial, int type, int x, int y, int button, int buttons, int modifiers,
                                    int frameWidth, int frameHeight, int showWidth, int showHeight);
    Q_INVOKABLE void sendWheelEvent(const QString &serial, int deltaX, int deltaY, int x, int y, int modifiers,
                                    int frameWidth, int frameHeight, int showWidth, int showHeight);
    Q_INVOKABLE void sendKeyEvent(const QString &serial, int type, int key, int modifiers, const QString &text = QString());

    // others
    Q_INVOKABLE void screenshot(const QString &serial);

    // observer control
    Q_INVOKABLE bool registerObserver(const QString &serial);
    Q_INVOKABLE void deRegisterObserver(const QString &serial);
    // 获取特定设备的 observer 对象，用于直接连接信号（性能优化）
    Q_INVOKABLE QObject* getObserver(const QString &serial) const;
    
    // 注册/注销外部 observer（允许多个 observer 同时存在）
    Q_INVOKABLE bool registerDeviceObserver(const QString &serial, QObject *observer);
    Q_INVOKABLE void unregisterDeviceObserver(const QString &serial, QObject *observer);

signals:
    void deviceConnected(const QString &serial, const QString &deviceName, const QSize &size);
    void deviceDisconnected(const QString &serial);
    void deviceConnectFailed(const QString &serial);
    void newFrame(const QString &serial, const QImage &frame);
    void screenInfo(const QString &serial, int width, int height);
    void fpsUpdated(const QString &serial, int fps);
    void grabCursorChanged(const QString &serial, bool grab);

private slots:
    void onDeviceConnected(bool success, const QString& serial, const QString& deviceName, const QSize& size);
    void onDeviceDisconnected(const QString& serial);
    void emitNewFrame(const QString &serial, const QImage &frame);
    void emitFpsUpdated(const QString &serial, int fps);
    void emitGrabCursorChanged(const QString &serial, bool grab);
    void emitScreenInfo(const QString &serial, int width, int height);

private:
    qsc::IDeviceManage& m_deviceManage;
    QHash<QString, QSharedPointer<ScrcpyObserver>> m_observers;
    QHash<QString, XapkInstaller*> m_xapkInstallers;  // 每个设备的XAPK安装器
    
    // ADB连接状态管理（参考server.cpp的状态机实现）
    enum AdbConnectState {
        ACS_IDLE,
        ACS_CONNECTING,
        ACS_VERIFYING,
        ACS_READY
    };
    QHash<QString, AdbConnectState> m_adbConnectStates;  // 每个设备的连接状态
    QHash<QString, int> m_adbConnectRetryCount;  // 每个设备的重试次数
    static constexpr int MAX_RETRY_COUNT = 3;  // 最大重试次数
    
    // 辅助方法：启动XAPK安装
    void startXapkInstallation(const QString &serial, const QString &xapkFile, QPointer<qsc::IDevice> dev, const QString &adbDeviceAddress);
    
    // ADB连接相关方法（参考server.cpp的实现）
    void connectAdbWithRetry(const QString &serial, const QString &adbDeviceAddress, const QString &apkFile, int retryCount = 0);
    void verifyAdbConnection(const QString &serial, const QString &adbDeviceAddress, const QString &apkFile);
    void startApkInstallationAfterConnect(const QString &serial, const QString &adbDeviceAddress, const QString &absoluteApkPath);
    void connectXapkAdbWithRetry(const QString &serial, const QString &adbDeviceAddress, const QString &xapkFile, QPointer<qsc::IDevice> dev, int retryCount = 0);
    
    // Allow ScrcpyObserver to access manager
    friend class ScrcpyObserver;
    qsc::IDeviceManage* mgr() { return &m_deviceManage; }
};
