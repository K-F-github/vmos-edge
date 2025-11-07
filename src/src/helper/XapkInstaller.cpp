#include "XapkInstaller.h"
#include "../../QtScrcpyCore/src/adb/adbprocessimpl.h"
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QProcess>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <archive.h>
#include <archive_entry.h>

XapkInstaller::XapkInstaller(QObject *parent)
    : QObject(parent)
    , m_workerThread(nullptr)
{
}

XapkInstaller::~XapkInstaller()
{
    if (m_workerThread) {
        if (m_workerThread->isRunning()) {
            m_workerThread->quit();
            m_workerThread->wait(5000);
        }
        m_workerThread->deleteLater();
    }
}

void XapkInstaller::installXapk(const QString& xapkFile, QPointer<qsc::IDevice> device,
                                const QString& adbPath, const QString& adbDeviceAddress)
{
    if (!device) {
        qWarning() << "XapkInstaller::installXapk - device is null";
        emit finished(false, "设备对象为空");
        return;
    }

    QFileInfo fileInfo(xapkFile);
    if (!fileInfo.exists()) {
        qWarning() << "XapkInstaller::installXapk - file does not exist:" << xapkFile;
        emit finished(false, "XAPK文件不存在");
        return;
    }

    if (!xapkFile.toLower().endsWith(".xapk")) {
        qWarning() << "XapkInstaller::installXapk - file is not a XAPK file:" << xapkFile;
        emit finished(false, "不是有效的XAPK文件");
        return;
    }

    emit progress("开始解压 XAPK 文件...");

    // 创建临时目录用于解压 XAPK
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir baseDir(tempDir);
    QString extractDir = baseDir.absoluteFilePath("xapk_extract_" + QString::number(QDateTime::currentMSecsSinceEpoch()));

    if (!QDir().mkpath(extractDir)) {
        qWarning() << "XapkInstaller::installXapk - failed to create temp directory:" << extractDir;
        emit finished(false, "无法创建临时目录");
        return;
    }

    qDebug() << "XapkInstaller: Extracting XAPK file:" << xapkFile << "to:" << extractDir;

    QString finalAdbPath = adbPath.isEmpty() ? AdbProcessImpl::getAdbPath() : adbPath;
    // adbDeviceAddress 是用于ADB命令的设备地址（例如 "192.168.10.49:5555" 或 "EDGEAQFZ7DCJSQJ0"）
    // 如果为空，则使用设备的serial
    QString finalAdbDeviceAddress = adbDeviceAddress.isEmpty() ? device->getSerial() : adbDeviceAddress;

    if (finalAdbPath.isEmpty() || finalAdbDeviceAddress.isEmpty()) {
        qWarning() << "XapkInstaller::installXapk - ADB path or device address is empty";
        QDir(extractDir).removeRecursively();
        emit finished(false, "ADB 路径或设备地址为空");
        return;
    }

    // 在后台线程执行阻塞操作
    if (m_workerThread) {
        if (m_workerThread->isRunning()) {
            qWarning() << "XapkInstaller: Previous installation is still running";
            emit finished(false, "上一个安装任务仍在进行中");
            return;
        }
        m_workerThread->deleteLater();
    }

    m_workerThread = new QThread(this);

    // 使用 lambda 在后台线程中执行所有阻塞操作
    connect(m_workerThread, &QThread::started, [=]() {
        doInstallInBackground(xapkFile, extractDir, finalAdbPath, finalAdbDeviceAddress, device);
    });

    // 线程结束时自动清理
    connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater);

    // 启动后台线程
    m_workerThread->start();
}

void XapkInstaller::doInstallInBackground(const QString& xapkFile, const QString& extractDir,
                                          const QString& adbPath, const QString& adbDeviceAddress,
                                          QPointer<qsc::IDevice> device)
{
    // 第一步：解压 XAPK 文件
    qDebug() << "XapkInstaller background thread: Extracting XAPK file";
    if (!extractZip(xapkFile, extractDir)) {
        qWarning() << "XapkInstaller background thread: Failed to extract XAPK file";
        QMetaObject::invokeMethod(this, "onBackgroundInstallCompleted",
                                Qt::QueuedConnection,
                                Q_ARG(bool, false),
                                Q_ARG(QString, "解压 XAPK 文件失败"),
                                Q_ARG(QString, extractDir),
                                Q_ARG(QStringList, QStringList()),
                                Q_ARG(QString, QString()),
                                Q_ARG(QPointer<qsc::IDevice>, device));
        return;
    }

    QMetaObject::invokeMethod(this, "progress",
                            Qt::QueuedConnection,
                            Q_ARG(QString, "解压完成，正在查找 APK 文件..."));

    // 第二步：查找解压后的文件
    QDir extractDirObj(extractDir);
    QStringList entries = extractDirObj.entryList(QDir::Files | QDir::NoDotAndDotDot);

    QStringList allApkFiles;
    QString mainApkFile;
    QStringList obbFiles;
    qint64 maxApkSize = 0;

    for (const QString& entry : entries) {
        QString filePath = extractDirObj.absoluteFilePath(entry);
        QFileInfo entryInfo(filePath);

        if (entry.toLower().endsWith(".apk")) {
            allApkFiles.append(filePath);
            qint64 fileSize = entryInfo.size();
            if (fileSize > maxApkSize) {
                maxApkSize = fileSize;
                mainApkFile = filePath;
            }
        } else if (entry.toLower().endsWith(".obb")) {
            obbFiles.append(filePath);
        }
    }

    if (allApkFiles.isEmpty()) {
        qWarning() << "XapkInstaller background thread: No APK file found in XAPK";
        QMetaObject::invokeMethod(this, "onBackgroundInstallCompleted",
                                Qt::QueuedConnection,
                                Q_ARG(bool, false),
                                Q_ARG(QString, "未找到 APK 文件"),
                                Q_ARG(QString, extractDir),
                                Q_ARG(QStringList, QStringList()),
                                Q_ARG(QString, QString()),
                                Q_ARG(QPointer<qsc::IDevice>, device));
        return;
    }

    qDebug() << "XapkInstaller background thread: Found APK files:" << allApkFiles.size();
    qDebug() << "XapkInstaller background thread: Found main APK:" << mainApkFile;
    qDebug() << "XapkInstaller background thread: Found OBB files:" << obbFiles;

    QMetaObject::invokeMethod(this, "progress",
                            Qt::QueuedConnection,
                            Q_ARG(QString, "正在提取包名..."));

    // 第三步：提取包名
    QString packageName = extractPackageNameFromApk(mainApkFile);
    if (packageName.isEmpty()) {
        qDebug() << "XapkInstaller background thread: Could not extract package name from APK";
    } else {
        qDebug() << "XapkInstaller background thread: Extracted package name:" << packageName;
    }

    // 第四步：安装 APK（阻塞操作）
    if (allApkFiles.size() > 1) {
        // 多个 APK，使用 install-multiple
        QMetaObject::invokeMethod(this, "progress",
                                Qt::QueuedConnection,
                                Q_ARG(QString, QString("正在安装 %1 个 APK 文件...").arg(allApkFiles.size())));

        // 重新排序 APK 文件
        QStringList sortedApkFiles;
        if (!mainApkFile.isEmpty()) {
            sortedApkFiles.append(mainApkFile);
        }
        for (const QString& apkFile : allApkFiles) {
            if (apkFile != mainApkFile) {
                sortedApkFiles.append(apkFile);
            }
        }

        // 构建命令
        QStringList adbArgs;
        adbArgs << "-s" << adbDeviceAddress;
        adbArgs << "install-multiple";
        adbArgs << "-r";
        for (const QString& apkFile : sortedApkFiles) {
            adbArgs << apkFile;
        }

        qDebug() << "XapkInstaller background thread: Executing install-multiple";

        // 执行安装（阻塞操作，但已在后台线程）
        QProcess installProcess;
        installProcess.setProgram(adbPath);
        installProcess.setArguments(adbArgs);
        installProcess.start();

        if (!installProcess.waitForFinished(60000)) {
            qWarning() << "XapkInstaller background thread: install-multiple timeout";
            QMetaObject::invokeMethod(this, "onBackgroundInstallCompleted",
                                    Qt::QueuedConnection,
                                    Q_ARG(bool, false),
                                    Q_ARG(QString, "安装超时（60秒）"),
                                    Q_ARG(QString, extractDir),
                                    Q_ARG(QStringList, QStringList()),
                                    Q_ARG(QString, QString()),
                                    Q_ARG(QPointer<qsc::IDevice>, device));
            return;
        }

        if (installProcess.exitCode() != 0) {
            QString errorOutput = installProcess.readAllStandardError();
            qWarning() << "XapkInstaller background thread: install-multiple failed:"
                       << "Exit code:" << installProcess.exitCode()
                       << "Error:" << errorOutput;
            QMetaObject::invokeMethod(this, "onBackgroundInstallCompleted",
                                    Qt::QueuedConnection,
                                    Q_ARG(bool, false),
                                    Q_ARG(QString, QString("安装失败: %1").arg(errorOutput)),
                                    Q_ARG(QString, extractDir),
                                    Q_ARG(QStringList, QStringList()),
                                    Q_ARG(QString, QString()),
                                    Q_ARG(QPointer<qsc::IDevice>, device));
            return;
        }

        qDebug() << "XapkInstaller background thread: Install-multiple completed successfully";
        QString output = installProcess.readAllStandardOutput();
        if (!output.isEmpty()) {
            qDebug() << "XapkInstaller background thread: Install output:" << output;
        }

        // 安装成功，通知主线程
        QMetaObject::invokeMethod(this, "onBackgroundInstallCompleted",
                                Qt::QueuedConnection,
                                Q_ARG(bool, true),
                                Q_ARG(QString, "XAPK 安装成功"),
                                Q_ARG(QString, extractDir),
                                Q_ARG(QStringList, obbFiles),
                                Q_ARG(QString, packageName),
                                Q_ARG(QPointer<qsc::IDevice>, device));
    } else {
        // 单个 APK，使用异步安装（不阻塞）
        QMetaObject::invokeMethod(this, "progress",
                                Qt::QueuedConnection,
                                Q_ARG(QString, "正在安装 APK 文件..."));

        // 单个APK安装需要回到主线程执行（通过信号槽机制）
        // 注意：这里不能直接调用device->installApkRequest，因为需要在主线程中执行
        // 我们通过信号通知主线程来执行安装
        QString singleApkFile = allApkFiles.first();
        QMetaObject::invokeMethod(this, [this, device, singleApkFile]() {
            if (device) {
                device->installApkRequest(singleApkFile);
            }
        }, Qt::QueuedConnection);

        // 单个 APK 安装是异步的
        QMetaObject::invokeMethod(this, "onBackgroundInstallCompleted",
                                Qt::QueuedConnection,
                                Q_ARG(bool, true),
                                Q_ARG(QString, "APK 安装已启动"),
                                Q_ARG(QString, extractDir),
                                Q_ARG(QStringList, obbFiles),
                                Q_ARG(QString, packageName),
                                Q_ARG(QPointer<qsc::IDevice>, device));
    }
}

void XapkInstaller::onBackgroundInstallCompleted(bool success, const QString& message,
                                                  const QString& extractDir,
                                                  const QStringList& obbFiles,
                                                  const QString& packageName,
                                                  QPointer<qsc::IDevice> device)
{
    // 这个方法在主线程中执行，可以安全访问 Qt 对象

    if (success) {
        emit progress("APK 安装完成");

        // 处理 OBB 文件
        if (!obbFiles.isEmpty()) {
            if (!packageName.isEmpty()) {
                emit progress("正在推送 OBB 文件...");
                pushObbFiles(obbFiles, packageName, device);
                emit progress("OBB 文件推送已启动");
            } else {
                qWarning() << "XapkInstaller: Could not determine package name for OBB files";
            }
        }

        // 清理临时目录
        QTimer::singleShot(10000, [extractDir]() {
            QDir(extractDir).removeRecursively();
        });
    } else {
        // 安装失败，延迟清理临时目录
        QTimer::singleShot(30000, [extractDir]() {
            QDir(extractDir).removeRecursively();
        });
    }

    emit finished(success, message);
}

bool XapkInstaller::extractZip(const QString& zipPath, const QString& outputDir)
{
    struct archive *a;
    struct archive *ext;
    struct archive_entry *entry;
    int flags;
    int r;

    // 设置解压标志
    flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS;

    // 创建读取和解压上下文
    a = archive_read_new();
    archive_read_support_format_zip(a);  // 只支持 ZIP 格式
    archive_read_support_filter_all(a);

    ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);

#ifdef Q_OS_WIN
    // Windows上：设置工作目录，让libarchive使用相对路径
    QString originalCurrentDir = QDir::currentPath();
    QDir::setCurrent(outputDir);
#endif

    // 打开 ZIP 文件
    QFile zipFile(zipPath);
    if (!zipFile.open(QIODevice::ReadOnly)) {
        qDebug() << "XapkInstaller: Failed to open ZIP file:" << zipPath << "Error:" << zipFile.errorString();
        archive_read_free(a);
        archive_write_free(ext);
#ifdef Q_OS_WIN
        QDir::setCurrent(originalCurrentDir);
#endif
        return false;
    }

#ifdef Q_OS_WIN
    // Windows上：读取整个文件到内存，然后使用内存方式打开archive
    QByteArray fileData = zipFile.readAll();
    zipFile.close();

    if (fileData.isEmpty()) {
        qDebug() << "XapkInstaller: Failed to read ZIP file or file is empty:" << zipPath;
        archive_read_free(a);
        archive_write_free(ext);
        QDir::setCurrent(originalCurrentDir);
        return false;
    }

    r = archive_read_open_memory(a, fileData.data(), fileData.size());
    if (r != ARCHIVE_OK) {
        qDebug() << "XapkInstaller: Failed to open archive from memory:" << archive_error_string(a);
        archive_read_free(a);
        archive_write_free(ext);
        QDir::setCurrent(originalCurrentDir);
        return false;
    }
#else
    // Unix系统上直接使用文件描述符
    int fd = zipFile.handle();
    if (fd == -1) {
        qDebug() << "XapkInstaller: Failed to get file descriptor for:" << zipPath;
        zipFile.close();
        archive_read_free(a);
        archive_write_free(ext);
        return false;
    }

    r = archive_read_open_fd(a, fd, 10240);
    if (r != ARCHIVE_OK) {
        qDebug() << "XapkInstaller: Failed to open ZIP file via fd:" << archive_error_string(a);
        zipFile.close();
        archive_read_free(a);
        archive_write_free(ext);
        return false;
    }
#endif

    // 解压所有文件
    while (true) {
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF) {
            break;
        }
        if (r != ARCHIVE_OK) {
            qDebug() << "XapkInstaller: Failed to read header:" << archive_error_string(a);
            break;
        }

        // 设置输出路径
        QString entryPath = QString::fromUtf8(archive_entry_pathname(entry));

#ifdef Q_OS_WIN
        // Windows上：使用相对路径（相对于outputDir）
        QFileInfo entryInfo(entryPath);
        QString parentDir = entryInfo.path();
        if (!parentDir.isEmpty() && parentDir != ".") {
            QDir outputDirObj(outputDir);
            QString fullParentDir = outputDirObj.absoluteFilePath(parentDir);
            if (!QDir().mkpath(fullParentDir)) {
                qDebug() << "XapkInstaller: Failed to create parent directory:" << fullParentDir;
            }
        }

        QByteArray entryPathUtf8 = entryPath.toUtf8();
        archive_entry_set_pathname(entry, entryPathUtf8.constData());
#else
        // Unix系统上：使用完整路径
        QDir outputDirObj(outputDir);
        QString fullPath = outputDirObj.absoluteFilePath(entryPath);
        QByteArray fullPathUtf8 = fullPath.toUtf8();
        archive_entry_set_pathname(entry, fullPathUtf8.constData());
#endif

        // 写入文件
        r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK) {
            qDebug() << "XapkInstaller: Failed to write header:" << archive_error_string(ext) << "Entry path:" << entryPath;
            break;
        }

        // 复制文件内容
        if (archive_entry_size(entry) > 0) {
            const void *buff;
            size_t size;
            la_int64_t offset;

            for (;;) {
                r = archive_read_data_block(a, &buff, &size, &offset);
                if (r == ARCHIVE_EOF) {
                    break;
                }
                if (r != ARCHIVE_OK) {
                    qDebug() << "XapkInstaller: Failed to read data:" << archive_error_string(a);
                    break;
                }
                r = archive_write_data_block(ext, buff, size, offset);
                if (r != ARCHIVE_OK) {
                    qDebug() << "XapkInstaller: Failed to write data:" << archive_error_string(ext);
                    break;
                }
            }
        }

        r = archive_write_finish_entry(ext);
        if (r != ARCHIVE_OK) {
            qDebug() << "XapkInstaller: Failed to finish entry:" << archive_error_string(ext);
            break;
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

#ifdef Q_OS_WIN
    // 恢复原始工作目录
    QDir::setCurrent(originalCurrentDir);
#else
    zipFile.close();
#endif

    return (r == ARCHIVE_OK || r == ARCHIVE_EOF);
}

QString XapkInstaller::extractPackageNameFromApk(const QString& apkFile)
{
    // 获取应用程序目录路径
    QString appDir = QCoreApplication::applicationDirPath();

    // 辅助函数：尝试使用指定的 aapt 路径提取包名
    auto tryExtractPackageName = [&apkFile](const QString& aaptPath) -> QString {
        QFileInfo aaptInfo(aaptPath);
        // 如果是相对路径（系统 PATH），直接尝试；如果是绝对路径，检查文件是否存在
        if (aaptPath.contains("/") || aaptPath.contains("\\")) {
            // 绝对路径，检查文件是否存在
            if (!aaptInfo.exists() || !aaptInfo.isFile()) {
                return QString(); // 文件不存在
            }
        }

        QProcess aaptProcess;
        aaptProcess.setProgram(aaptPath);
        aaptProcess.setArguments(QStringList() << "dump" << "badging" << apkFile);
        aaptProcess.start();

        if (aaptProcess.waitForFinished(5000) && aaptProcess.exitCode() == 0) {
            QByteArray output = aaptProcess.readAllStandardOutput();
            QString outputStr = QString::fromUtf8(output);

            // 解析 aapt 输出，查找 package: name='...'
            QRegularExpression regex(R"(package:\s*name='([^']+)')");
            QRegularExpressionMatch match = regex.match(outputStr);
            if (match.hasMatch()) {
                QString packageName = match.captured(1);
                qDebug() << "XapkInstaller: Extracted package name using" << aaptPath << "package:" << packageName;
                return packageName;
            }
        }
        return QString();
    };

    // 方法1: 优先尝试应用程序目录下的 aapt
#ifdef Q_OS_WIN
    QString localAapt = appDir + "/aapt.exe";
    QString localAapt2 = appDir + "/aapt2.exe";
#else
    QString localAapt = appDir + "/aapt";
    QString localAapt2 = appDir + "/aapt2";
#endif

    QString packageName = tryExtractPackageName(localAapt);
    if (!packageName.isEmpty()) {
        return packageName;
    }

    // 方法2: 尝试应用程序目录下的 aapt2
    packageName = tryExtractPackageName(localAapt2);
    if (!packageName.isEmpty()) {
        return packageName;
    }

    // 方法3: 回退到系统 PATH 中的 aapt
    packageName = tryExtractPackageName("aapt");
    if (!packageName.isEmpty()) {
        return packageName;
    }

    // 方法4: 回退到系统 PATH 中的 aapt2
    packageName = tryExtractPackageName("aapt2");
    if (!packageName.isEmpty()) {
        return packageName;
    }

    qDebug() << "XapkInstaller: Could not extract package name using aapt/aapt2. "
             << "Checked application directory:" << appDir
             << "and system PATH.";
    return QString();
}

void XapkInstaller::pushObbFiles(const QStringList& obbFiles, const QString& packageName,
                                  QPointer<qsc::IDevice> device)
{
    if (packageName.isEmpty() || obbFiles.isEmpty() || !device) {
        return;
    }

    qDebug() << "XapkInstaller: Pushing OBB files for package:" << packageName;

    // OBB 文件需要推送到 /sdcard/Android/obb/<package_name>/
    QString obbBasePath = QString("/sdcard/Android/obb/%1").arg(packageName);

    for (const QString& obbFile : obbFiles) {
        QFileInfo obbInfo(obbFile);
        QString obbFileName = obbInfo.fileName();
        QString deviceObbPath = QString("%1/%2").arg(obbBasePath, obbFileName);

        qDebug() << "XapkInstaller: Pushing OBB file:" << obbFile << "to" << deviceObbPath;
        device->pushFileRequest(obbFile, deviceObbPath);
    }

    qDebug() << "XapkInstaller: OBB files pushed successfully";
}

