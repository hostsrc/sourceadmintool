#ifndef PINGCHECK_H
#define PINGCHECK_H

#include <QObject>
#include <QProcess>
#include "customitems.h"

class ServerInfo;

class PingCheck : public QObject
{
    Q_OBJECT
public:
    PingCheck(QObject *parent = nullptr);
    void check(const QString &hostAddress, ServerTableIndexItem *item);

signals:
    void finished(bool hostReachable, ServerTableIndexItem *item);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess *process;
    ServerTableIndexItem *indexItem;
};

#endif // PINGCHECK_H
