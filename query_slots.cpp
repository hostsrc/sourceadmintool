#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "customitems.h"
#include "worker.h"
#include "query.h"
#include "serverinfo.h"
#include "settings.h"

#include <QPainter>
#include <QPushButton>
#include <QHBoxLayout>
#include <QRegularExpression>

#include "database.h"
#include "playerhistorydialog.h"
#include "maphistorydialog.h"
#include "latencydialog.h"
#include "pingcheck.h"

extern QMap<int, QString> appIDMap;
extern QList<ServerInfo *> serverList;

QColor errorColor(255, 60, 60);
QColor queryingColor(80, 170, 80);

int g_queryInterval = QUERY_INTERVAL_DEFAULT;

//TIMER TRIGGERED UPDATING
void MainWindow::TimedUpdate()
{
    static int run = 1;

    if(run % g_queryInterval == 0)
    {
        for(int i = 0; i < this->ui->browserTable->rowCount(); i++)
        {
            ServerTableIndexItem *id = this->GetServerTableIndexItem(i);
            ServerInfo *info = id->GetServerInfo();

            if(info->queryState == QueryResolveFailed || info->queryState == QueryResolving || info->queryState == QueryRunning)
                continue;

            InfoQuery *infoQuery = new InfoQuery(this);

            info->cleanHashTable();

            infoQuery->query(&info->host, info->port, id);
        }
        if(run % 60 == 0)
        {
            this->UpdateSelectedItemInfo(false, true);
            run = 1;
        }
        else
        {
            this->UpdateSelectedItemInfo(false, false);
            run++;
        }
    }
    else
    {
        for(int i = 0; i < this->ui->playerTable->rowCount(); i++)
        {
            PlayerTimeTableItem *item = (PlayerTimeTableItem *)this->ui->playerTable->item(i, 3);

            item->updateTime(item->getTime()+1.0);
        }
        run++;
    }
}

void MainWindow::UpdateSelectedItemInfo(bool removeFirst, bool updateRules)
{
    if(this->ui->browserTable->selectedItems().size() == 0)
        return;

    ServerTableIndexItem *item = this->GetServerTableIndexItem(this->ui->browserTable->currentRow());
    ServerInfo *info = item->GetServerInfo();

    if(removeFirst)
    {
        while(this->ui->playerTable->rowCount() > 0)
        {
            this->ui->playerTable->removeRow(0);
        }
        while(this->ui->infoTable->rowCount() > 0)
        {
            this->ui->infoTable->removeRow(0);
        }
        if(updateRules)
        {
            while(this->ui->rulesTable->rowCount() > 0)
            {
                this->ui->rulesTable->removeRow(0);
            }
        }
    }

    pPlayerQuery = new PlayerQuery(this);
    pPlayerQuery->query(&info->host, info->port, item);

    if(updateRules)
    {
        pRulesQuery = new RulesQuery(this);
        pRulesQuery->query(&info->host, info->port, item);
    }
    this->UpdateInfoTable(info);
}

//Update
void MainWindow::CreateTableItemOrUpdate(size_t row, size_t col, QTableWidget *table, ServerInfo *info)
{
    if(table == this->ui->browserTable)
    {
         bool bAddItem = (table->item(row, col) == nullptr);
        //Handle player stuff differently
        if(col == kBrowserColPlayerCount)
        {
            PlayerTableItem *playerItem = bAddItem ? new PlayerTableItem() : (PlayerTableItem *)table->item(row, col);
            playerItem->players = info->currentPlayers;

            if(info->currentPlayers == info->maxPlayers)
                playerItem->setForeground(errorColor);
            else
                playerItem->setForeground(queryingColor);

            playerItem->setText(info->playerCount);

            if(bAddItem)
            {
                table->setItem(row, col, playerItem);
            }

            return;
        }

        QTableWidgetItem *item = bAddItem ? new QTableWidgetItem() : table->item(row, col);

        if(bAddItem)
        {
            table->setItem(row, col, item);
        }

        switch(col)
        {
            case kBrowserColModIcon:
            {
                QImage icon;
                icon.load(appIDMap.value(info->appId, ":/icons/icons/hl2.gif"));
                item->setData(Qt::DecorationRole, icon);
                break;
            }
            case kBrowserColVACIcon:
                if(bAddItem)
                {
                    item->setData(Qt::DecorationRole, this->GetVACImage());
                }
                break;
            case kBrowserColLockIcon:
                if(bAddItem)
                {
                    item->setData(Qt::DecorationRole, this->GetLockImage());
                }
                break;
            case kBrowserColFlagIcon:
                if(bAddItem)
                {
                    item->setData(Qt::DecorationRole, info->countryFlag);
                }
                break;
            case kBrowserColHostname:
                if(info->queryState == QuerySuccess)
                {
                    item->setText("");
                    auto *hostnameLabel = new QLabel();
                    hostnameLabel->setTextFormat(Qt::RichText);
                    if(!info->alias.isEmpty())
                        hostnameLabel->setText(QString("<b>%1</b> <span style='color:gray;'>(%2)</span>").arg(info->alias.toHtmlEscaped(), info->serverNameRich));
                    else
                        hostnameLabel->setText(info->serverNameRich);
                    table->setCellWidget(row, col, hostnameLabel);
                }
                else if(info->queryState == QueryFailed)
                {
                    item->setForeground(errorColor);
                    item->setText(QString("Failed to query %1, retrying in %2 seconds").arg(info->hostPort, QString::number(g_queryInterval)));
                }
                else if(info->queryState == QueryRunning)
                {
                    item->setForeground(queryingColor);
                    item->setText(QString("Querying server %1...").arg(info->hostPort));
                }
                else if(info->queryState == QueryResolving)
                {
                    item->setForeground(queryingColor);
                    item->setText(QString("Resolving hostname %1...").arg(info->hostPort));
                }
                else if(info->queryState == QueryResolveFailed)
                {
                    item->setForeground(errorColor);
                    item->setText(QString("Failed to resolve %1...").arg(info->hostPort));
                }
                else if(info->queryState == QueryOffline)
                {
                    item->setForeground(errorColor);
                    item->setText(QString("Host unreachable: %1 (network/host down)").arg(info->hostPort));
                }
                else if(info->queryState == QueryUnreachable)
                {
                    item->setForeground(QColor(255, 165, 0)); // orange
                    item->setText(QString("Server offline (host up): %1").arg(info->hostPort));
                }
                if(bAddItem)
                {
                    item->setToolTip(info->hostPort);
                }
                break;
            case kBrowserColMap:
                item->setText(info->currentMap);
                item->setForeground(errorColor);
                break;
            case kBrowserColPing:
            {
                //If timedout show 2000 else show the actual avg.
                quint16 avgPing = (info->lastPing == 2000) ? 2000 : info->avgPing;

                item->setText(QString("%1, Ø%2").arg(QString::number(info->lastPing), QString::number(avgPing)));
                if(info->lastPing > 200)
                    item->setForeground(errorColor);
                else
                    item->setForeground(queryingColor);
                break;
            }
        }
    }
}

//Cleanup?
void MainWindow::UpdateInfoTable(ServerInfo *info, bool current, QList<RulesInfo> *list)
{
    QString mapString;
    QString gameString;

    if(list)
    {
        info->nextMap.clear();
        info->ff.clear();
        info->timelimit.clear();
        info->mods.clear();

        for(int i = 0; i < list->size(); i++)
        {
            this->ui->rulesTable->insertRow(i);
            this->ui->rulesTable->setItem(i, 0, new QTableWidgetItem(list->at(i).name));
            this->ui->rulesTable->setItem(i, 1, new QTableWidgetItem(list->at(i).value));

            if(list->at(i).name == "mp_friendlyfire")
                info->ff = list->at(i).value.toInt() ? "On":"Off";
            if(list->at(i).name == "mp_timelimit")
                info->timelimit = list->at(i).value;
            if(list->at(i).name == "sm_nextmap")
                info->nextMap = list->at(i).value;
            if(list->at(i).name == "sourcemod_version")
                info->mods.append(QString("SourceMod v%1").arg(list->at(i).value));
            if(list->at(i).name == "metamod_version")
                info->mods.prepend(QString("MetaMod v%1").arg(list->at(i).value));
        }
    }

    //Add info list items, gg
    while(this->ui->infoTable->rowCount() > 0 && current)
    {
        this->ui->infoTable->removeRow(0);
    }

    if(current)
    {
        if(!info->currentMap.isEmpty() && !info->nextMap.isEmpty())
        {
            mapString = QString("%1 (Nextmap : %2)").arg(info->currentMap, info->nextMap);
        }
        else if(!info->currentMap.isEmpty())
        {
            mapString = info->currentMap;
        }

        if(!info->gameName.isEmpty() && info->appId != -1)
        {
            gameString = QString("%1 (%2)").arg(info->gameName, QString::number(info->appId));
        }
        else if(!info->gameName.isEmpty())
        {
            gameString = info->gameName;
        }

        QList<InfoTableItem> items;
        items.append(InfoTableItem("Server IP", info->hostPort));
        items.append(InfoTableItem("PingGraph", ""));//Not used but place holder
        if(!info->alias.isEmpty())
            items.append(InfoTableItem("Alias", info->alias));
        items.append(InfoTableItem("Server Name", info->serverNameRich, true));
        if(!info->group.isEmpty())
            items.append(InfoTableItem("Group", info->group));
        items.append(InfoTableItem("Game", gameString));
        items.append(InfoTableItem("Players", info->playerCount));
        items.append(InfoTableItem("PlayerGraph", ""));//Placeholder for player count graph
        items.append(InfoTableItem("Map", mapString));
        items.append(InfoTableItem("MapHistory", ""));//Placeholder for map history button
        items.append(InfoTableItem("Timelimit", info->timelimit));
        if(!info->version.isEmpty())
        {
            items.append(InfoTableItem("Version", QString("v%1 (%2, %3, Protocol %4)").arg(info->version, info->os == "l" ? "Linux" : info->os == "m" ? "Mac" : "Windows", info->type == "d" ? "Dedicated" : "Local", QString::number(info->protocol))));
        }
        if(info->haveInfo)
        {
            QString engineStr = info->goldsrc ? "GoldSrc" : "Source";
            items.append(InfoTableItem("Engine", QString("%1 (%2, Protocol %3)").arg(engineStr, info->goldsrc ? "0x6D" : "0x49", QString::number(info->protocol))));
        }

        QString modString;

        if(info->mods.length() > 0)
        {
            modString = info->mods.join("  ");
        }

        items.append(InfoTableItem("Addons", modString));
        items.append(InfoTableItem("AntiCheat", info->vac ? "VAC" : ""));
        items.append(InfoTableItem("Steam ID", info->serverID));
        if(!info->notes.isEmpty())
            items.append(InfoTableItem("Notes", info->notes));

        quint16 row = 0;
        InfoTableItem item;

        for(int i = 0; i < items.length(); i++)
        {
            item = items.at(i);

            if(item.display == "PingGraph")//Ping graph
            {
                this->ui->infoTable->insertRow(row);
                this->ui->infoTable->setSpan(row, 0, 1, 2);
                this->ui->infoTable->setRowHeight(row, 50);
                auto *pingGraph = new GraphWidget(&info->pingList, GraphWidget::PingGraph, 300, this);
                this->ui->infoTable->setCellWidget(row, 0, pingGraph);
                row++;

                // Latency stats link
                this->ui->infoTable->insertRow(row);
                this->ui->infoTable->setItem(row, 0, new QTableWidgetItem("Latency"));
                auto *latLink = new QLabel("<a href='#' style='color: #4a9eff;'>Latency & Traceroute</a>", this);
                latLink->setCursor(Qt::PointingHandCursor);
                QString sKey = info->hostPort;
                QString sName = info->serverNameRich;
                sName.remove(QRegularExpression("<[^>]*>"));
                QString sHost = info->hostPort;
                connect(latLink, &QLabel::linkActivated, this, [this, sKey, sName, sHost]{
                    LatencyDialog dlg(sKey, sName, sHost, this);
                    dlg.exec();
                });
                this->ui->infoTable->setCellWidget(row, 1, latLink);
                row++;
            }
            else if(item.display == "PlayerGraph" && info->playerCountHistory.length() > 1)
            {
                this->ui->infoTable->insertRow(row);
                this->ui->infoTable->setSpan(row, 0, 1, 2);
                this->ui->infoTable->setRowHeight(row, 50);
                int maxP = info->maxPlayers > 0 ? info->maxPlayers : 32;
                auto *playerGraph = new GraphWidget(&info->playerCountHistory, GraphWidget::PlayerGraph, maxP, this);
                this->ui->infoTable->setCellWidget(row, 0, playerGraph);
                row++;

                // Player history link
                this->ui->infoTable->insertRow(row);
                this->ui->infoTable->setItem(row, 0, new QTableWidgetItem("History"));
                auto *histLink = new QLabel("<a href='#' style='color: #4a9eff;'>Player History (24h/7D/30D)</a>", this);
                histLink->setCursor(Qt::PointingHandCursor);
                QString sKey = info->hostPort;
                QString sName = info->serverNameRich;
                sName.remove(QRegularExpression("<[^>]*>"));
                int sMaxP = maxP;
                connect(histLink, &QLabel::linkActivated, this, [this, sKey, sName, sMaxP]{
                    PlayerHistoryDialog dlg(sKey, sName, sMaxP, this);
                    dlg.exec();
                });
                this->ui->infoTable->setCellWidget(row, 1, histLink);
                row++;
            }
            else if(item.display == "MapHistory")
            {
                this->ui->infoTable->insertRow(row);
                this->ui->infoTable->setItem(row, 0, new QTableWidgetItem("Maps"));
                auto *mapLink = new QLabel("<a href='#' style='color: #4a9eff;'>Map History</a>", this);
                mapLink->setCursor(Qt::PointingHandCursor);
                QString sKey = info->hostPort;
                QString sName = info->serverNameRich;
                sName.remove(QRegularExpression("<[^>]*>"));
                connect(mapLink, &QLabel::linkActivated, this, [this, sKey, sName]{
                    MapHistoryDialog dlg(sKey, sName, this);
                    dlg.exec();
                });
                this->ui->infoTable->setCellWidget(row, 1, mapLink);
                row++;
            }
            else if(!item.val.isEmpty() && item.display != "PlayerGraph")
            {
                this->ui->infoTable->insertRow(row);
                this->ui->infoTable->setItem(row, 0, new QTableWidgetItem(item.display));
                if (item.richValue)
                {
                    auto label = new QLabel(item.val);
                    label->setTextFormat(Qt::RichText);
                    this->ui->infoTable->setCellWidget(row, 1, label);
                }
                else
                {
                    this->ui->infoTable->setItem(row, 1, new QTableWidgetItem(item.val));
                }
                row++;
            }
        }
        items.clear();
    }
}

//QUERY INFO READY
void MainWindow::ServerInfoReady(InfoReply *reply, ServerTableIndexItem *indexCell)
{
    QTableWidget *browserTable = this->ui->browserTable;

    int row = -1;

    //Make sure our item still exists.
    for(int i = 0; i < this->ui->browserTable->rowCount(); i++)
    {
        if(indexCell == this->GetServerTableIndexItem(i))
        {
            row = i;
            break;
        }
    }

    if(row == -1)
    {
        if(reply)
            delete reply;
        return;
    }

    ServerInfo *info = indexCell->GetServerInfo();

    if(reply)
    {
        info->lastPing = reply->ping;

        while(info->pingList.length() >= 1000)
        {
            info->pingList.removeFirst();
        }

        info->pingList.append(TimestampedValue(reply->ping));

        // Record latency to database
        Database::instance()->insertLatencySample(info->hostPort,
            QDateTime::currentSecsSinceEpoch(), reply->ping);

        quint64 totalPing = 0;
        quint16 totalPings = 0;

        for(int i = 0; i < info->pingList.length(); i++)
        {
            if(info->pingList.at(i).value == 2000)//only count completed pings
                continue;

            totalPing += info->pingList.at(i).value;
            totalPings++;
        }

        if(totalPings)
        {
            info->avgPing = totalPing/totalPings;
        }

        this->CreateTableItemOrUpdate(row, kBrowserColPing, browserTable, info);
    }

    if(reply && reply->success)
    {
        bool appIdChanged = info->appId != reply->appId;

        info->vac = reply->vac;
        info->goldsrc = reply->goldsrc;
        info->appId = reply->appId;
        info->os = reply->os;
        info->tags = reply->tags;
        info->rawServerId = reply->rawServerId;
        info->protocol = reply->protocol;
        info->version = reply->version;
        info->currentMap = reply->map;
        info->gameName = reply->gamedesc;
        info->type = reply->type;
        info->serverNameRich = reply->hostnameRich;
        info->playerCount = QString("%1 (%3)/%2").arg(QString::number(reply->players), QString::number(reply->maxplayers), QString::number(reply->bots));
        info->haveInfo = true;
        info->serverID = reply->serverID;
        info->queryState = QuerySuccess;
        info->maxPlayers = reply->maxplayers;
        info->currentPlayers = reply->players;

        // Track player count history
        while(info->playerCountHistory.length() >= 1000)
            info->playerCountHistory.removeFirst();
        info->playerCountHistory.append(TimestampedValue(reply->players));

        // Record player count to database
        Database::instance()->insertPlayerCount(info->hostPort,
            QDateTime::currentSecsSinceEpoch(), reply->players, reply->maxplayers);

        // Detect map changes
        if(!info->previousMap.isEmpty() && info->previousMap != reply->map)
        {
            Database::instance()->insertMapChange(info->hostPort,
                QDateTime::currentSecsSinceEpoch(), reply->map);
        }
        else if(info->previousMap.isEmpty() && !reply->map.isEmpty())
        {
            // First time seeing this server, record initial map
            Database::instance()->insertMapChange(info->hostPort,
                QDateTime::currentSecsSinceEpoch(), reply->map);
        }
        info->previousMap = reply->map;

        if (!info->countryFlag.isNull())
        {
            this->CreateTableItemOrUpdate(row, kBrowserColFlagIcon, browserTable, info);
        }

        if(appIdChanged)
        {
            this->CreateTableItemOrUpdate(row, kBrowserColModIcon, browserTable, info);
        }

        if(reply->vac)
        {
            this->CreateTableItemOrUpdate(row, kBrowserColVACIcon, browserTable, info);
        }
        else if(!reply->vac && browserTable->item(row, kBrowserColVACIcon))
        {
            browserTable->removeCellWidget(row, kBrowserColVACIcon);
        }

        if(reply->visibility)
        {
            this->CreateTableItemOrUpdate(row, kBrowserColLockIcon, browserTable, info);
        }
        else if(!reply->visibility && browserTable->item(row, kBrowserColLockIcon))
        {
            browserTable->removeCellWidget(row, kBrowserColLockIcon);
        }

        this->CreateTableItemOrUpdate(row, kBrowserColHostname, browserTable, info);
        this->CreateTableItemOrUpdate(row, kBrowserColMap, browserTable, info);
        this->CreateTableItemOrUpdate(row, kBrowserColPlayerCount, browserTable, info);

        delete reply;
    }
    else
    {
        info->queryState = QueryFailed;
        this->CreateTableItemOrUpdate(row, kBrowserColHostname, browserTable, info);

        // Trigger ICMP ping check to distinguish offline vs unreachable
        PingCheck *pingCheck = new PingCheck(this);
        connect(pingCheck, &PingCheck::finished, this, &MainWindow::HostReachabilityReady);
        pingCheck->check(info->hostPort, indexCell);

        if(reply)
            delete reply;
    }
    this->UpdateInfoTable(info, (row == this->ui->browserTable->currentRow()));
    this->ApplyBrowserFilter();
    this->UpdateStatusBar();
}

void MainWindow::HostReachabilityReady(bool reachable, ServerTableIndexItem *indexCell)
{
    // Find the row for this item
    int row = -1;
    for(int i = 0; i < this->ui->browserTable->rowCount(); i++)
    {
        if(indexCell == this->GetServerTableIndexItem(i))
        {
            row = i;
            break;
        }
    }

    if(row == -1)
        return;

    ServerInfo *info = indexCell->GetServerInfo();

    // Only update if still in a failed state (not already re-queried successfully)
    if(info->queryState != QueryFailed && info->queryState != QueryOffline && info->queryState != QueryUnreachable)
        return;

    if(reachable)
        info->queryState = QueryUnreachable; // Host alive, server process down
    else
        info->queryState = QueryOffline; // Host unreachable

    this->CreateTableItemOrUpdate(row, kBrowserColHostname, this->ui->browserTable, info);
    this->UpdateInfoTable(info, (row == this->ui->browserTable->currentRow()));
    this->ApplyBrowserFilter();
    this->UpdateStatusBar();
}

void MainWindow::PlayerInfoReady(QList<PlayerInfo> *list, ServerTableIndexItem *indexCell)
{
    if(this->ui->browserTable->selectedItems().empty() || this->ui->browserTable->selectedItems().at(0) != indexCell)
    {
        if(list)
        {
            delete list;
        }

        return;
    }

    ServerInfo *info = indexCell->GetServerInfo();
    this->ui->playerTable->setRowCount(0);

    this->ui->playerTable->setSortingEnabled(false);

    for(int i = 0; i < list->size(); i++)
    {
        QTableWidgetItem *itemScore = new QTableWidgetItem();
        QTableWidgetItem *id = new QTableWidgetItem();

        id->setData(Qt::DisplayRole, i+1);
        itemScore->setData(Qt::DisplayRole, list->at(i).score);

        this->ui->playerTable->insertRow(i);
        this->ui->playerTable->setItem(i, 0, id);
        this->ui->playerTable->setItem(i, 1, new QTableWidgetItem(list->at(i).name));
        this->ui->playerTable->setItem(i, 2, itemScore);

        PlayerTimeTableItem *time = new PlayerTimeTableItem();
        time->updateTime(list->at(i).time);

        this->ui->playerTable->setItem(i, 3, time);

        QString steamID = "";

        if(info->logHashTable.contains(list->at(i).name))
        {
            steamID = (info->logHashTable.value(list->at(i).name).steamID);
        }

        this->ui->playerTable->setItem(i, 4, new QTableWidgetItem(steamID));
    }

    if(list)
    {
        delete list;
    }

    this->ui->playerTable->setSortingEnabled(true);
}

void MainWindow::RulesInfoReady(QList<RulesInfo> *list, ServerTableIndexItem *indexCell)
{
    if(this->ui->browserTable->selectedItems().empty() || this->ui->browserTable->selectedItems().at(0) != indexCell)
    {
        if(list)
        {
            delete list;
        }

        return;
    }
    ServerInfo *info = indexCell->GetServerInfo();

    if(info->haveInfo)
    {
        list->append(RulesInfo("vac", QString::number(info->vac)));
        list->append(RulesInfo("version", info->version));
        list->append(RulesInfo("appID", QString::number(info->appId)));
        list->append(RulesInfo("os", info->os));

        if(info->rawServerId != 0)
        {
           list->append(RulesInfo("steamID64", QString::number(info->rawServerId)));
        }
    }

    this->ui->rulesTable->setRowCount(0);
    this->ui->rulesTable->setSortingEnabled(false);

    this->UpdateInfoTable(info, true, list);

    if(list)
    {
        delete list;
    }

    this->ui->rulesTable->setSortingEnabled(true);
}
