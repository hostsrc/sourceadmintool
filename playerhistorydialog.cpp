#include "playerhistorydialog.h"

#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QLinearGradient>

// --- HistoryGraphWidget ---

HistoryGraphWidget::HistoryGraphWidget(QWidget *parent)
    : QWidget(parent), maxVal(32), currentRange(Range24h)
{
    setMouseTracking(true);
    setMinimumHeight(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void HistoryGraphWidget::setData(const QList<PlayerCountRecord> &records, int maxPlayers)
{
    dataRecords = records;
    maxVal = maxPlayers > 0 ? maxPlayers : 32;
    update();
}

void HistoryGraphWidget::setTimeRange(TimeRange range)
{
    currentRange = range;
}

void HistoryGraphWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int margin = 40;
    int graphW = w - margin * 2;
    int graphH = h - margin * 2;

    if(dataRecords.isEmpty() || graphW <= 0 || graphH <= 0)
    {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "No data available for this time range");
        return;
    }

    // Background
    painter.fillRect(QRect(margin, margin, graphW, graphH), QColor(30, 30, 34));

    // Draw grid lines
    painter.setPen(QPen(QColor(60, 60, 65), 1));
    int gridLines = 4;
    for(int i = 0; i <= gridLines; i++)
    {
        int y = margin + (graphH * i / gridLines);
        painter.drawLine(margin, y, margin + graphW, y);

        // Y-axis labels
        int val = maxVal - (maxVal * i / gridLines);
        painter.setPen(Qt::gray);
        painter.drawText(0, y - 8, margin - 5, 16, Qt::AlignRight | Qt::AlignVCenter, QString::number(val));
        painter.setPen(QPen(QColor(60, 60, 65), 1));
    }

    // Time range for X-axis labels
    qint64 startTime = dataRecords.first().timestamp;
    qint64 endTime = dataRecords.last().timestamp;
    qint64 timeSpan = endTime - startTime;

    // Draw X-axis time labels
    painter.setPen(Qt::gray);
    int numLabels = qMin(6, graphW / 80);
    for(int i = 0; i <= numLabels; i++)
    {
        qint64 t = startTime + (timeSpan * i / numLabels);
        int x = margin + (graphW * i / numLabels);

        QString label;
        if(currentRange == Range24h)
            label = QDateTime::fromSecsSinceEpoch(t).toString("HH:mm");
        else
            label = QDateTime::fromSecsSinceEpoch(t).toString("MM/dd HH:mm");

        painter.drawText(x - 30, h - margin + 5, 60, 20, Qt::AlignHCenter | Qt::AlignTop, label);
    }

    // Draw the filled area graph
    QLinearGradient gradient(0, margin, 0, margin + graphH);
    gradient.setColorAt(0.0, QColor(0, 150, 255, 180));
    gradient.setColorAt(1.0, QColor(0, 150, 255, 30));

    QPolygonF fillPoly;
    QPolygonF linePoly;

    for(int i = 0; i < dataRecords.size(); i++)
    {
        float xRatio = (timeSpan > 0) ? (float)(dataRecords[i].timestamp - startTime) / timeSpan : 0;
        float yRatio = (float)dataRecords[i].playerCount / maxVal;

        float x = margin + xRatio * graphW;
        float y = margin + graphH - (yRatio * graphH);

        linePoly.append(QPointF(x, y));
    }

    // Build fill polygon (line + bottom edge)
    if(!linePoly.isEmpty())
    {
        fillPoly = linePoly;
        fillPoly.append(QPointF(margin + graphW, margin + graphH));
        fillPoly.append(QPointF(margin, margin + graphH));

        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawPolygon(fillPoly);

        // Draw line
        painter.setPen(QPen(QColor(0, 150, 255), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(linePoly);
    }

    // Draw crosshair on hover
    QPoint mousePos = mapFromGlobal(QCursor::pos());
    if(rect().contains(mousePos) && mousePos.x() >= margin && mousePos.x() <= margin + graphW)
    {
        painter.setPen(QPen(QColor(255, 255, 255, 120), 1, Qt::DashLine));
        painter.drawLine(mousePos.x(), margin, mousePos.x(), margin + graphH);
    }

    // Border
    painter.setPen(QPen(QColor(80, 80, 85), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(margin, margin, graphW, graphH);
}

void HistoryGraphWidget::mouseMoveEvent(QMouseEvent *event)
{
    int margin = 40;
    int graphW = width() - margin * 2;
    int x = event->position().toPoint().x() - margin;

    if(dataRecords.isEmpty() || graphW <= 0 || x < 0 || x > graphW)
    {
        QToolTip::hideText();
        QWidget::mouseMoveEvent(event);
        return;
    }

    qint64 startTime = dataRecords.first().timestamp;
    qint64 endTime = dataRecords.last().timestamp;
    qint64 timeSpan = endTime - startTime;

    if(timeSpan <= 0)
    {
        QWidget::mouseMoveEvent(event);
        return;
    }

    qint64 targetTime = startTime + (qint64)((double)x / graphW * timeSpan);

    // Find nearest data point
    int nearest = 0;
    qint64 minDist = qAbs(dataRecords[0].timestamp - targetTime);
    for(int i = 1; i < dataRecords.size(); i++)
    {
        qint64 dist = qAbs(dataRecords[i].timestamp - targetTime);
        if(dist < minDist)
        {
            minDist = dist;
            nearest = i;
        }
    }

    const PlayerCountRecord &r = dataRecords[nearest];
    QString timeStr = QDateTime::fromSecsSinceEpoch(r.timestamp).toString("yyyy-MM-dd HH:mm:ss");
    QToolTip::showText(event->globalPosition().toPoint(),
        QString("Players: %1/%2\nTime: %3").arg(
            QString::number(r.playerCount),
            QString::number(r.maxPlayers),
            timeStr));
    update();

    QWidget::mouseMoveEvent(event);
}

// --- PlayerHistoryDialog ---

PlayerHistoryDialog::PlayerHistoryDialog(const QString &serverKey, const QString &serverName, int maxPlayers, QWidget *parent)
    : QDialog(parent), serverKey(serverKey), maxPlayers(maxPlayers)
{
    setWindowTitle(QString("Player History - %1").arg(serverName));
    setMinimumSize(700, 400);
    resize(800, 450);

    QVBoxLayout *layout = new QVBoxLayout(this);

    // Time range buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btn24h = new QPushButton("24 Hours", this);
    btn7D = new QPushButton("7 Days", this);
    btn30D = new QPushButton("30 Days", this);

    btn24h->setCheckable(true);
    btn7D->setCheckable(true);
    btn30D->setCheckable(true);
    btn24h->setChecked(true);

    btnLayout->addStretch();
    btnLayout->addWidget(btn24h);
    btnLayout->addWidget(btn7D);
    btnLayout->addWidget(btn30D);
    btnLayout->addStretch();

    layout->addLayout(btnLayout);

    // Graph
    graphWidget = new HistoryGraphWidget(this);
    layout->addWidget(graphWidget, 1);

    connect(btn24h, &QPushButton::clicked, this, &PlayerHistoryDialog::onRange24h);
    connect(btn7D, &QPushButton::clicked, this, &PlayerHistoryDialog::onRange7D);
    connect(btn30D, &QPushButton::clicked, this, &PlayerHistoryDialog::onRange30D);

    // Load initial data
    loadData(HistoryGraphWidget::Range24h);
}

void PlayerHistoryDialog::onRange24h()
{
    btn24h->setChecked(true);
    btn7D->setChecked(false);
    btn30D->setChecked(false);
    loadData(HistoryGraphWidget::Range24h);
}

void PlayerHistoryDialog::onRange7D()
{
    btn24h->setChecked(false);
    btn7D->setChecked(true);
    btn30D->setChecked(false);
    loadData(HistoryGraphWidget::Range7D);
}

void PlayerHistoryDialog::onRange30D()
{
    btn24h->setChecked(false);
    btn7D->setChecked(false);
    btn30D->setChecked(true);
    loadData(HistoryGraphWidget::Range30D);
}

void PlayerHistoryDialog::loadData(HistoryGraphWidget::TimeRange range)
{
    qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 startTime;

    switch(range)
    {
        case HistoryGraphWidget::Range24h:
            startTime = now - (24 * 3600);
            break;
        case HistoryGraphWidget::Range7D:
            startTime = now - (7 * 24 * 3600);
            break;
        case HistoryGraphWidget::Range30D:
            startTime = now - (30 * 24 * 3600);
            break;
    }

    QList<PlayerCountRecord> records = Database::instance()->getPlayerCounts(serverKey, startTime, now);
    graphWidget->setTimeRange(range);
    graphWidget->setData(records, maxPlayers);
}
