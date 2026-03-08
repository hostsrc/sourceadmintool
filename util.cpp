#include "util.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

QString BuildPath(const char *file)
{
#ifdef WIN32
    return QString("./%1").arg(file);
#elif defined(__APPLE__)
    // Data files live in Contents/Resources/ inside the app bundle
    return QString("%1/../Resources/%2").arg(QCoreApplication::applicationDirPath(), file);
#else
    return QString("%1/%2").arg(QCoreApplication::applicationDirPath(), file);
#endif
}

QString BuildUserPath(const char *file)
{
#ifdef WIN32
    return QString("./%1").arg(file);
#else
    // Store user data in a proper config directory so settings survive app updates.
    // macOS: ~/Library/Application Support/SourceAdminTool/
    // Linux: ~/.local/share/SourceAdminTool/
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(configDir);
    if(!dir.exists())
        dir.mkpath(".");

    // If the file exists next to the binary (old location), migrate it
    QString oldPath = QString("%1/%2").arg(QCoreApplication::applicationDirPath(), file);
    QString newPath = QString("%1/%2").arg(configDir, file);

    if(QFile::exists(oldPath) && !QFile::exists(newPath))
        QFile::copy(oldPath, newPath);

    return newPath;
#endif
}
