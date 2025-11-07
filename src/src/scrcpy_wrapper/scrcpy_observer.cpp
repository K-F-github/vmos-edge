#include "scrcpy_observer.h"
#include "devicemanager.h"
#include "../sdk_wrapper/video_render_sink.h"
#include "../sdk_wrapper/video_frame.h"
#include <QImage>
#include <QMetaObject>
#include <libyuv.h>

ScrcpyObserver::ScrcpyObserver(DeviceManager *owner, const QString &serial)
    : QObject(owner)  // 将 DeviceManager 作为父对象
    , m_owner(owner)
    , m_serial(serial)
    , m_isFirstFrame(false)
    , m_lastWidth(0)
    , m_lastHeight(0)
{
}

void ScrcpyObserver::onFrame(int width, int height, uint8_t* dataY, uint8_t* dataU, uint8_t* dataV, 
                             int linesizeY, int linesizeU, int linesizeV)
{
    if (!m_owner) return;

    // Check if userData is a VideoRenderSink and call it directly (more efficient)
    if (m_owner) {
        auto dev = m_owner->mgr()->getDevice(m_serial);
        if (dev && dev->getUserData()) {
            auto userData = static_cast<QObject*>(dev->getUserData());
            // VideoRenderItem inherits from both QQuickPaintedItem (QObject) and VideoRenderSink
            // Try to cast to VideoRenderSink
            auto* sink = dynamic_cast<armcloud::VideoRenderSink*>(userData);
            if (sink) {
                // Create VideoFrame directly and call sink
                auto videoFrame = std::make_shared<armcloud::VideoFrame>(width, height, armcloud::PixelFormat::ARGB);
                libyuv::I420ToARGB(dataY, linesizeY,
                                   dataU, linesizeU,
                                   dataV, linesizeV,
                                   videoFrame->buffer(0), videoFrame->stride(0),
                                   width, height);
                
                // Call sink directly - VideoRenderSink implementations handle their own thread safety
                sink->onFrame(videoFrame);
                
                // Also emit signal for QML if needed
                QImage image(width, height, QImage::Format_ARGB32);
                libyuv::I420ToARGB(dataY, linesizeY,
                                   dataU, linesizeU,
                                   dataV, linesizeV,
                                   reinterpret_cast<uint8_t*>(image.bits()), image.bytesPerLine(),
                                   width, height);
                QMetaObject::invokeMethod(m_owner, "emitNewFrame", Qt::QueuedConnection,
                                          Q_ARG(QString, m_serial),
                                          Q_ARG(QImage, image));
                
                // 检测屏幕尺寸变化（第一帧或尺寸改变时发射 screenInfo 信号）
                // 这样可以检测到屏幕旋转（比如打开横屏游戏时）
                if (!m_isFirstFrame || m_lastWidth != width || m_lastHeight != height) {
                    m_isFirstFrame = true;
                    m_lastWidth = width;
                    m_lastHeight = height;
                    // 直接发射信号，不需要通过 DeviceManager
                    QMetaObject::invokeMethod(this, "doEmitScreenInfo", Qt::QueuedConnection,
                                              Q_ARG(int, width),
                                              Q_ARG(int, height));
                }
                return;
            }
        }
    }

    // Fallback: convert to QImage (如果需要通过信号发送)
    // 注意：如果 VideoRenderSink 已设置，就不需要这个分支了
    // 但保留作为备用

    // 检测屏幕尺寸变化（第一帧或尺寸改变时发射 screenInfo 信号）
    if (!m_isFirstFrame || m_lastWidth != width || m_lastHeight != height) {
        m_isFirstFrame = true;
        m_lastWidth = width;
        m_lastHeight = height;
        // 直接发射信号，不需要通过 DeviceManager
        QMetaObject::invokeMethod(this, "doEmitScreenInfo", Qt::QueuedConnection,
                                  Q_ARG(int, width),
                                  Q_ARG(int, height));
    }
}

void ScrcpyObserver::updateFPS(quint32 fps)
{
    // 直接发射信号，不需要通过 DeviceManager
    QMetaObject::invokeMethod(this, "doEmitFpsUpdated", Qt::QueuedConnection,
                              Q_ARG(int, static_cast<int>(fps)));
}

void ScrcpyObserver::grabCursor(bool grab)
{
    // 直接发射信号，不需要通过 DeviceManager
    QMetaObject::invokeMethod(this, "doEmitGrabCursorChanged", Qt::QueuedConnection,
                              Q_ARG(bool, grab));
}

void ScrcpyObserver::doEmitScreenInfo(int width, int height)
{
    emit screenInfo(width, height);
}

void ScrcpyObserver::doEmitFpsUpdated(int fps)
{
    emit fpsUpdated(fps);
}

void ScrcpyObserver::doEmitGrabCursorChanged(bool grab)
{
    emit grabCursorChanged(grab);
}
