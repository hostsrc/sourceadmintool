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

private:
    void populateGames();
    void setSearching(bool searching);

    MainWindow *m_main;
    QNetworkAccessManager *m_manager;
    QComboBox *m_gameCombo;
    QLineEdit *m_mapFilter;
    QPushButton *m_searchBtn;
    QTableWidget *m_table;
    QLabel *m_statusLabel;
    QString m_apiKey;

    QList<BrowserServerEntry> m_results;
    static const QList<GameEntry> s_games;
};

#endif // SERVERBROWSER_H
