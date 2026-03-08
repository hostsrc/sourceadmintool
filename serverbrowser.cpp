#include "serverbrowser.h"
#include "mainwindow.h"
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

const QList<GameEntry> ServerBrowser::s_games = {
    {"Counter-Strike 2", 730},
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
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    // Top bar: game selector, map filter, search button
    QHBoxLayout *topBar = new QHBoxLayout;

    QLabel *gameLabel = new QLabel("Game:", this);
    m_gameCombo = new QComboBox(this);
    m_gameCombo->setMinimumWidth(200);
    populateGames();

    QLabel *mapLabel = new QLabel("Map:", this);
    m_mapFilter = new QLineEdit(this);
    m_mapFilter->setPlaceholderText("e.g. de_dust2");
    m_mapFilter->setMaximumWidth(200);

    m_searchBtn = new QPushButton("Search", this);

    m_statusLabel = new QLabel(this);

    topBar->addWidget(gameLabel);
    topBar->addWidget(m_gameCombo);
    topBar->addWidget(mapLabel);
    topBar->addWidget(m_mapFilter);
    topBar->addWidget(m_searchBtn);
    topBar->addWidget(m_statusLabel, 1);

    layout->addLayout(topBar);

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
    connect(m_mapFilter, &QLineEdit::returnPressed, this, &ServerBrowser::onSearchClicked);
    connect(m_manager, &QNetworkAccessManager::finished, this, &ServerBrowser::onReplyFinished);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &ServerBrowser::onTableDoubleClicked);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &ServerBrowser::onContextMenu);
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

    if(!m_mapFilter->text().isEmpty())
        filter += QString("\\map\\%1").arg(m_mapFilter->text());

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
