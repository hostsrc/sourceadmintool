#include "mainwindow.h"
#include "util.h"
#include <QFile>

#include <QMutex>
#include <maxminddb.h>

ServerInfo::ServerInfo(QString server, QueryState state, bool isIP)
{
    this->appId = -1;
    this->rconPassword = "";
    this->saveRcon = false;
    this->rcon = nullptr;
    this->vac = 0;
    this->version = "";
    this->os = "";
    this->tags = "";
    this->haveInfo = false;
    this->goldsrc = false;
    this->queryState = state;

    QStringList address = server.split(":");
    if (address.count() == 1)
    {
        address.append("27015");
    }

    if(isIP)
    {
        this->host = QHostAddress(address.at(0));
        this->GetCountryFlag();
    }
    else
    {
        this->hostname = address.at(0);
    }
    this->hostPort = server;
    this->port = address.at(1).toInt();
}

bool ServerInfo::isEqual(ServerInfo *other) const
{
    return (this->host == other->host && this->port == other->port);
}

void ServerInfo::cleanHashTable()
{
    QList<QString> keys = this->logHashTable.keys();
    for(int i = 0; i < keys.length(); i++)
    {
        PlayerLogInfo info = this->logHashTable.value(keys.at(i));
        if(info.time+(1000*60*30) < QDateTime::currentMSecsSinceEpoch())//remove the key if its older than 30 minutes
        {
            this->logHashTable.remove(keys.at(i));
        }
    }
}

void ServerInfo::GetCountryFlag()
{
    static QMutex mmdbMutex;
    static MMDB_s mmdb;
    static bool mmdbOpen = false;
    static bool mmdbFailed = false;

    if(this->host.toString().isEmpty())
        return;

    QMutexLocker locker(&mmdbMutex);

    if(mmdbFailed)
        return;

    if(!mmdbOpen)
    {
        QByteArray path = BuildPath("GeoLite2-Country.mmdb").toUtf8();
        int status = MMDB_open(path.data(), MMDB_MODE_MMAP, &mmdb);
        if (status != MMDB_SUCCESS)
        {
            qDebug() << "Failed to open MaxMind db (" << MMDB_strerror(status) << ")";
            mmdbFailed = true;
            return;
        }
        mmdbOpen = true;
    }

    int gai_error, mmdb_error;
    MMDB_lookup_result_s results = MMDB_lookup_string(&mmdb, this->host.toString().toLatin1().data(), &gai_error, &mmdb_error);
    if (gai_error == 0 && mmdb_error == MMDB_SUCCESS && results.found_entry)
    {
        MMDB_entry_data_s entry_data;
        int res = MMDB_get_value(&results.entry, &entry_data, "country", "iso_code", NULL);
        if (res == MMDB_SUCCESS && entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING)
        {
            QString countryName = QString(QByteArray::fromRawData(entry_data.utf8_string, entry_data.data_size)).toLower();
            QString flagPath = QString(":/icons/icons/countries/%1.png").arg(countryName);
            if (QFile::exists(flagPath))
            {
                countryFlag.load(flagPath);
            }
            else
            {
                qDebug() << "Flag icon does not exist at " << flagPath << ".";
            }
        }
        else
        {
            qDebug() << "Bad entry. MMDBerror " << MMDB_strerror(res) << ", HasData: " << entry_data.has_data << ", DataType: " << entry_data.utf8_string;
        }
    }
    else
    {
        qDebug() << "Lookup failure. gai: " << gai_error << ", MMDBerror " << MMDB_strerror(mmdb_error) << " (" << mmdb_error << ")";
    }
}

void HostQueryResult::HostInfoResolved(QHostInfo hostInfo)
{
    for(const QHostAddress &addr : hostInfo.addresses())
    {
        if(!addr.isNull() && addr.protocol() == QAbstractSocket::IPv4Protocol)
        {
            this->info->host = addr;
            this->info->queryState = QueryRunning;
            this->mainWindow->CreateTableItemOrUpdate(id->row(), kBrowserColHostname, id->tableWidget(), this->info);

            this->info->GetCountryFlag();

            InfoQuery *infoQuery = new InfoQuery(this->mainWindow);
            infoQuery->query(& this->info->host,  this->info->port, this->id);
            this->deleteLater();
            return;
        }
    }
    info->queryState = QueryResolveFailed;
    this->mainWindow->CreateTableItemOrUpdate(id->row(), kBrowserColHostname, id->tableWidget(), this->info);
    this->deleteLater();
}
