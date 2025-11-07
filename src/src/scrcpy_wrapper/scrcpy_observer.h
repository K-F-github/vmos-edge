#pragma once

#include <QObject>
#include <QString>
#include <QImage>
#include <QPointer>
#include "QtScrcpyCore.h"

class DeviceManager;

class ScrcpyObserver : public QObject, public qsc::DeviceObserver
{
    Q_OBJECT
public:
    explicit ScrcpyObserver(DeviceManager *owner, const QString &serial);
    ~ScrcpyObserver() override = default;

    void onFrame(int width, int height, uint8_t* dataY, uint8_t* dataU, uint8_t* dataV, 
                 int linesizeY, int linesizeU, int linesizeV) override;
    void updateFPS(quint32 fps) override;
    void grabCursor(bool grab) override;

    QString serial() const { return m_serial; }

signals:
    // 直接发射信号，不再通过 DeviceManager 广播
    void screenInfo(int width, int height);
    void fpsUpdated(int fps);
    void grabCursorChanged(bool grab);

private slots:
    // 用于线程安全的信号发射
    void doEmitScreenInfo(int width, int height);
    void doEmitFpsUpdated(int fps);
    void doEmitGrabCursorChanged(bool grab);

private:
    QPointer<DeviceManager> m_owner;
    QString m_serial;
    bool m_isFirstFrame;
    int m_lastWidth;
    int m_lastHeight;
    
};

