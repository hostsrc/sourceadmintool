#include "database.h"
#include "util.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

Database *Database::s_instance = nullptr;

Database::Database()
{
}

Database *Database::instance()
{
    if(!s_instance)
        s_instance = new Database();
    return s_instance;
}

bool Database::open()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(BuildUserPath("sat_analytics.db"));

    if(!db.open())
    {
        qWarning() << "Database: failed to open" << db.lastError().text();
        return false;
    }

    createTables();
    purgeOldData();
    return true;
}

void Database::close()
{
    if(db.isOpen())
        db.close();
}

void Database::createTables()
{
    QSqlQuery q(db);

    q.exec("CREATE TABLE IF NOT EXISTS player_counts ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "server_key TEXT NOT NULL,"
           "timestamp INTEGER NOT NULL,"
           "player_count INTEGER NOT NULL,"
           "max_players INTEGER NOT NULL)");

    q.exec("CREATE INDEX IF NOT EXISTS idx_pc_server_time ON player_counts(server_key, timestamp)");

    q.exec("CREATE TABLE IF NOT EXISTS map_changes ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "server_key TEXT NOT NULL,"
           "timestamp INTEGER NOT NULL,"
           "map_name TEXT NOT NULL)");

    q.exec("CREATE INDEX IF NOT EXISTS idx_mc_server_time ON map_changes(server_key, timestamp)");

    q.exec("CREATE TABLE IF NOT EXISTS latency_samples ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "server_key TEXT NOT NULL,"
           "timestamp INTEGER NOT NULL,"
           "ping_ms INTEGER NOT NULL)");

    q.exec("CREATE INDEX IF NOT EXISTS idx_ls_server_time ON latency_samples(server_key, timestamp)");
}

void Database::purgeOldData()
{
    // Purge data older than 30 days
    qint64 cutoff = QDateTime::currentSecsSinceEpoch() - (30 * 24 * 3600);

    QSqlQuery q(db);
    q.prepare("DELETE FROM player_counts WHERE timestamp < ?");
    q.addBindValue(cutoff);
    q.exec();

    q.prepare("DELETE FROM map_changes WHERE timestamp < ?");
    q.addBindValue(cutoff);
    q.exec();

    q.prepare("DELETE FROM latency_samples WHERE timestamp < ?");
    q.addBindValue(cutoff);
    q.exec();
}

// --- Player Counts ---

void Database::insertPlayerCount(const QString &serverKey, qint64 timestamp, int players, int maxPlayers)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO player_counts (server_key, timestamp, player_count, max_players) VALUES (?, ?, ?, ?)");
    q.addBindValue(serverKey);
    q.addBindValue(timestamp);
    q.addBindValue(players);
    q.addBindValue(maxPlayers);
    q.exec();
}

QList<PlayerCountRecord> Database::getPlayerCounts(const QString &serverKey, qint64 startTime, qint64 endTime)
{
    QList<PlayerCountRecord> results;
    QSqlQuery q(db);
    q.prepare("SELECT timestamp, player_count, max_players FROM player_counts "
              "WHERE server_key = ? AND timestamp >= ? AND timestamp <= ? "
              "ORDER BY timestamp ASC");
    q.addBindValue(serverKey);
    q.addBindValue(startTime);
    q.addBindValue(endTime);
    q.exec();

    while(q.next())
    {
        PlayerCountRecord r;
        r.timestamp = q.value(0).toLongLong();
        r.playerCount = q.value(1).toInt();
        r.maxPlayers = q.value(2).toInt();
        results.append(r);
    }
    return results;
}

// --- Map Changes ---

void Database::insertMapChange(const QString &serverKey, qint64 timestamp, const QString &mapName)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO map_changes (server_key, timestamp, map_name) VALUES (?, ?, ?)");
    q.addBindValue(serverKey);
    q.addBindValue(timestamp);
    q.addBindValue(mapName);
    q.exec();
}

QList<MapChangeRecord> Database::getMapHistory(const QString &serverKey, int limit)
{
    QList<MapChangeRecord> results;
    QSqlQuery q(db);
    q.prepare("SELECT timestamp, map_name FROM map_changes "
              "WHERE server_key = ? ORDER BY timestamp DESC LIMIT ?");
    q.addBindValue(serverKey);
    q.addBindValue(limit);
    q.exec();

    while(q.next())
    {
        MapChangeRecord r;
        r.timestamp = q.value(0).toLongLong();
        r.mapName = q.value(1).toString();
        results.append(r);
    }
    return results;
}

// --- Latency ---

void Database::insertLatencySample(const QString &serverKey, qint64 timestamp, int pingMs)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO latency_samples (server_key, timestamp, ping_ms) VALUES (?, ?, ?)");
    q.addBindValue(serverKey);
    q.addBindValue(timestamp);
    q.addBindValue(pingMs);
    q.exec();
}

QList<LatencyRecord> Database::getLatencySamples(const QString &serverKey, qint64 startTime, qint64 endTime)
{
    QList<LatencyRecord> results;
    QSqlQuery q(db);
    q.prepare("SELECT timestamp, ping_ms FROM latency_samples "
              "WHERE server_key = ? AND timestamp >= ? AND timestamp <= ? "
              "ORDER BY timestamp ASC");
    q.addBindValue(serverKey);
    q.addBindValue(startTime);
    q.addBindValue(endTime);
    q.exec();

    while(q.next())
    {
        LatencyRecord r;
        r.timestamp = q.value(0).toLongLong();
        r.pingMs = q.value(1).toInt();
        results.append(r);
    }
    return results;
}
