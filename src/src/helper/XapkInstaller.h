#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QPointer>
#include "QtScrcpyCore.h"

/**
 * @brief XAPK安装帮助类
 * 
 * 从XAPK文件中提取APK和OBB文件，安装APK并推送OBB文件到设备
 */
class XapkInstaller : public QObject
{
    Q_OBJECT

public:
    explicit XapkInstaller(QObject *parent = nullptr);
    ~XapkInstaller() override;

    /**
     * @brief 安装XAPK文件
     * @param xapkFile XAPK文件路径
     * @param device 设备对象（用于推送文件）
     * @param adbPath ADB路径
     * @param adbDeviceAddress ADB设备地址（例如 "192.168.10.49:5555" 或设备序列号）
     */
    void installXapk(const QString& xapkFile, QPointer<qsc::IDevice> device, 
                     const QString& adbPath, const QString& adbDeviceAddress);

signals:
    /**
     * @brief 安装进度通知
     * @param message 进度消息
     */
    void progress(const QString& message);

    /**
     * @brief 安装完成通知
     * @param success 是否成功
     * @param message 结果消息
     */
    void finished(bool success, const QString& message);

private:
    /**
     * @brief 解压ZIP文件
     * @param zipPath ZIP文件路径
     * @param outputDir 输出目录
     * @return 是否成功
     */
    static bool extractZip(const QString& zipPath, const QString& outputDir);

    /**
     * @brief 从APK文件中提取包名
     * @param apkFile APK文件路径
     * @return 包名，失败返回空字符串
     */
    static QString extractPackageNameFromApk(const QString& apkFile);

    /**
     * @brief 推送OBB文件到设备
     * @param obbFiles OBB文件列表
     * @param packageName 包名
     * @param device 设备对象
     */
    static void pushObbFiles(const QStringList& obbFiles, const QString& packageName, 
                             QPointer<qsc::IDevice> device);

    /**
     * @brief 在后台线程中执行安装操作
     */
    void doInstallInBackground(const QString& xapkFile, const QString& extractDir,
                               const QString& adbPath, const QString& adbDeviceAddress,
                               QPointer<qsc::IDevice> device);

private slots:
    /**
     * @brief 处理后台安装完成的结果
     */
    void onBackgroundInstallCompleted(bool success, const QString& message,
                                      const QString& extractDir,
                                      const QStringList& obbFiles,
                                      const QString& packageName,
                                      QPointer<qsc::IDevice> device);

private:
    QThread* m_workerThread;
};

