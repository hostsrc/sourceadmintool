#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "customitems.h"
#include "settings.h"
#include <QKeyEvent>
#include <QTimer>
#include <QHBoxLayout>
#include <QLabel>

extern Settings *settings;
extern QList<ServerInfo *> serverList;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    pPlayerQuery = NULL;
    pRulesQuery = NULL;
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

    this->SetRconEnabled(false);
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
}

MainWindow::~MainWindow()
{
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

    if(isIP)
    {
        InfoQuery *infoQuery = new InfoQuery(this);
        infoQuery->query(&info->host, info->port, id);
    }
    else
    {
        //Resolve Address
        HostQueryResult *res = new HostQueryResult(info, this, id);
        QHostInfo::lookupHost(info->hostname, res, SLOT(HostInfoResolved(QHostInfo)));
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
        if(hideOffline && (info->queryState == QueryFailed || info->queryState == QueryResolveFailed))
        {
            visible = false;
        }

        // Filter by group
        if(visible && !selectedGroup.isEmpty() && info->group != selectedGroup)
        {
            visible = false;
        }

        // Filter by text (match against server name, map, or host:port)
        if(visible && !filterText.isEmpty())
        {
            bool matches = false;
            // Check hostPort
            if(info->hostPort.contains(filterText, Qt::CaseInsensitive))
                matches = true;
            // Check server name (strip HTML tags for comparison)
            if(!matches && !info->serverNameRich.isEmpty())
            {
                QString plainName = info->serverNameRich;
                plainName.remove(QRegExp("<[^>]*>"));
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
    UpdateGroupComboBox();
    ApplyBrowserFilter();
    settings->SaveSettings();
}
