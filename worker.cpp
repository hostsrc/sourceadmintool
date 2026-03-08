#include "worker.h"
#include "query.h"
#include "serverinfo.h"
#include <QString>

void Worker::getServerInfo(QHostAddress *host, quint16 port, ServerTableIndexItem *item)
{
    InfoReply *reply = GetInfoReply(*host, port);
    emit serverInfoReady(reply, item);
    this->currentThread()->quit();
}

void Worker::getPlayerInfo(QHostAddress *host, quint16 port, ServerTableIndexItem *item)
{
    bool goldsrcSplits = item->GetServerInfo()->goldsrc;
    QList<PlayerInfo> *list = GetPlayerReply(*host, port, goldsrcSplits);
    emit playerInfoReady(list, item);
    this->currentThread()->quit();
}

void Worker::getRulesInfo(QHostAddress *host, quint16 port, ServerTableIndexItem *item)
{
    bool goldsrcSplits = item->GetServerInfo()->goldsrc;
    QList<RulesInfo> *list = GetRulesReply(*host, port, goldsrcSplits);
    emit rulesInfoReady(list, item);
    this->currentThread()->quit();
}
