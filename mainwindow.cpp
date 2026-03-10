#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "customitems.h"
#include "settings.h"
#include "updatechecker.h"
#include "serverbrowser.h"
#include "database.h"
#include <QKeyEvent>
#include <QTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QStatusBar>
#include <QRegularExpression>

extern Settings *settings;
extern QList<ServerInfo *> serverList;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    pPlayerQuery = nullptr;
    pRulesQuery = nullptr;
    ui->setupUi(this);
    setWindowTitle("Source Admin Tool");
    commandIter = new QMutableListIterator<QString>(this->commandHistory);
    commandIterDirection = kIterInit;
    sayIter = new QMutableListIterator<QString>(this->sayHistory);
    sayIterDirection = kIterInit;
    this->ui->commandOutput->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    // Create filter bar above the main splitter
    QWidget *filterBar = new QWidget(this);
    QHBoxLayout *filterLayout = new QHBoxLayout(filterBar);
    filterLayout->setContentsMargins(0, 0, 0, 0);

    filterEdit = new QLineEdit(filterBar);
    filterEdit->setPlaceholderText("Filter servers...");
    filterEdit->setClearButtonEnabled(true);

    hideOfflineCheck = new QCheckBox("Hide Offline", filterBar);

    QLabel *groupLabel = new QLabel("Group:", filterBar);
    groupFilterCombo = new QComboBox(filterBar);
    groupFilterCombo->setMinimumWidth(120);
    groupFilterCombo->addItem("All Groups");

    filterLayout->addWidget(filterEdit, 1);
    filterLayout->addWidget(hideOfflineCheck);
    filterLayout->addWidget(groupLabel);
    filterLayout->addWidget(groupFilterCombo);

    // Insert filter bar into the grid layout at row 0, push splitter to row 1
    QGridLayout *grid = qobject_cast<QGridLayout *>(ui->centralWidget->layout());
    grid->addWidget(filterBar, 0, 0);
    grid->addWidget(ui->splitter, 1, 0);

    // Status summary bar
    statusBarLabel = new QLabel(this);
    statusBar()->addWidget(statusBarLabel, 1);

    this->SetRconEnabled(false);
    m_updateChecker = new UpdateChecker(this);
    Database::instance()->open();

    settings = new Settings(this);
    settings->SetDefaultSettings();
    settings->ReadSettings();
    settings->GetCtxCommands();
    pLogHandler = new LogHandler(this);
    pLogHandler->createBind(u16logPort);

    if(this->ui->browserTable->rowCount() > 0)
    {
        this->ui->browserTable->selectRow(0);
        this->browserTableItemSelected();
        this->SetRconEnabled(true);
        this->SetRconSignals(true);
        this->RestoreRcon(0);
        this->SetRconSignals(false);
    }

    this->HookEvents();
    this->ConnectSlots();

    this->updateTimer = new QTimer(this);
    connect(this->updateTimer, &QTimer::timeout, this, &MainWindow::TimedUpdate);
    this->updateTimer->start(1000);

    m_updateChecker->checkForUpdates();

    // Set up server browser in the Browse tab
    m_serverBrowser = new ServerBrowser(this, ui->browseContainer);
    QVBoxLayout *browseLayout = new QVBoxLayout(ui->browseContainer);
    browseLayout->setContentsMargins(0, 0, 0, 0);
    browseLayout->addWidget(m_serverBrowser);

    // Load Steam API key (must be after m_serverBrowser is created)
    settings->LoadSteamApiKey();
}

MainWindow::~MainWindow()
{
    Database::instance()->close();
    delete settings;
    serverList.clear();
    delete ui;
    delete pLogHandler;
    delete sayIter;
    delete commandIter;
}

AddServerError MainWindow::CheckServerList(QString server)
{
    QStringList address = server.split(":");
    bool ok;

    if(address.size() == 1)
    {
        address.append("27015");
    }
    else if (address.size() != 2)
    {
        return AddServerError_Invalid;
    }

    QString ip = address.at(0);
    quint16 port = address.at(1).toInt(&ok);
    QHostAddress addr;
    AddServerError ret = AddServerError_None;

    if(!port || !ok)
    {
        return AddServerError_Invalid;
    }
    else if(!addr.setAddress(ip))
    {
        //Check if it is a hostname
        QUrl url = QUrl::fromUserInput(ip);
        if(url != QUrl())
        {
            ret = AddServerError_Hostname;
        }
        else
        {
            return AddServerError_Invalid;
        }
    }

    for(int i = 0; i < serverList.size(); i++)
    {
        //Check if the ip or hostname already exists.
        if(serverList.at(i)->hostPort == server)
        {
            return AddServerError_AlreadyExists;
        }
    }

    return ret;
}

ServerInfo *MainWindow::AddServerToList(QString server, AddServerError *pError)
{
    AddServerError error = CheckServerList(server);

    if(pError != nullptr)
    {
        *pError = error;
    }

    if(error != AddServerError_None && error != AddServerError_Hostname)
    {
        return nullptr;
    }

    bool isIP = (error == AddServerError_None);
    QueryState state = (error == AddServerError_None) ? QueryRunning : QueryResolving;

    ServerInfo *info = new ServerInfo(server, state, isIP);

    serverList.append(info);

    int row = ui->browserTable->rowCount();
    ui->browserTable->insertRow(row);

    ServerTableIndexItem *id = new ServerTableIndexItem(info);
    id->setData(Qt::DisplayRole, serverList.size());
    ui->browserTable->setItem(row, kBrowserColIndex, id);

    this->CreateTableItemOrUpdate(row, kBrowserColHostname, ui->browserTable, info);
    this->UpdateGroupColumn(row, info);

    if(isIP)
    {
        InfoQuery *infoQuery = new InfoQuery(this);
        infoQuery->query(&info->host, info->port, id);
    }
    else
    {
        //Resolve Address
        HostQueryResult *res = new HostQueryResult(info, this, id);
        QHostInfo::lookupHost(info->hostname, res, &HostQueryResult::HostInfoResolved);
    }
    return info;
}

QImage MainWindow::GetVACImage()
{
    if(this->ui->actionDark_Theme->isChecked())
    {
        static QImage vacDark(":/icons/icons/vac.png");
        return vacDark;
    }
    else
    {
        static QImage vacLight(":/icons/icons/vac-light.png");
        return vacLight;
    }
}

QImage MainWindow::GetLockImage()
{
    if(this->ui->actionDark_Theme->isChecked())
    {
        static QImage lockDark(":/icons/icons/lock.png");
        return lockDark;
    }
    else
    {
        static QImage lockLight(":/icons/icons/lock-light.png");
        return lockLight;
    }
}

QColor MainWindow::GetTextColor()
{
    if(this->ui->actionDark_Theme->isChecked())
    {
        return Qt::white;
    }
    else
    {
        return Qt::black;
    }
}

void MainWindow::ApplyBrowserFilter()
{
    QString filterText = filterEdit->text().trimmed();
    bool hideOffline = hideOfflineCheck->isChecked();
    QString selectedGroup = groupFilterCombo->currentIndex() > 0 ? groupFilterCombo->currentText() : QString();

    for(int i = 0; i < this->ui->browserTable->rowCount(); i++)
    {
        ServerTableIndexItem *id = this->GetServerTableIndexItem(i);
        if(!id)
            continue;

        ServerInfo *info = id->GetServerInfo();
        bool visible = true;

        // Hide offline servers
        if(hideOffline && (info->queryState == QueryFailed || info->queryState == QueryResolveFailed
            || info->queryState == QueryOffline || info->queryState == QueryUnreachable))
        {
            visible = false;
        }

        // Filter by group
        if(visible && !selectedGroup.isEmpty() && info->group != selectedGroup)
        {
            visible = false;
        }

        // Filter by text (match against server name, alias, map, or host:port)
        if(visible && !filterText.isEmpty())
        {
            bool matches = false;
            // Check hostPort
            if(info->hostPort.contains(filterText, Qt::CaseInsensitive))
                matches = true;
            // Check alias
            if(!matches && !info->alias.isEmpty() && info->alias.contains(filterText, Qt::CaseInsensitive))
                matches = true;
            // Check server name (strip HTML tags for comparison)
            if(!matches && !info->serverNameRich.isEmpty())
            {
                QString plainName = info->serverNameRich;
                plainName.remove(QRegularExpression("<[^>]*>"));
                if(plainName.contains(filterText, Qt::CaseInsensitive))
                    matches = true;
            }
            // Check current map
            if(!matches && info->currentMap.contains(filterText, Qt::CaseInsensitive))
                matches = true;

            visible = matches;
        }

        this->ui->browserTable->setRowHidden(i, !visible);
    }
}

void MainWindow::UpdateGroupComboBox()
{
    QString current = groupFilterCombo->currentText();
    groupFilterCombo->blockSignals(true);
    groupFilterCombo->clear();
    groupFilterCombo->addItem("All Groups");

    QStringList groups;
    for(int i = 0; i < serverList.size(); i++)
    {
        QString g = serverList.at(i)->group;
        if(!g.isEmpty() && !groups.contains(g))
            groups.append(g);
    }
    groups.sort();
    groupFilterCombo->addItems(groups);

    int idx = groupFilterCombo->findText(current);
    if(idx >= 0)
        groupFilterCombo->setCurrentIndex(idx);
    groupFilterCombo->blockSignals(false);
}

void MainWindow::SetServerGroup(ServerInfo *info, const QString &group)
{
    info->group = group;
    // Update group column for this server's row
    for(int i = 0; i < this->ui->browserTable->rowCount(); i++)
    {
        ServerTableIndexItem *id = this->GetServerTableIndexItem(i);
        if(id && id->GetServerInfo() == info)
        {
            UpdateGroupColumn(i, info);
            break;
        }
    }
    UpdateGroupComboBox();
    ApplyBrowserFilter();
    settings->SaveSettings();
}

void MainWindow::UpdateStatusBar()
{
    int online = 0, offline = 0, totalPlayers = 0, totalMaxPlayers = 0;

    for(int i = 0; i < serverList.size(); i++)
    {
        ServerInfo *info = serverList.at(i);
        if(info->queryState == QuerySuccess)
        {
            online++;
            totalPlayers += info->currentPlayers;
            totalMaxPlayers += info->maxPlayers;
        }
        else if(info->queryState == QueryFailed || info->queryState == QueryResolveFailed
                || info->queryState == QueryOffline || info->queryState == QueryUnreachable)
        {
            offline++;
        }
    }

    int querying = serverList.size() - online - offline;
    QString status = QString("%1 Online, %2 Offline, %3/%4 Players")
        .arg(QString::number(online), QString::number(offline),
             QString::number(totalPlayers), QString::number(totalMaxPlayers));
    if(querying > 0)
        status += QString(", %1 Querying").arg(querying);

    statusBarLabel->setText(status);
}

void MainWindow::UpdateGroupColumn(int row, ServerInfo *info)
{
    QTableWidgetItem *item = this->ui->browserTable->item(row, kBrowserColGroup);
    if(!item)
    {
        item = new QTableWidgetItem();
        this->ui->browserTable->setItem(row, kBrowserColGroup, item);
    }
    item->setText(info->group);

    if(!info->group.isEmpty())
    {
        QColor groupColor = GetGroupColor(info->group);
        item->setBackground(groupColor);
        // Use dark text on light backgrounds, light text on dark backgrounds
        int brightness = (groupColor.red() * 299 + groupColor.green() * 587 + groupColor.blue() * 114) / 1000;
        item->setForeground(brightness > 128 ? Qt::black : Qt::white);
    }
    else
    {
        item->setBackground(QBrush());
        item->setForeground(this->GetTextColor());
    }
}

QColor MainWindow::GetGroupColor(const QString &group)
{
    if(group.isEmpty())
        return QColor();

    uint hash = qHash(group);
    int hue = hash % 360;
    return QColor::fromHsl(hue, 100, 160);
}
