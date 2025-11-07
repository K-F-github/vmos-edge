#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMutexLocker>

#include "devicemanage.h"
#include "device.h"
#include "demuxer.h"

namespace qsc {

#define DM_MAX_DEVICES_NUM 1000

IDeviceManage& IDeviceManage::getInstance() {
    static DeviceManage dm;
    return dm;
}

DeviceManage::DeviceManage() {
    Demuxer::init();
}

DeviceManage::~DeviceManage() {
    Demuxer::deInit();
}

QPointer<IDevice> DeviceManage::getDevice(const QString &serial)
{
    if (!m_devices.contains(serial)) {
        return QPointer<IDevice>();
    }
    return m_devices[serial];
}

bool DeviceManage::connectDevice(qsc::DeviceParams params)
{
    if (params.serial.trimmed().isEmpty()) {
        return false;
    }
    
    // 使用互斥锁保护端口分配和设备添加，避免并发冲突
    QMutexLocker locker(&m_portMutex);
    
    // 如果设备已存在，先断开并清理，以便重新连接
    if (m_devices.contains(params.serial)) {
        auto existingDevice = m_devices[params.serial];
        if (existingDevice) {
            // 设备存在且有效，不允许重复连接
            qWarning() << "Device already connected:" << params.serial;
            return false;
        } else {
            // 设备存在但为 nullptr（可能是之前的连接失败留下的），清理它
            qInfo() << "Cleaning up stale device entry:" << params.serial;
            m_devices.remove(params.serial);
            if (m_allocatedPorts.contains(params.serial)) {
                m_allocatedPorts.remove(params.serial);
            }
        }
    }
    if (DM_MAX_DEVICES_NUM < m_devices.size()) {
        qInfo("over the maximum number of connections");
        return false;
    }
    // 为每个设备分配独立的本地端口，避免端口冲突
    // forward 模式和 reverse 模式都需要独立的端口
    quint16 port = getFreePort();
    if (0 == port) {
        qWarning("no port available, cannot connect device");
        return false;
    }
    params.localPort = port;
    
    // 记录已分配的端口，确保并发时不会冲突
    m_allocatedPorts[params.serial] = port;
    
    // 先添加到 m_devices（使用 nullptr），确保端口被标记为已使用
    // 这样即使后续连接失败，端口也会在 removeDevice 时被释放
    m_devices[params.serial] = nullptr;
    
    locker.unlock();  // 释放锁，避免在设备连接过程中阻塞其他操作
    
    qInfo("allocated port %d for device %s", port, params.serial.toStdString().c_str());
    IDevice *device = new Device(params);
    connect(device, &Device::deviceConnected, this, &DeviceManage::onDeviceConnected);
    connect(device, &Device::deviceDisconnected, this, &DeviceManage::onDeviceDisconnected);
    if (!device->connectDevice()) {
        // 连接失败，需要清理
        locker.relock();  // 重新加锁
        m_devices.remove(params.serial);  // 从 m_devices 中移除
        m_allocatedPorts.remove(params.serial);  // 释放端口
        locker.unlock();
        delete device;
        return false;
    }
    
    // 连接成功，更新 m_devices 中的设备指针
    locker.relock();
    m_devices[params.serial] = device;
    locker.unlock();
    
    return true;
}

bool DeviceManage::disconnectDevice(const QString &serial)
{
    bool ret = false;
    if (!serial.isEmpty() && m_devices.contains(serial)) {
        auto it = m_devices.find(serial);
        if (it->data()) {
            delete it->data();
            ret = true;
        }
    }
    return ret;
}

void DeviceManage::disconnectAllDevice()
{
    QMapIterator<QString, QPointer<IDevice>> i(m_devices);
    while (i.hasNext()) {
        i.next();
        if (i.value()) {
            delete i.value();
        }
    }
}

void DeviceManage::onDeviceConnected(bool success, const QString &serial, const QString &deviceName, const QSize &size)
{
    emit deviceConnected(success, serial, deviceName, size);
    if (!success) {
        removeDevice(serial);
    }
}

void DeviceManage::onDeviceDisconnected(QString serial)
{
    emit deviceDisconnected(serial);
    removeDevice(serial);
}

quint16 DeviceManage::getFreePort()
{
    quint16 port = m_localPortStart;
    while (port < m_localPortStart + DM_MAX_DEVICES_NUM) {
        bool used = false;
        
        // 首先检查已分配的端口（包括未启动的设备）
        for (auto it = m_allocatedPorts.constBegin(); it != m_allocatedPorts.constEnd(); ++it) {
            if (it.value() == port) {
                used = true;
                break;
            }
        }
        
        // 如果没有在已分配列表中，再检查已启动设备的实际端口
        if (!used) {
            QMapIterator<QString, QPointer<IDevice>> i(m_devices);
            while (i.hasNext()) {
                i.next();
                auto device = i.value();
                if (device) {
                    // 检查所有模式的端口使用情况（reverse 和 forward 都需要独立端口）
                    quint16 devicePort = device->getLocalPort();
                    if (devicePort == port && devicePort != 0) {
                        used = true;
                        break;
                    }
                }
            }
        }
        
        if (!used) {
            return port;
        }
        port++;
    }
    return 0;
}

void DeviceManage::removeDevice(const QString &serial)
{
    QMutexLocker locker(&m_portMutex);
    if (!serial.isEmpty() && m_devices.contains(serial)) {
        if (m_devices[serial]) {
            m_devices[serial]->deleteLater();
        }
        m_devices.remove(serial);
        m_allocatedPorts.remove(serial);  // 释放端口
    }
}

}
