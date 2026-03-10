#ifndef SERVERBROWSER_H
#define SERVERBROWSER_H

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QMap>

class MainWindow;

struct BrowserServerEntry
{
    QString addr;       // "ip:port"
    QString name;
    QString map;
    int players;
    int maxPlayers;
    int bots;
    bool vac;
    bool password;
    QString gamedir;
    QString version;
    QString tags;
};

struct GameEntry
{
    QString name;
    int appId;
};

class ServerBrowser : public QWidget
{
    Q_OBJECT
public:
    explicit ServerBrowser(MainWindow *main, QWidget *parent = nullptr);

    QString apiKey() const { return m_apiKey; }
    void setApiKey(const QString &key) { m_apiKey = key; }

private slots:
    void onSearchClicked();
    void onReplyFinished(QNetworkReply *reply);
    void onTableDoubleClicked(int row, int column);
    void onContextMenu(const QPoint &pos);
    void onLocalFilterChanged(const QString &text);
    void onCheckListingClicked();
    void onCheckListingReply(QNetworkReply *reply);

private:
    void populateGames();
    void setSearching(bool searching);
    void processNextCheckAddr();

    MainWindow *m_main;
    QNetworkAccessManager *m_manager;
    QNetworkAccessManager *m_checkManager;
    QComboBox *m_gameCombo;
    QLineEdit *m_nameFilter;
    QLineEdit *m_mapFilter;
    QLineEdit *m_ipFilter;
    QLineEdit *m_localFilter;
    QPushButton *m_searchBtn;
    QPushButton *m_checkListingBtn;
    QTableWidget *m_table;
    QLabel *m_statusLabel;
    QString m_apiKey;

    // Check listing state
    QStringList m_checkAddrs;                       // IPs remaining to query
    QSet<QString> m_listedAddrs;                    // addr (ip:port) found on Steam
    QMap<QString, BrowserServerEntry> m_checkResults; // Steam data by addr
    int m_checkTotal = 0;
    int m_checkPending = 0;

    QList<BrowserServerEntry> m_results;
    static const QList<GameEntry> s_games;
};

#endif // SERVERBROWSER_H
