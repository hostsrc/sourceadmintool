#include "pingcheck.h"
#include "serverinfo.h"

PingCheck::PingCheck(QObject *parent)
    : QObject(parent), process(nullptr), indexItem(nullptr)
{
}

void PingCheck::check(const QString &hostAddress, ServerTableIndexItem *item)
{
    indexItem = item;

    // Extract IP (remove port)
    QString ip = hostAddress;
    if(ip.contains(":"))
        ip = ip.split(":").first();

    process = new QProcess(this);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PingCheck::onProcessFinished);

#ifdef Q_OS_WIN
    process->start("ping", {"-n", "1", "-w", "2000", ip});
#else
    process->start("ping", {"-c", "1", "-W", "2", ip});
#endif
}

void PingCheck::onProcessFinished(int exitCode, QProcess::ExitStatus)
{
    bool reachable = (exitCode == 0);
    emit finished(reachable, indexItem);
    this->deleteLater();
}
