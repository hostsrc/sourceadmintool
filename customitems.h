#ifndef PLAYERTIMEITEM_H
#define PLAYERTIMEITEM_H

#include <QTableWidgetItem>
#include <QWidget>
#include <QList>

class ServerInfo;
class MainWindow;
struct TimestampedValue;

//Time played in player area
class PlayerTimeTableItem : public QTableWidgetItem
{
public:
    void updateTime(float newTime);
    bool operator< (const QTableWidgetItem &other) const
    {
        return (this->time < ((PlayerTimeTableItem *)&other)->time);
    }
    float getTime(){return time;}
private:
    void setTime();
    float time;
};

//Server items
class ServerTableIndexItem : public QTableWidgetItem
{
public:
    ServerTableIndexItem(ServerInfo *info);
    ServerTableIndexItem(ServerInfo *info, QString text);
    ServerInfo *GetServerInfo(){return serverInfo;}
private:
    ServerInfo *serverInfo = nullptr;
};


//Player count in browser area
class PlayerTableItem : public QTableWidgetItem
{
public:
    bool operator< (const QTableWidgetItem &other) const
    {
        return (this->players < ((PlayerTableItem *)&other)->players);
    }
    int players;
};

// Interactive graph widget with timestamp tooltips on hover
class GraphWidget : public QWidget
{
    Q_OBJECT
public:
    enum GraphType { PingGraph, PlayerGraph };

    GraphWidget(const QList<TimestampedValue> *data, GraphType type, int maxValue, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    const QList<TimestampedValue> *dataList;
    GraphType graphType;
    int maxVal;
    int getVisibleStartIndex() const;
};

#endif // PLAYERTIMEITEM_H
