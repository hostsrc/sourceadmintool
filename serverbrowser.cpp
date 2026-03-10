#include "serverbrowser.h"
#include "mainwindow.h"
#include "serverinfo.h"
#include <QApplication>
#include <QClipboard>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMenu>
#include <QMessageBox>
#include <QUrl>
#include <QUrlQuery>

extern QList<ServerInfo *> serverList;

const QList<GameEntry> ServerBrowser::s_games = {
    {"Counter-Strike 2", 730},
    {"Counter-Strike: GO", 4465480},
    {"Team Fortress 2", 440},
    {"Left 4 Dead 2", 550},
    {"Counter-Strike: Source", 240},
    {"Garry's Mod", 4000},
    {"Rust", 252490},
    {"ARK: Survival Evolved", 346110},
    {"Day of Defeat: Source", 300},
    {"Half-Life 2: Deathmatch", 320},
    {"Insurgency", 222880},
    {"No More Room in Hell", 224260},
    {"Black Mesa", 362890},
    {"Counter-Strike 1.6", 10},
    {"Half-Life Deathmatch", 70},
    {"Day of Defeat", 30},
    {"Team Fortress Classic", 20},
};

enum BrowserCol
{
    kColHostname,
    kColMap,
    kColPlayers,
    kColPing,
    kColVAC,
    kColTags,
    kColAddress,

    kColCount
};

ServerBrowser::ServerBrowser(MainWindow *main, QWidget *parent)
    : QWidget(parent)
    , m_main(main)
    , m_manager(new QNetworkAccessManager(this))
    , m_checkManager(new QNetworkAccessManager(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    // Top bar: game selector, map filter, search button
    QHBoxLayout *topBar = new QHBoxLayout;

    QLabel *gameLabel = new QLabel("Game:", this);
    m_gameCombo = new QComboBox(this);
    m_gameCombo->setMinimumWidth(200);
    populateGames();

    QLabel *nameLabel = new QLabel("Name:", this);
    m_nameFilter = new QLineEdit(this);
    m_nameFilter->setPlaceholderText("Server name");
    m_nameFilter->setMaximumWidth(200);

    QLabel *mapLabel = new QLabel("Map:", this);
    m_mapFilter = new QLineEdit(this);
    m_mapFilter->setPlaceholderText("e.g. de_dust2");
    m_mapFilter->setMaximumWidth(200);

    QLabel *ipLabel = new QLabel("IP:", this);
    m_ipFilter = new QLineEdit(this);
    m_ipFilter->setPlaceholderText("e.g. 192.168.1.1");
    m_ipFilter->setMaximumWidth(160);

    m_searchBtn = new QPushButton("Search", this);
    m_checkListingBtn = new QPushButton("Check Listing", this);
    m_checkListingBtn->setToolTip("Check if your saved servers are listed on the Steam Master Server");

    m_statusLabel = new QLabel(this);

    topBar->addWidget(gameLabel);
    topBar->addWidget(m_gameCombo);
    topBar->addWidget(nameLabel);
    topBar->addWidget(m_nameFilter);
    topBar->addWidget(mapLabel);
    topBar->addWidget(m_mapFilter);
    topBar->addWidget(ipLabel);
    topBar->addWidget(m_ipFilter);
    topBar->addWidget(m_searchBtn);
    topBar->addWidget(m_checkListingBtn);
    topBar->addWidget(m_statusLabel, 1);

    layout->addLayout(topBar);

    // Local filter bar to search within results
    QHBoxLayout *filterBar = new QHBoxLayout;
    QLabel *filterLabel = new QLabel("Filter results:", this);
    m_localFilter = new QLineEdit(this);
    m_localFilter->setPlaceholderText("Type to filter by name, map, or address...");
    m_localFilter->setClearButtonEnabled(true);
    filterBar->addWidget(filterLabel);
    filterBar->addWidget(m_localFilter, 1);
    layout->addLayout(filterBar);

    // Results table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(kColCount);
    m_table->setHorizontalHeaderLabels({"Server", "Map", "Players", "Ping", "VAC", "Tags", "Address"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(20);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);

    auto header = m_table->horizontalHeader();
    header->setSectionResizeMode(kColHostname, QHeaderView::Stretch);
    header->setSectionResizeMode(kColMap, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(kColPlayers, QHeaderView::Fixed);
    header->setSectionResizeMode(kColPing, QHeaderView::Fixed);
    header->setSectionResizeMode(kColVAC, QHeaderView::Fixed);
    header->setSectionResizeMode(kColTags, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(kColAddress, QHeaderView::ResizeToContents);
    m_table->setColumnWidth(kColPlayers, 80);
    m_table->setColumnWidth(kColPing, 50);
    m_table->setColumnWidth(kColVAC, 40);

    layout->addWidget(m_table);

    // Connections
    connect(m_searchBtn, &QPushButton::clicked, this, &ServerBrowser::onSearchClicked);
    connect(m_checkListingBtn, &QPushButton::clicked, this, &ServerBrowser::onCheckListingClicked);
    connect(m_nameFilter, &QLineEdit::returnPressed, this, &ServerBrowser::onSearchClicked);
    connect(m_mapFilter, &QLineEdit::returnPressed, this, &ServerBrowser::onSearchClicked);
    connect(m_ipFilter, &QLineEdit::returnPressed, this, &ServerBrowser::onSearchClicked);
    connect(m_manager, &QNetworkAccessManager::finished, this, &ServerBrowser::onReplyFinished);
    connect(m_checkManager, &QNetworkAccessManager::finished, this, &ServerBrowser::onCheckListingReply);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &ServerBrowser::onTableDoubleClicked);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &ServerBrowser::onContextMenu);
    connect(m_localFilter, &QLineEdit::textChanged, this, &ServerBrowser::onLocalFilterChanged);
}

void ServerBrowser::populateGames()
{
    for(const auto &game : s_games)
    {
        m_gameCombo->addItem(game.name, game.appId);
    }
}

void ServerBrowser::setSearching(bool searching)
{
    m_searchBtn->setEnabled(!searching);
    m_gameCombo->setEnabled(!searching);
    if(searching)
        m_statusLabel->setText("Searching...");
}

void ServerBrowser::onSearchClicked()
{
    if(m_apiKey.isEmpty())
    {
        QMessageBox::warning(this, "Steam API Key Required",
            "A Steam Web API key is required to browse servers.\n\n"
            "Get a free key at: https://steamcommunity.com/dev/apikey\n"
            "Then set it in Settings > Set Steam API Key.");
        return;
    }

    int appId = m_gameCombo->currentData().toInt();
    if(appId <= 0)
        return;

    setSearching(true);

    // Build filter string
    QString filter = QString("\\appid\\%1").arg(appId);

    if(!m_nameFilter->text().isEmpty())
        filter += QString("\\name_match\\*%1*").arg(m_nameFilter->text());

    if(!m_mapFilter->text().isEmpty())
        filter += QString("\\map\\%1").arg(m_mapFilter->text());

    if(!m_ipFilter->text().isEmpty())
        filter += QString("\\gameaddr\\%1").arg(m_ipFilter->text().trimmed());

    // Exclude empty servers by default
    filter += "\\empty\\1";

    QUrl url("https://api.steampowered.com/IGameServersService/GetServerList/v1/");
    QUrlQuery query;
    query.addQueryItem("key", m_apiKey);
    query.addQueryItem("filter", filter);
    query.addQueryItem("limit", "500");
    url.setQuery(query);

    QNetworkRequest request{url};
    request.setRawHeader("User-Agent", "SourceAdminTool");
    m_manager->get(request);
}

void ServerBrowser::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    setSearching(false);

    if(reply->error() != QNetworkReply::NoError)
    {
        m_statusLabel->setText(QString("Error: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();
    QJsonObject response = root["response"].toObject();
    QJsonArray servers = response["servers"].toArray();

    m_results.clear();
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    m_table->setRowCount(servers.size());

    for(int i = 0; i < servers.size(); i++)
    {
        QJsonObject srv = servers[i].toObject();

        BrowserServerEntry entry;
        entry.addr = srv["addr"].toString();
        entry.name = srv["name"].toString();
        entry.map = srv["map"].toString();
        entry.players = srv["players"].toInt();
        entry.maxPlayers = srv["max_players"].toInt();
        entry.bots = srv["bots"].toInt();
        entry.vac = srv["secure"].toBool();
        entry.password = srv["password"].toBool();
        entry.gamedir = srv["gamedir"].toString();
        entry.version = srv["version"].toString();
        entry.tags = srv["gametype"].toString();
        m_results.append(entry);

        m_table->setItem(i, kColHostname, new QTableWidgetItem(entry.name));
        m_table->setItem(i, kColMap, new QTableWidgetItem(entry.map));

        QString playerStr = QString("%1/%2").arg(entry.players).arg(entry.maxPlayers);
        if(entry.bots > 0)
            playerStr = QString("%1(%2)/%3").arg(entry.players).arg(entry.bots).arg(entry.maxPlayers);
        QTableWidgetItem *playerItem = new QTableWidgetItem(playerStr);
        playerItem->setData(Qt::UserRole, entry.players); // For numeric sorting
        m_table->setItem(i, kColPlayers, playerItem);

        // No ping from API, leave empty
        m_table->setItem(i, kColPing, new QTableWidgetItem(""));

        m_table->setItem(i, kColVAC, new QTableWidgetItem(entry.vac ? "Yes" : ""));
        m_table->setItem(i, kColTags, new QTableWidgetItem(entry.tags));
        m_table->setItem(i, kColAddress, new QTableWidgetItem(entry.addr));
    }

    m_table->setSortingEnabled(true);
    m_statusLabel->setText(QString("Found %1 servers").arg(servers.size()));
}

void ServerBrowser::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    QTableWidgetItem *addrItem = m_table->item(row, kColAddress);
    if(!addrItem)
        return;

    QString addr = addrItem->text();
    m_main->AddServerToList(addr);
}

void ServerBrowser::onContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = m_table->itemAt(pos);
    if(!item)
        return;

    int row = item->row();
    QTableWidgetItem *addrItem = m_table->item(row, kColAddress);
    if(!addrItem)
        return;

    QString addr = addrItem->text();

    QMenu menu(this);
    QAction *addAction = menu.addAction("Add to Server List");
    QAction *copyAction = menu.addAction("Copy Address");

    QAction *selected = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if(selected == addAction)
    {
        m_main->AddServerToList(addr);
    }
    else if(selected == copyAction)
    {
        QApplication::clipboard()->setText(addr);
    }
}

void ServerBrowser::onLocalFilterChanged(const QString &text)
{
    QString filter = text.trimmed().toLower();
    for(int i = 0; i < m_table->rowCount(); i++)
    {
        if(filter.isEmpty())
        {
            m_table->setRowHidden(i, false);
            continue;
        }

        bool match = false;
        // Check hostname, map, and address columns
        for(int col : {kColHostname, kColMap, kColAddress, kColTags})
        {
            QTableWidgetItem *item = m_table->item(i, col);
            if(item && item->text().toLower().contains(filter))
            {
                match = true;
                break;
            }
        }
        m_table->setRowHidden(i, !match);
    }
}

void ServerBrowser::onCheckListingClicked()
{
    if(serverList.isEmpty())
    {
        m_statusLabel->setText("No servers in your list to check");
        return;
    }

    // Collect all server addresses
    m_checkAddrs.clear();
    m_listedAddrs.clear();
    m_checkResults.clear();
    m_checkPending = 0;

    for(const auto *info : serverList)
    {
        if(!info->hostPort.isEmpty() && info->queryState == QuerySuccess)
            m_checkAddrs.append(info->hostPort);
    }

    if(m_checkAddrs.isEmpty())
    {
        m_statusLabel->setText("No online servers to check");
        return;
    }

    m_checkTotal = m_checkAddrs.size();

    m_results.clear();
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);

    m_searchBtn->setEnabled(false);
    m_checkListingBtn->setEnabled(false);
    m_statusLabel->setText(QString("Checking %1 servers...").arg(m_checkTotal));

    processNextCheckAddr();
}

void ServerBrowser::processNextCheckAddr()
{
    if(m_checkAddrs.isEmpty() && m_checkPending == 0)
    {
        // All done — populate table
        m_table->setRowCount(0);

        int listed = 0;
        int notListed = 0;

        for(const auto *info : serverList)
        {
            if(info->hostPort.isEmpty())
                continue;

            int row = m_table->rowCount();
            m_table->insertRow(row);

            bool isListed = m_listedAddrs.contains(info->hostPort);
            if(isListed)
                listed++;
            else
                notListed++;

            QString name = info->serverNameRich.isEmpty() ? info->alias : info->serverNameRich;
            // Strip HTML tags from name for display
            QString plainName = name;
            plainName.remove(QRegularExpression("<[^>]*>"));
            if(plainName.isEmpty()) plainName = info->hostPort;

            m_table->setItem(row, kColHostname, new QTableWidgetItem(plainName));

            // Show gamedir and appid from Steam response
            QString gameInfo;
            bool vac = false;
            if(m_checkResults.contains(info->hostPort))
            {
                const auto &r = m_checkResults[info->hostPort];
                gameInfo = r.gamedir;
                if(r.players > 0)
                    gameInfo += QString(" (AppID %1)").arg(r.players); // appid stored in players field
                vac = r.vac;
            }
            m_table->setItem(row, kColMap, new QTableWidgetItem(gameInfo));

            // Players from local data
            QString playerStr;
            if(info->maxPlayers > 0)
                playerStr = QString("%1/%2").arg(info->currentPlayers).arg(info->maxPlayers);
            m_table->setItem(row, kColPlayers, new QTableWidgetItem(playerStr));

            // Status column
            QTableWidgetItem *statusItem = new QTableWidgetItem(isListed ? "Listed" : "Not Listed");
            if(isListed)
                statusItem->setForeground(QColor(0, 180, 0));
            else
                statusItem->setForeground(QColor(255, 60, 60));
            m_table->setItem(row, kColPing, statusItem);

            m_table->setItem(row, kColVAC, new QTableWidgetItem(vac ? "Yes" : ""));
            m_table->setItem(row, kColTags, new QTableWidgetItem(info->group));
            m_table->setItem(row, kColAddress, new QTableWidgetItem(info->hostPort));
        }

        m_table->setSortingEnabled(true);
        m_searchBtn->setEnabled(true);
        m_checkListingBtn->setEnabled(true);
        m_checkResults.clear();
        m_statusLabel->setText(QString("Check complete: %1 listed, %2 not listed (of %3 servers)")
            .arg(listed).arg(notListed).arg(listed + notListed));
        return;
    }

    // Send requests per address using GetServersAtAddress (no API key needed)
    while(!m_checkAddrs.isEmpty() && m_checkPending < 15)
    {
        QString addr = m_checkAddrs.takeFirst();

        QUrl url("https://api.steampowered.com/ISteamApps/GetServersAtAddress/v0001/");
        QUrlQuery query;
        query.addQueryItem("addr", addr);
        query.addQueryItem("format", "json");
        url.setQuery(query);

        QNetworkRequest request{url};
        request.setRawHeader("User-Agent", "SourceAdminTool");
        m_checkManager->get(request);
        m_checkPending++;
    }
}

void ServerBrowser::onCheckListingReply(QNetworkReply *reply)
{
    reply->deleteLater();
    m_checkPending--;

    // Extract the queried address from the request URL
    QUrlQuery replyQuery(reply->request().url().query());
    QString queriedAddr = replyQuery.queryItemValue("addr");

    if(reply->error() == QNetworkReply::NoError)
    {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject response = doc.object()["response"].toObject();

        if(response["success"].toBool())
        {
            QJsonArray servers = response["servers"].toArray();
            if(!servers.isEmpty())
            {
                // Mark the queried address as listed (API returned servers for it)
                m_listedAddrs.insert(queriedAddr);

                // Use first server entry for metadata
                QJsonObject srv = servers[0].toObject();
                BrowserServerEntry entry;
                entry.addr = queriedAddr;
                entry.gamedir = srv["gamedir"].toString();
                entry.players = srv["appid"].toInt(); // store appid in players field for display
                entry.vac = srv["secure"].toBool();
                m_checkResults[queriedAddr] = entry;
            }
        }
    }

    int checked = m_checkTotal - m_checkAddrs.size() - m_checkPending;
    m_statusLabel->setText(QString("Checking... %1/%2 IPs").arg(checked).arg(m_checkTotal));

    // Continue with more requests or finalize
    processNextCheckAddr();
}
