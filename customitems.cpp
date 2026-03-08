#include "customitems.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "serverinfo.h"

#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <QLinearGradient>

void PlayerTimeTableItem::updateTime(float newTime)
{
    this->time = newTime;
    this->setTime();
}

void PlayerTimeTableItem::setTime()
{
    QString res;

    int days = qRound(this->time) / 60 / 60 / 24;
    int hours = qRound(this->time) / 60 / 60 % 24;
    int minutes = qRound(this->time) / 60 % 60;
    int seconds = qRound(this->time) % 60;

    if(days)
    {
        res.append(QString("%1%2").arg(QString::number(days), "d "));
    }
    if(hours)
    {
        res.append(QString("%1%2").arg(QString::number(hours), "h "));
    }
    if(minutes)
    {
        res.append(QString("%1%2").arg(QString::number(minutes), "m "));
    }
    res.append(QString("%1%2").arg(QString::number(seconds), "s"));

    this->setText(res);
}

ServerTableIndexItem::ServerTableIndexItem(ServerInfo *info) : QTableWidgetItem()
{
    this->serverInfo = info;
}

ServerTableIndexItem::ServerTableIndexItem(ServerInfo *info, QString text) : QTableWidgetItem(text)
{
    this->serverInfo = info;
}

ServerTableIndexItem *MainWindow::GetServerTableIndexItem(size_t row)
{
    return ((ServerTableIndexItem *)ui->browserTable->item(row, kBrowserColIndex));
}

GraphWidget::GraphWidget(const QList<TimestampedValue> *data, GraphType type, int maxValue, QWidget *parent)
    : QWidget(parent), dataList(data), graphType(type), maxVal(maxValue)
{
    setMouseTracking(true);
    setFixedHeight(50);
}

int GraphWidget::getVisibleStartIndex() const
{
    int w = width();
    return (dataList->length() - w) > 0 ? dataList->length() - w : 0;
}

void GraphWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setPen(QPen(Qt::transparent));

    QLinearGradient lgrad(QPoint(0, 0), QPoint(0, 50));
    if(graphType == PingGraph)
    {
        lgrad.setColorAt(0.0, Qt::red);
        lgrad.setColorAt(1.0, Qt::green);
    }
    else
    {
        lgrad.setColorAt(0.0, QColor(255, 165, 0));
        lgrad.setColorAt(1.0, QColor(0, 150, 255));
    }
    painter.setBrush(lgrad);

    int idx = getVisibleStartIndex();

    for(int j = idx; j < dataList->length(); j++)
    {
        int val = dataList->at(j).value;
        int h;
        if(graphType == PingGraph)
        {
            h = qRound(((float)val / 300.0) * 50);
            if(h <= 1) h = 2;
            if(h >= 50) h = 50;
        }
        else
        {
            h = qRound(((float)val / (float)maxVal) * 50);
            if(h <= 0) h = 1;
            if(h > 50) h = 50;
        }
        painter.drawRect((j - idx), 50 - h, 1, h);
    }

    // Draw crosshair line at mouse position if inside widget
    QPoint mousePos = mapFromGlobal(QCursor::pos());
    if(rect().contains(mousePos))
    {
        painter.setPen(QPen(QColor(255, 255, 255, 150), 1));
        painter.drawLine(mousePos.x(), 0, mousePos.x(), 50);
    }
}

void GraphWidget::mouseMoveEvent(QMouseEvent *event)
{
    int x = event->position().toPoint().x();
    int idx = getVisibleStartIndex();
    int dataIndex = idx + x;

    if(dataIndex >= 0 && dataIndex < dataList->length())
    {
        const TimestampedValue &tv = dataList->at(dataIndex);
        QString timeStr = QDateTime::fromMSecsSinceEpoch(tv.timestamp).toString("hh:mm:ss");
        QString label = (graphType == PingGraph) ? "Ping" : "Players";
        QToolTip::showText(event->globalPosition().toPoint(),
            QString("%1: %2\nTime: %3").arg(label, QString::number(tv.value), timeStr));
        update(); // repaint to update crosshair
    }
    else
    {
        QToolTip::hideText();
    }
    QWidget::mouseMoveEvent(event);
}
