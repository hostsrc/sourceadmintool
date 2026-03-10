#ifndef SERVERINFO
#define SERVERINFO

#include <QMainWindow>
#include <QHostAddress>
#include <QHash>
#include <QDateTime>
#include <QHostInfo>
#include "rcon.h"
#include "customitems.h"

struct TimestampedValue
{
    qint64 timestamp;
    int value;
    TimestampedValue() : timestamp(0), value(0) {}
    TimestampedValue(int val) : timestamp(QDateTime::currentMSecsSinceEpoch()), value(val) {}
};

class MainWindow;

enum QueryState
{
    QueryRunning,
    QueryFailed,
    QuerySuccess,
    QueryResolving,
    QueryResolveFailed,
    QueryOffline,       // Host unreachable (ICMP ping fails)
    QueryUnreachable,   // Host alive but game server not responding
};

class PlayerLogInfo
{
public:
    PlayerLogInfo()
    {
        time = QDateTime::currentMSecsSinceEpoch();
        steamID = "";
        userid = 0;
    }

    PlayerLogInfo(quint32 id, QString steam)
    {
        time = QDateTime::currentMSecsSinceEpoch();
        steamID = steam;
        userid = id;
    }

    qint64 time;
    QString steamID;
    quint32 userid;
};



class ServerInfo
{
public:
    ~ServerInfo()
    {
        if(this->rcon != nullptr)
            delete this->rcon;
    }

    ServerInfo(QString, QueryState, bool);
    bool isEqual(ServerInfo *)const;
    void cleanHashTable();
    void GetCountryFlag();
public:
    bool haveInfo;
    bool goldsrc;
    qint8 protocol;
    QString tags;
    quint8 vac;
    QString version;
    QString os;
    qint32 appId;
    QString serverNameRich;
    QString currentMap;
    QString previousMap;
    QString nextMap;
    QString ff;
    QString timelimit;
    QString hostname;
    QStringList mods;
    QString playerCount;
    QString gameName;
    QString type;
    QString serverID;
    QHostAddress host;
    quint16 port;
    quint64 rawServerId;
    //This name sucks... this is IP:PORT or HOSTNAME:PORT
    QString hostPort;
    QString rconPassword;
    QStringList rconOutput;
    QStringList logOutput;
    QStringList chatOutput;
    bool saveRcon;
    RconQuery *rcon;
    QHash<QString, PlayerLogInfo> logHashTable;
    QList<TimestampedValue> pingList;
    quint16 lastPing;
    quint16 avgPing;
    QImage countryFlag;
    QueryState queryState;
    quint8 currentPlayers;
    quint8 maxPlayers;
    QString group;
    QString alias;
    QString notes;
    QList<TimestampedValue> playerCountHistory;
};

class HostQueryResult : public QObject
{
    Q_OBJECT
public slots:
    void HostInfoResolved(QHostInfo);
public:
    HostQueryResult(ServerInfo *p, MainWindow *main, ServerTableIndexItem *item)
    {
        info = p;
        id = item;
        mainWindow = main;
    }

private:
    MainWindow *mainWindow;
    ServerTableIndexItem *id;
    ServerInfo *info;

};

#endif // SERVERINFO
