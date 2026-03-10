#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QNetworkAccessManager>

#define DEFAULT_UPDATE_URL "https://api.github.com/repos/hostsrc/sourceadmintool/releases?per_page=1"

class QWidget;

class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit UpdateChecker(QWidget *parent = nullptr);
    void checkForUpdates(bool force = false);
    void setUpdateUrl(const QString &url);
    QString updateUrl() const;

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    bool isNewerVersion(const QString &remote, const QString &local) const;

    QWidget *m_parentWidget;
    QNetworkAccessManager *m_manager;
    QString m_updateUrl;
    bool m_forceCheck;
};

#endif // UPDATECHECKER_H
