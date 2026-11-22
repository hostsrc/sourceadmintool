#include "updatechecker.h"
#include "version.h"
#include "util.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QDesktopServices>
#include <QPushButton>
#include <QSettings>
#include <QDateTime>
#include <QUrl>
#include <QRegularExpression>

static constexpr int CHECK_INTERVAL_SECS = 86400; // 24 hours

UpdateChecker::UpdateChecker(QWidget *parent)
    : QObject(parent)
    , m_parentWidget(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_updateUrl(DEFAULT_UPDATE_URL)
    , m_forceCheck(false)
{
    connect(m_manager, &QNetworkAccessManager::finished, this, &UpdateChecker::onReplyFinished);
}

void UpdateChecker::setUpdateUrl(const QString &url)
{
    m_updateUrl = url.isEmpty() ? DEFAULT_UPDATE_URL : url;
}

QString UpdateChecker::updateUrl() const
{
    return m_updateUrl;
}

void UpdateChecker::checkForUpdates(bool force)
{
    m_forceCheck = force;

    if(!force)
    {
        QSettings settings(BuildUserPath("settings.ini"), QSettings::IniFormat);
        qint64 lastCheck = settings.value("updater/lastCheckTime", 0).toLongLong();
        qint64 now = QDateTime::currentSecsSinceEpoch();
        if(now - lastCheck < CHECK_INTERVAL_SECS)
            return;
    }

    QNetworkRequest request{QUrl(m_updateUrl)};
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", "SourceAdminTool/" APP_VERSION_STRING);
    m_manager->get(request);
}

void UpdateChecker::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if(reply->error() != QNetworkReply::NoError)
    {
        if(m_forceCheck)
        {
            QMessageBox::warning(m_parentWidget, "Update Check Failed",
                QString("Could not check for updates:\n%1").arg(reply->errorString()));
        }
        return;
    }

    // Update last check time
    QSettings settings(BuildUserPath("settings.ini"), QSettings::IniFormat);
    settings.setValue("updater/lastCheckTime", QDateTime::currentSecsSinceEpoch());

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());

    // Support both single object (/releases/latest) and array (/releases?per_page=1)
    QJsonObject release;
    if(doc.isArray())
    {
        QJsonArray arr = doc.array();
        if(arr.isEmpty())
        {
            if(m_forceCheck)
                QMessageBox::information(m_parentWidget, "No Updates", "No releases found.");
            return;
        }
        release = arr[0].toObject();
    }
    else if(doc.isObject())
    {
        release = doc.object();
    }
    else
    {
        return;
    }
    QString tagName = release["tag_name"].toString();
    QString releaseName = release["name"].toString();
    QString htmlUrl = release["html_url"].toString();
    QString body = release["body"].toString();
    QString targetCommit = release["target_commitish"].toString();

    // Extract version: try tag first (e.g. "v1.2.0"), then release name (e.g. "Dev Build v1.2.0 (abc1234)")
    QString remoteVersion = tagName;
    if(remoteVersion.startsWith('v') || remoteVersion.startsWith('V'))
        remoteVersion = remoteVersion.mid(1);

    // If tag isn't a valid semver (e.g. "dev"), extract version from release name
    QRegularExpression versionRx("(\\d+\\.\\d+\\.\\d+)");
    if(!versionRx.match(remoteVersion).hasMatch())
    {
        QRegularExpressionMatch m = versionRx.match(releaseName);
        if(m.hasMatch())
            remoteVersion = m.captured(1);
    }

    bool isNewer = isNewerVersion(remoteVersion, APP_VERSION_STRING);

    // For same version (dev builds), compare commit hashes
    bool isDifferentCommit = false;
    if(!isNewer && remoteVersion == APP_VERSION_STRING &&
       QRegularExpression("^[0-9a-fA-F]{40}$").match(targetCommit).hasMatch())
    {
        QString localCommit = APP_COMMIT_HASH_FULL;
        // Only compare commits when the build has a real full Git SHA.
        if(QRegularExpression("^[0-9a-fA-F]{40}$").match(localCommit).hasMatch() &&
           targetCommit.compare(localCommit, Qt::CaseInsensitive) != 0)
            isDifferentCommit = true;
    }

    if(!isNewer && !isDifferentCommit)
    {
        if(m_forceCheck)
        {
            QMessageBox::information(m_parentWidget, "No Updates",
                QString("You are running the latest version (%1 - %2).").arg(APP_VERSION_STRING, APP_COMMIT_HASH));
        }
        return;
    }

    // Build display string for the update
    QString remoteDisplay = remoteVersion;
    if(isDifferentCommit && !targetCommit.isEmpty())
        remoteDisplay = QString("%1 (%2)").arg(remoteVersion, targetCommit.left(7));

    QString localDisplay = QString("%1 (%2)").arg(APP_VERSION_STRING, APP_COMMIT_HASH);

    // Check if user previously skipped this version/commit
    if(!m_forceCheck)
    {
        QString skipped = settings.value("updater/skippedVersion").toString();
        QString skipKey = isDifferentCommit ? targetCommit.left(7) : remoteVersion;
        if(skipped == skipKey)
            return;
    }

    // Show update dialog
    QMessageBox msgBox(m_parentWidget);
    msgBox.setWindowTitle("Update Available");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(QString("A new version <b>%1</b> is available! You are running %2.")
        .arg(remoteDisplay, localDisplay));

    if(!body.isEmpty())
    {
        // Truncate long release notes
        QString notes = body.length() > 500 ? body.left(500) + "..." : body;
        msgBox.setInformativeText(notes);
    }

    QPushButton *downloadBtn = msgBox.addButton("Download", QMessageBox::AcceptRole);
    QPushButton *skipBtn = msgBox.addButton("Skip Version", QMessageBox::DestructiveRole);
    msgBox.addButton("Later", QMessageBox::RejectRole);

    msgBox.exec();

    if(msgBox.clickedButton() == downloadBtn)
    {
        QDesktopServices::openUrl(QUrl(htmlUrl));
    }
    else if(msgBox.clickedButton() == skipBtn)
    {
        QString skipKey = isDifferentCommit ? targetCommit.left(7) : remoteVersion;
        settings.setValue("updater/skippedVersion", skipKey);
    }
}

bool UpdateChecker::isNewerVersion(const QString &remote, const QString &local) const
{
    QStringList remoteParts = remote.split('.');
    QStringList localParts = local.split('.');

    for(int i = 0; i < qMax(remoteParts.size(), localParts.size()); i++)
    {
        int r = i < remoteParts.size() ? remoteParts[i].toInt() : 0;
        int l = i < localParts.size() ? localParts[i].toInt() : 0;
        if(r > l) return true;
        if(r < l) return false;
    }
    return false;
}
