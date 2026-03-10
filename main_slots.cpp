#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "worker.h"
#include "query.h"
#include "serverinfo.h"
#include "settings.h"
#include "updatechecker.h"
#include "serverbrowser.h"
#include "version.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QPalette>
#include <QDesktopServices>
#include <QWidgetAction>
#include <QTextEdit>
#include <QFile>

#define STEAMID_COLUMN 4
#define NAME_COLUMN 1

extern QPalette defaultPalette;
extern QMap<int, QString> appIDMap;
extern Settings *settings;
extern QList<ServerInfo *> serverList;
extern QColor errorColor;
extern QColor queryingColor;
extern QList<ContextMenuItem> contextMenuItems;

void MainWindow::ConnectSlots()
{
    this->ui->commandText->connect(this->ui->commandText, &QLineEdit::returnPressed, this, &MainWindow::processCommand);
    this->ui->commandText->connect(this->ui->sendChat, &QLineEdit::returnPressed, this, &MainWindow::sendChat);
    this->ui->rconPassword->connect(this->ui->rconPassword, &QLineEdit::returnPressed, this, &MainWindow::rconLogin);
    this->ui->rconPassword->connect(this->ui->rconPassword, &QLineEdit::textChanged, this, &MainWindow::passwordUpdated);
    this->ui->rconSave->connect(this->ui->rconSave, &QCheckBox::toggled, this, &MainWindow::rconSaveToggled);
    this->ui->rconLogin->connect(this->ui->rconLogin, &QPushButton::released, this, &MainWindow::rconLogin);
    this->ui->logGetLog->connect(this->ui->logGetLog, &QPushButton::released, this, &MainWindow::getLog);
    this->ui->actionAdd_Server->connect(this->ui->actionAdd_Server, &QAction::triggered, this, &MainWindow::addServerEntry);
    this->ui->actionBatch_Add_Servers->connect(this->ui->actionBatch_Add_Servers, &QAction::triggered, this, &MainWindow::batchAddServerEntry);
    this->ui->actionDark_Theme->connect(this->ui->actionDark_Theme, &QAction::triggered, this, &MainWindow::darkThemeTriggered);
    this->ui->actionSet_Log_Port->connect(this->ui->actionSet_Log_Port, &QAction::triggered, this, &MainWindow::showPortEntry);
    this->ui->actionSet_Query_Retries->connect(this->ui->actionSet_Query_Retries, &QAction::triggered, this, &MainWindow::showQueryRetriesEntry);
    this->ui->actionSet_Query_Interval->connect(this->ui->actionSet_Query_Interval, &QAction::triggered, this, &MainWindow::showQueryIntervalEntry);
    this->ui->actionCheck_Updates->connect(this->ui->actionCheck_Updates, &QAction::triggered, this, &MainWindow::checkForUpdates);
    this->ui->actionSet_Update_URL->connect(this->ui->actionSet_Update_URL, &QAction::triggered, this, &MainWindow::showUpdateUrlEntry);
    this->ui->actionSet_Steam_API_Key->connect(this->ui->actionSet_Steam_API_Key, &QAction::triggered, this, &MainWindow::showSteamApiKeyEntry);
    this->ui->actionAbout->connect(this->ui->actionAbout, &QAction::triggered, this, &MainWindow::showAbout);
    this->ui->browserTable->connect(this->ui->browserTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::browserTableItemSelected);
    this->ui->rconShow->connect(this->ui->rconShow, &QCheckBox::clicked, this, &MainWindow::showRconClicked);
    this->ui->playerTable->connect(this->ui->playerTable, &QTableWidget::customContextMenuRequested, this, &MainWindow::customPlayerContextMenu);
    this->ui->browserTable->connect(this->ui->browserTable, &QTableWidget::customContextMenuRequested, this, &MainWindow::serverBrowserContextMenu);

    connect(this->filterEdit, &QLineEdit::textChanged, this, &MainWindow::filterTextChanged);
    connect(this->hideOfflineCheck, &QCheckBox::toggled, this, &MainWindow::hideOfflineToggled);
    connect(this->groupFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::groupFilterChanged);
}

QString CreateCommand(QString command, QString subValue, ContextTypes type, QString name, QString SteamID)
{
    QString target = "";

    if(type == ContextTypeSteamID)
    {
        target = SteamID;
    }
    else if(type == ContextTypeName)
    {
        target = name;
    }

    if(!subValue.isEmpty())
    {
        if(target.isEmpty())
            return command.arg(subValue);
        else
            return command.arg(target, subValue);
    }
    else
    {
        if(target.isEmpty())
            return command;
        else
            return command.arg(target);
    }
}

void MainWindow::customPlayerContextMenu(const QPoint &pos)
{
    int row = this->ui->playerTable->rowAt(pos.y());

    if(row != -1)
    {
        QPoint globalpos = this->ui->playerTable->mapToGlobal(pos);
        globalpos.setY(globalpos.y()+15);
        globalpos.setX(globalpos.x()+5);
        QString name = this->ui->playerTable->item(row, NAME_COLUMN)->text();
        QString steamid = this->ui->playerTable->item(row, STEAMID_COLUMN)->text();

        QMenu *pContextMenu = new QMenu(this);

        for(const ContextMenuItem &ctxItem : contextMenuItems)
        {
            if((ctxItem.type == ContextTypeSteamID && steamid.isEmpty()) || (ctxItem.type == ContextTypeName && name.isEmpty()))
                continue;

            if(ctxItem.subItems.length() > 0)
            {
                QMenu *submenu = new QMenu(ctxItem.display, pContextMenu);
                submenu->addSection(ctxItem.subTitle);

                if(!ctxItem.defaultSub.isNull())
                {
                    QAction *pAction = new QAction("Default", submenu);
                    QString cmd = CreateCommand(ctxItem.defaultSub, "", ctxItem.type, name, steamid);
                    connect(pAction, &QAction::triggered, this, [this, cmd]{ playerContextMenuAction(cmd); });
                    submenu->addAction(pAction);
                }

                for(const CtxSubItem &ctxSubItem : ctxItem.subItems)
                {
                    QAction *pAction = new QAction(ctxSubItem.display, submenu);
                    QString cmd = CreateCommand(ctxItem.cmd, ctxSubItem.val, ctxItem.type, name, steamid);
                    connect(pAction, &QAction::triggered, this, [this, cmd]{ playerContextMenuAction(cmd); });
                    submenu->addAction(pAction);
                }
                pContextMenu->addMenu(submenu);
            }
            else
            {
                QAction *pAction = new QAction(ctxItem.display, pContextMenu);
                QString cmd = CreateCommand(ctxItem.cmd, "", ctxItem.type, name, steamid);
                connect(pAction, &QAction::triggered, this, [this, cmd]{ playerContextMenuAction(cmd); });
                pContextMenu->addAction(pAction);
            }
        }

        pContextMenu->connect(pContextMenu, &QMenu::aboutToHide, this, &MainWindow::hideContextMenu);
        pContextMenu->exec(globalpos);
    }
}
void MainWindow::hideContextMenu()
{
    QObject* obj = sender();
    obj->deleteLater();
}

void MainWindow::playerContextMenuAction(const QString &cmd)
{
    if(this->ui->browserTable->selectedItems().size() == 0)
    {
        this->browserTableItemSelected();
        return;
    }

    ServerTableIndexItem *item = this->GetServerTableIndexItem(this->ui->browserTable->currentRow());
    ServerInfo *info = item->GetServerInfo();

    if(info && (info->rcon == nullptr || !info->rcon->isAuthed))
    {
        QList<QueuedCommand>cmds;
        cmds.append(QueuedCommand(cmd, QueuedCommandType::ContextCommand));
        this->rconLoginQueued(cmds);
        return;
    }
    info->rcon->execCommand(cmd, false);
}

void MainWindow::serverBrowserContextMenu(const QPoint &pos)
{
    int row = this->ui->browserTable->rowAt(pos.y());

    QPoint globalpos = this->ui->browserTable->mapToGlobal(pos);
    globalpos.setY(globalpos.y()+15);
    globalpos.setX(globalpos.x()+5);
    QMenu *pContextMenu = new QMenu(this);

    if(row == -1)
    {
        QAction *add = new QAction("Add Server", pContextMenu);
        add->connect(add, &QAction::triggered, this, [this]{addServerEntry();});
        pContextMenu->addAction(add);

        QAction *batchAdd = new QAction("Batch Add Servers...", pContextMenu);
        batchAdd->connect(batchAdd, &QAction::triggered, this, [this]{batchAddServerEntry();});
        pContextMenu->addAction(batchAdd);
    }
    else
    {
        //Add delete action.
        QAction *del = new QAction("Delete Server", pContextMenu);
        del->connect(del, &QAction::triggered, this, [this]{deleteServerDialog();});
        pContextMenu->addAction(del);

        //Add connect action
        QAction *con = new QAction("Connect", pContextMenu);
        con->connect(con, &QAction::triggered, this, [this]{connectToServer();}, Qt::QueuedConnection);
        pContextMenu->addAction(con);

        pContextMenu->addSeparator();

        //Add set group action
        ServerTableIndexItem *id = this->GetServerTableIndexItem(row);
        if(id)
        {
            ServerInfo *info = id->GetServerInfo();
            QAction *setGroup = new QAction(info->group.isEmpty() ? "Set Group..." : QString("Group: %1 (Change...)").arg(info->group), pContextMenu);
            setGroup->connect(setGroup, &QAction::triggered, this, [this, info]{
                bool ok;
                QString group = QInputDialog::getText(this, tr("Set Server Group"), tr("Group name (leave empty to remove):"), QLineEdit::Normal, info->group, &ok, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
                if(ok)
                {
                    this->SetServerGroup(info, group.trimmed());
                }
            });
            pContextMenu->addAction(setGroup);

            if(!info->group.isEmpty())
            {
                QAction *removeGroup = new QAction("Remove from Group", pContextMenu);
                removeGroup->connect(removeGroup, &QAction::triggered, this, [this, info]{
                    this->SetServerGroup(info, "");
                });
                pContextMenu->addAction(removeGroup);
            }

            pContextMenu->addSeparator();

            // Set alias action
            QAction *setAlias = new QAction(info->alias.isEmpty() ? "Set Alias..." : QString("Alias: %1 (Change...)").arg(info->alias), pContextMenu);
            setAlias->connect(setAlias, &QAction::triggered, this, [this, info]{
                bool ok;
                QString alias = QInputDialog::getText(this, tr("Set Server Alias"), tr("Display name (leave empty to remove):"), QLineEdit::Normal, info->alias, &ok, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
                if(ok)
                {
                    info->alias = alias.trimmed();
                    // Refresh hostname display
                    for(int i = 0; i < this->ui->browserTable->rowCount(); i++)
                    {
                        ServerTableIndexItem *sid = this->GetServerTableIndexItem(i);
                        if(sid && sid->GetServerInfo() == info)
                        {
                            this->CreateTableItemOrUpdate(i, kBrowserColHostname, this->ui->browserTable, info);
                            break;
                        }
                    }
                    settings->SaveSettings();
                }
            });
            pContextMenu->addAction(setAlias);

            // Set notes action
            QAction *setNotes = new QAction(info->notes.isEmpty() ? "Add Notes..." : "Edit Notes...", pContextMenu);
            setNotes->connect(setNotes, &QAction::triggered, this, [this, info]{
                QDialog dialog(this);
                dialog.setWindowTitle("Server Notes");
                dialog.setMinimumSize(400, 200);
                QVBoxLayout *layout = new QVBoxLayout(&dialog);
                QTextEdit *textEdit = new QTextEdit(&dialog);
                textEdit->setPlainText(info->notes);
                layout->addWidget(textEdit);
                QHBoxLayout *btnLayout = new QHBoxLayout();
                QPushButton *okBtn = new QPushButton("OK", &dialog);
                QPushButton *cancelBtn = new QPushButton("Cancel", &dialog);
                btnLayout->addStretch();
                btnLayout->addWidget(okBtn);
                btnLayout->addWidget(cancelBtn);
                layout->addLayout(btnLayout);
                connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
                connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
                if(dialog.exec() == QDialog::Accepted)
                {
                    info->notes = textEdit->toPlainText();
                    settings->SaveSettings();
                    // Refresh info table if this server is selected
                    if(this->ui->browserTable->currentRow() >= 0)
                    {
                        ServerTableIndexItem *sid = this->GetServerTableIndexItem(this->ui->browserTable->currentRow());
                        if(sid && sid->GetServerInfo() == info)
                            this->UpdateInfoTable(info);
                    }
                }
            });
            pContextMenu->addAction(setNotes);
        }
    }
    pContextMenu->connect(pContextMenu, &QMenu::aboutToHide, this, &MainWindow::hideContextMenu);
    pContextMenu->exec(globalpos);
}

void MainWindow::addServerEntry()
{
    QMessageBox message(this);
    while(true)
    {
        bool ok;
        QString server = QInputDialog::getText(this, tr("Add Server"), tr("IP:Port"), QLineEdit::Normal,"", &ok, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);

        if(ok && !server.isEmpty())
        {
            AddServerError error;
            this->AddServerToList(server, &error);

            if(error == AddServerError_None || error == AddServerError_Hostname)
            {
                settings->SaveSettings();
                break;
            }
            else if(error == AddServerError_AlreadyExists)//Valid ip but exists.
            {
                message.setText("Server already exists");
                message.exec();
                break;
            }
            else
            {
                message.setText("Invalid IP or Port");
                message.exec();
            }
        }
        else
            break;
    }
}

void MainWindow::batchAddServerEntry()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Batch Add Servers");
    dialog.setMinimumSize(400, 300);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *label = new QLabel("Enter server addresses (one per line, IP:Port):", &dialog);
    layout->addWidget(label);

    QTextEdit *textEdit = new QTextEdit(&dialog);
    textEdit->setPlaceholderText("192.168.1.1:27015\n192.168.1.2:27015\nexample.com:27015");
    layout->addWidget(textEdit);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton("Add All", &dialog);
    QPushButton *cancelBtn = new QPushButton("Cancel", &dialog);
    btnLayout->addStretch();
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(addBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    if(dialog.exec() == QDialog::Accepted)
    {
        QString text = textEdit->toPlainText().trimmed();
        if(text.isEmpty()) return;

        QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        int added = 0, skipped = 0;

        for(int i = 0; i < lines.size(); i++)
        {
            QString server = lines.at(i).trimmed();
            if(server.isEmpty()) continue;

            AddServerError error;
            this->AddServerToList(server, &error);

            if(error == AddServerError_None || error == AddServerError_Hostname)
                added++;
            else
                skipped++;
        }

        if(added > 0)
            settings->SaveSettings();

        if(skipped > 0)
        {
            QMessageBox msg(this);
            msg.setText(QString("Added %1 server(s), skipped %2 (invalid or duplicate).").arg(added).arg(skipped));
            msg.exec();
        }
        this->UpdateStatusBar();
    }
}

bool MainWindow::deleteServerDialog()
{
    // Collect unique selected rows
    QList<int> selectedRows;
    QList<QTableWidgetItem *> selectedItems = this->ui->browserTable->selectedItems();
    for(int i = 0; i < selectedItems.size(); i++)
    {
        int r = selectedItems.at(i)->row();
        if(!selectedRows.contains(r))
            selectedRows.append(r);
    }

    if(selectedRows.isEmpty())
        return false;

    QString deleteText;
    if(selectedRows.size() == 1)
    {
        ServerTableIndexItem *id = this->GetServerTableIndexItem(selectedRows.at(0));
        if(!id) return false;
        deleteText = QString("Delete %1?").arg(id->GetServerInfo()->hostPort);
    }
    else
    {
        deleteText = QString("Delete %1 servers?").arg(selectedRows.size());
    }

    QMessageBox message(this);
    message.setInformativeText(deleteText);
    message.setText("Delete server(s) from list?");
    message.setStandardButtons(QMessageBox::Ok|QMessageBox::Cancel);
    message.setDefaultButton(QMessageBox::Cancel);
    int ret = message.exec();

    if(ret == QMessageBox::Ok)
    {
        // Sort rows descending so removal doesn't shift indices
        std::sort(selectedRows.begin(), selectedRows.end(), std::greater<int>());

        for(int i = 0; i < selectedRows.size(); i++)
        {
            int row = selectedRows.at(i);
            ServerTableIndexItem *id = this->GetServerTableIndexItem(row);
            if(!id) continue;

            ServerInfo *info = id->GetServerInfo();
            this->ui->browserTable->removeRow(row);
            serverList.removeAll(info);
            pLogHandler->removeServer(info);
            delete info;
        }

        // Renumber remaining index items
        for(int i = 0; i < this->ui->browserTable->rowCount(); i++)
        {
            QTableWidgetItem *item = this->ui->browserTable->item(i, kBrowserColIndex);
            if(item)
                item->setData(Qt::DisplayRole, i + 1);
        }

        settings->SaveSettings();
        this->UpdateStatusBar();
        this->UpdateGroupComboBox();

        if(this->ui->browserTable->selectedItems().size() == 0)
        {
            this->ui->rulesTable->setRowCount(0);
            this->ui->playerTable->setRowCount(0);
            this->ui->infoTable->setRowCount(0);

            this->ui->chatOutput->setHtml("");
            this->ui->commandOutput->setPlainText("");
            this->ui->rconPassword->setText("");
            this->ui->rconSave->setChecked(false);
        }
        return true;
    }
    return false;
}

void MainWindow::connectToServer()
{
    ServerTableIndexItem *id = this->GetServerTableIndexItem(this->ui->browserTable->currentRow());

    if(!id)
        return;

    ServerInfo *info = id->GetServerInfo();
    QString url = QString("steam://connect/%1").arg(info->hostPort);
    QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::browserTableItemSelected()
{
    if(this->ui->browserTable->selectedItems().size() == 0)
    {
        this->SetRconEnabled(false);
        return;
    }

    this->SetRconEnabled(true);

    ServerTableIndexItem *item = this->GetServerTableIndexItem(this->ui->browserTable->currentRow());
    ServerInfo *info = item->GetServerInfo();

    this->SetRconSignals(true);
    this->RestoreRcon(info);
    this->SetRconSignals(false);

    this->UpdateSelectedItemInfo(true, true);
}

void MainWindow::darkThemeTriggered()
{
    // When user manually toggles (not during startup load), save explicit preference
    if(settings && this->isVisible())
        settings->pSettings->setValue("themeMode", this->ui->actionDark_Theme->isChecked() ? "dark" : "light");

    if(this->ui->actionDark_Theme->isChecked())
    {
        QPalette palette;
        palette.setColor(QPalette::Window, QColor(33,33,37));
        palette.setColor(QPalette::WindowText, QColor(250,250,250));
        palette.setColor(QPalette::Base, QColor(46,46,46));
        palette.setColor(QPalette::AlternateBase, QColor(62,62,62));
        palette.setColor(QPalette::ToolTipBase, QColor(46,46,46));
        palette.setColor(QPalette::ToolTipText, QColor(250,250,250));
        palette.setColor(QPalette::Text, QColor(250,250,250));
        palette.setColor(QPalette::Button, QColor(33,33,37));
        palette.setColor(QPalette::ButtonText, QColor(250,250,250));
        palette.setColor(QPalette::BrightText, QColor(220,100,80));
        palette.setColor(QPalette::Highlight, QColor(100,90,200));
        palette.setColor(QPalette::HighlightedText, QColor(250,250,250));
        palette.setColor(QPalette::Link, QColor(250,250,250));
        palette.setColor(QPalette::LinkVisited, QColor(174,174,174));
        qApp->setPalette(palette);

        QFile qss(":/themes/dark-theme.qss");
        if(qss.open(QFile::ReadOnly | QFile::Text))
        {
            qApp->setStyleSheet(qss.readAll());
            qss.close();
        }
    }
    else
    {
        qApp->setPalette(defaultPalette);
        qApp->setStyleSheet("");
    }

    QTableWidgetItem *hostname;
    QColor color = this->GetTextColor();

    for(int i = 0; i < this->ui->browserTable->rowCount(); i++)
    {
        hostname = this->ui->browserTable->item(i, kBrowserColHostname);
        if(this->ui->browserTable->item(i, kBrowserColVACIcon))
        {
            this->ui->browserTable->item(i, kBrowserColVACIcon)->setData(Qt::DecorationRole, this->GetVACImage());
        }
        if(this->ui->browserTable->item(i, kBrowserColLockIcon))
        {
            this->ui->browserTable->item(i, kBrowserColLockIcon)->setData(Qt::DecorationRole, this->GetLockImage());
        }
        if(hostname && (hostname->foreground().color() == Qt::white || hostname->foreground().color() == Qt::black))
        {
            hostname->setForeground(color);
        }
    }
}

void MainWindow::showPortEntry()
{
    bool ok;
    uint port = QInputDialog::getInt(this, tr("Select Port"), tr("Port Range %1 -> %2").arg(QString::number(PORT_MIN), QString::number(PORT_MAX)), this->u16logPort, PORT_MIN, PORT_MAX, 1, &ok, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);

    if(ok)
    {
        this->u16logPort = port;
        settings->SaveSettings();
    }
}

void MainWindow::showQueryRetriesEntry()
{
    bool ok;
    int retries = QInputDialog::getInt(this, tr("Query Retries"), tr("Max retries per query (0-5)"), g_queryMaxRetries, 0, 5, 1, &ok, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);

    if(ok)
    {
        g_queryMaxRetries = retries;
        settings->SaveSettings();
    }
}

void MainWindow::showQueryIntervalEntry()
{
    bool ok;
    int interval = QInputDialog::getInt(this, tr("Query Interval"), tr("Server refresh interval in seconds (5-300)"), g_queryInterval, 5, 300, 1, &ok, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);

    if(ok)
    {
        g_queryInterval = interval;
        settings->SaveSettings();
    }
}

void MainWindow::checkForUpdates()
{
    m_updateChecker->checkForUpdates(true);
}

void MainWindow::showUpdateUrlEntry()
{
    bool ok;
    QString url = QInputDialog::getText(this, tr("Update URL"),
        tr("GitHub Releases API URL:"), QLineEdit::Normal,
        m_updateChecker->updateUrl(), &ok,
        Qt::WindowSystemMenuHint | Qt::WindowTitleHint);

    if(ok)
    {
        m_updateChecker->setUpdateUrl(url);
        settings->SaveSettings();
    }
}

void MainWindow::showSteamApiKeyEntry()
{
    bool ok;
    QString key = QInputDialog::getText(this, tr("Steam API Key"),
        tr("Enter your Steam Web API key.\n"
           "Get one free at: https://steamcommunity.com/dev/apikey"),
        QLineEdit::Normal,
        m_serverBrowser->apiKey(), &ok,
        Qt::WindowSystemMenuHint | Qt::WindowTitleHint);

    if(ok)
    {
        m_serverBrowser->setApiKey(key);
        settings->SaveSettings();
    }
}

void MainWindow::showAbout()
{
    QMessageBox message(this);
    message.setTextFormat(Qt::RichText);
    message.setText(
                QString("Version: %1 (%2)\n").arg(APP_VERSION_STRING, APP_COMMIT_HASH) +
                "Created by Dr!fter @ <a href=\"https://github.com/Drifter321\">https://github.com/Drifter321</a><br>"
                "Using miniupnpc @ <a href=\"https://github.com/miniupnp/miniupnp\">https://github.com/miniupnp/miniupnp</a><br><br>"
                "This product includes GeoLite2 data created by MaxMind, available from<br>"
                "<a href=\"http://www.maxmind.com\">http://www.maxmind.com</a>."
            );
    message.exec();
}

void MainWindow::AddRconHistory(QString chat)
{
    while(this->commandHistory.size() > 30)
        this->commandHistory.removeLast();

    this->commandHistory.prepend(chat);
    this->commandIter->toFront();
    this->commandIterDirection = kIterInit;
}

void MainWindow::AddChatHistory(QString txt)
{
    while(this->sayHistory.size() > 30)
        this->sayHistory.removeLast();

    this->sayHistory.prepend(txt);
    this->sayIter->toFront();
    this->sayIterDirection = kIterInit;
}

void MainWindow::filterTextChanged(const QString &)
{
    this->ApplyBrowserFilter();
}

void MainWindow::hideOfflineToggled(bool)
{
    this->ApplyBrowserFilter();
    settings->SaveSettings();
}

void MainWindow::groupFilterChanged(int)
{
    this->ApplyBrowserFilter();
}
