#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QList>
#include <QString>
#include "serverinfo.h"

struct PlayerCountRecord
{
    qint64 timestamp;
    int playerCount;
    int maxPlayers;
};

struct MapChangeRecord
{
    qint64 timestamp;
    QString mapName;
};

struct LatencyRecord
{
    qint64 timestamp;
    int pingMs;
};

class Database
{
public:
    static Database *instance();
    bool open();
    void close();

    // Player counts
    void insertPlayerCount(const QString &serverKey, qint64 timestamp, int players, int maxPlayers);
    QList<PlayerCountRecord> getPlayerCounts(const QString &serverKey, qint64 startTime, qint64 endTime);

    // Map changes
    void insertMapChange(const QString &serverKey, qint64 timestamp, const QString &mapName);
    QList<MapChangeRecord> getMapHistory(const QString &serverKey, int limit = 100);

    // Latency
    void insertLatencySample(const QString &serverKey, qint64 timestamp, int pingMs);
    QList<LatencyRecord> getLatencySamples(const QString &serverKey, qint64 startTime, qint64 endTime);

private:
    Database();
    void createTables();
    void purgeOldData();

    QSqlDatabase db;
    static Database *s_instance;
};

#endif // DATABASE_H
