#include "playerhistorydialog.h"

#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QLinearGradient>
#include <QApplication>

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
    int leftMargin = 45;
    int rightMargin = 15;
    int topMargin = 15;
    int bottomMargin = 30;
    int graphW = w - leftMargin - rightMargin;
    int graphH = h - topMargin - bottomMargin;

    // Theme-aware colors from palette
    QPalette pal = qApp->palette();
    QColor bgColor = pal.color(QPalette::Base);
    QColor gridColor = pal.color(QPalette::Mid);
    QColor labelColor = pal.color(QPalette::Text);
    gridColor.setAlpha(60);

    if(dataRecords.isEmpty() || graphW <= 0 || graphH <= 0)
    {
        painter.setPen(pal.color(QPalette::PlaceholderText));
        painter.drawText(rect(), Qt::AlignCenter, "No data available for this time range");
        return;
    }

    // Background
    painter.fillRect(QRect(leftMargin, topMargin, graphW, graphH), bgColor);

    // Draw grid lines and Y-axis labels
    painter.setPen(QPen(gridColor, 1));
    int gridLines = 4;
    for(int i = 0; i <= gridLines; i++)
    {
        int y = topMargin + (graphH * i / gridLines);
        painter.drawLine(leftMargin, y, leftMargin + graphW, y);

        int val = maxVal - (maxVal * i / gridLines);
        painter.setPen(labelColor);
        painter.drawText(0, y - 8, leftMargin - 8, 16, Qt::AlignRight | Qt::AlignVCenter, QString::number(val));
        painter.setPen(QPen(gridColor, 1));
    }

    // Time range for X-axis labels
    qint64 startTime = dataRecords.first().timestamp;
    qint64 endTime = dataRecords.last().timestamp;
    qint64 timeSpan = endTime - startTime;

    // Draw X-axis time labels
    painter.setPen(labelColor);
    int numLabels = qMin(6, graphW / 80);
    for(int i = 0; i <= numLabels; i++)
    {
        qint64 t = startTime + (timeSpan * i / numLabels);
        int x = leftMargin + (graphW * i / numLabels);

        QString label;
        if(currentRange == Range24h)
            label = QDateTime::fromSecsSinceEpoch(t).toString("HH:mm");
        else
            label = QDateTime::fromSecsSinceEpoch(t).toString("MM/dd HH:mm");

        painter.drawText(x - 35, topMargin + graphH + 4, 70, 20, Qt::AlignHCenter | Qt::AlignTop, label);
    }

    // Draw the filled area graph
    QLinearGradient gradient(0, topMargin, 0, topMargin + graphH);
    gradient.setColorAt(0.0, QColor(0, 150, 255, 180));
    gradient.setColorAt(1.0, QColor(0, 150, 255, 30));

    QPolygonF fillPoly;
    QPolygonF linePoly;

    for(int i = 0; i < dataRecords.size(); i++)
    {
        float xRatio = (timeSpan > 0) ? (float)(dataRecords[i].timestamp - startTime) / timeSpan : 0;
        float yRatio = (float)dataRecords[i].playerCount / maxVal;

        float x = leftMargin + xRatio * graphW;
        float y = topMargin + graphH - (yRatio * graphH);

        linePoly.append(QPointF(x, y));
    }

    // Build fill polygon (line + bottom edge)
    if(!linePoly.isEmpty())
    {
        fillPoly = linePoly;
        fillPoly.append(QPointF(leftMargin + graphW, topMargin + graphH));
        fillPoly.append(QPointF(leftMargin, topMargin + graphH));

        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawPolygon(fillPoly);

        // Draw line
        painter.setPen(QPen(QColor(0, 150, 255), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(linePoly);
    }

    // Crosshair with horizontal line
    QColor crossColor = labelColor;
    crossColor.setAlpha(80);
    QPoint mousePos = mapFromGlobal(QCursor::pos());
    if(rect().contains(mousePos) && mousePos.x() >= leftMargin && mousePos.x() <= leftMargin + graphW
       && mousePos.y() >= topMargin && mousePos.y() <= topMargin + graphH)
    {
        painter.setPen(QPen(crossColor, 1, Qt::DashLine));
        painter.drawLine(mousePos.x(), topMargin, mousePos.x(), topMargin + graphH);
        painter.drawLine(leftMargin, mousePos.y(), leftMargin + graphW, mousePos.y());
    }

    // Border
    QColor borderColor = pal.color(QPalette::Mid);
    painter.setPen(QPen(borderColor, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(leftMargin, topMargin, graphW, graphH);
}

void HistoryGraphWidget::mouseMoveEvent(QMouseEvent *event)
{
    int leftMargin = 45;
    int rightMargin = 15;
    int graphW = width() - leftMargin - rightMargin;
    int x = event->position().toPoint().x() - leftMargin;

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
    resize(850, 500);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // Stats summary
    statsLabel = new QLabel(this);
    statsLabel->setTextFormat(Qt::RichText);
    QPalette pal = qApp->palette();
    QColor statsBg = pal.color(QPalette::Base);
    QColor statsBorder = pal.color(QPalette::Mid);
    statsLabel->setStyleSheet(
        QString("QLabel { background: %1; border: 1px solid %2; border-radius: 4px; padding: 8px 12px; }")
        .arg(statsBg.name(), statsBorder.name()));
    layout->addWidget(statsLabel);

    // Time range buttons - compact toggle style
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btn24h = new QPushButton("24h", this);
    btn7D = new QPushButton("7D", this);
    btn30D = new QPushButton("30D", this);

    btn24h->setCheckable(true);
    btn7D->setCheckable(true);
    btn30D->setCheckable(true);
    btn24h->setChecked(true);
    btn24h->setFixedWidth(60);
    btn7D->setFixedWidth(60);
    btn30D->setFixedWidth(60);

    QString toggleStyle =
        "QPushButton { border: 1px solid palette(mid); border-radius: 3px; padding: 4px 8px; }"
        "QPushButton:checked { background: #3a6fcc; border-color: #5a8fec; color: white; }";
    btn24h->setStyleSheet(toggleStyle);
    btn7D->setStyleSheet(toggleStyle);
    btn30D->setStyleSheet(toggleStyle);

    btnLayout->addStretch();
    btnLayout->addWidget(btn24h);
    btnLayout->addWidget(btn7D);
    btnLayout->addWidget(btn30D);
    btnLayout->addStretch();

    layout->addLayout(btnLayout);

    // Graph
    graphWidget = new HistoryGraphWidget(this);
    graphWidget->setMinimumHeight(220);
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
    updateStats(records);
}

void PlayerHistoryDialog::updateStats(const QList<PlayerCountRecord> &records)
{
    if(records.isEmpty())
    {
        statsLabel->setText("No player data available");
        return;
    }

    int minPlayers = INT_MAX, maxPlayers = 0;
    qint64 total = 0;

    for(const auto &r : records)
    {
        if(r.playerCount < minPlayers) minPlayers = r.playerCount;
        if(r.playerCount > maxPlayers) maxPlayers = r.playerCount;
        total += r.playerCount;
    }

    int avg = total / records.size();

    // Find peak time
    int peakIdx = 0;
    for(int i = 1; i < records.size(); i++)
    {
        if(records[i].playerCount > records[peakIdx].playerCount)
            peakIdx = i;
    }
    QString peakTime = QDateTime::fromSecsSinceEpoch(records[peakIdx].timestamp).toString("MM/dd HH:mm");

    QString stats = QString(
        "<table cellspacing='8'><tr>"
        "<td><b>Min:</b> %1</td>"
        "<td><b>Max:</b> <span style='color:#0096ff'>%2</span></td>"
        "<td><b>Avg:</b> %3</td>"
        "<td><b>Peak at:</b> %4</td>"
        "<td><b>Samples:</b> %5</td>"
        "</tr></table>")
        .arg(minPlayers == INT_MAX ? 0 : minPlayers)
        .arg(maxPlayers)
        .arg(avg)
        .arg(peakTime)
        .arg(records.size());

    statsLabel->setText(stats);
}
