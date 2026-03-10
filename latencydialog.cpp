#include "latencydialog.h"

#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QLinearGradient>
#include <QGroupBox>
#include <QApplication>
#include <cmath>

// --- LatencyGraphWidget ---

LatencyGraphWidget::LatencyGraphWidget(QWidget *parent)
    : QWidget(parent), currentRange(Range24h)
{
    setMouseTracking(true);
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void LatencyGraphWidget::setData(const QList<LatencyRecord> &records)
{
    dataRecords = records;
    update();
}

void LatencyGraphWidget::setTimeRange(TimeRange range)
{
    currentRange = range;
}

void LatencyGraphWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int leftMargin = 55;
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
        painter.drawText(rect(), Qt::AlignCenter, "No latency data for this time range");
        return;
    }

    // Find max ping for scaling
    int maxPing = 50; // minimum scale
    for(const auto &r : dataRecords)
    {
        if(r.pingMs < 2000 && r.pingMs > maxPing)
            maxPing = r.pingMs;
    }
    maxPing = qMin(maxPing + 20, 500); // cap at 500ms for display

    // Round maxPing to a nice number for Y-axis
    int niceMax;
    if(maxPing <= 50) niceMax = 50;
    else if(maxPing <= 100) niceMax = 100;
    else if(maxPing <= 200) niceMax = 200;
    else if(maxPing <= 300) niceMax = 300;
    else niceMax = 500;
    maxPing = niceMax;

    // Background
    painter.fillRect(QRect(leftMargin, topMargin, graphW, graphH), bgColor);

    // Grid lines and Y-axis labels
    painter.setPen(QPen(gridColor, 1));
    int gridLines = 4;
    for(int i = 0; i <= gridLines; i++)
    {
        int y = topMargin + (graphH * i / gridLines);
        painter.drawLine(leftMargin, y, leftMargin + graphW, y);

        int val = maxPing - (maxPing * i / gridLines);
        painter.setPen(labelColor);
        painter.drawText(0, y - 8, leftMargin - 8, 16, Qt::AlignRight | Qt::AlignVCenter,
                         QString("%1 ms").arg(val));
        painter.setPen(QPen(gridColor, 1));
    }

    // X-axis labels
    qint64 startTime = dataRecords.first().timestamp;
    qint64 endTime = dataRecords.last().timestamp;
    qint64 timeSpan = endTime - startTime;

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

    // Draw filled area under the line
    for(int i = 1; i < dataRecords.size(); i++)
    {
        float x1Ratio = (timeSpan > 0) ? (float)(dataRecords[i-1].timestamp - startTime) / timeSpan : 0;
        float x2Ratio = (timeSpan > 0) ? (float)(dataRecords[i].timestamp - startTime) / timeSpan : 0;

        int ping1 = qMin(dataRecords[i-1].pingMs, maxPing);
        int ping2 = qMin(dataRecords[i].pingMs, maxPing);

        float y1Ratio = (float)ping1 / maxPing;
        float y2Ratio = (float)ping2 / maxPing;

        float x1 = leftMargin + x1Ratio * graphW;
        float x2 = leftMargin + x2Ratio * graphW;
        float y1 = topMargin + graphH - (y1Ratio * graphH);
        float y2 = topMargin + graphH - (y2Ratio * graphH);
        float yBase = topMargin + graphH;

        // Color based on latency: green < 50ms, yellow 50-150ms, red > 150ms
        QColor lineColor;
        int avgPing = (ping1 + ping2) / 2;
        if(avgPing < 50)
            lineColor = QColor(0, 200, 0);
        else if(avgPing < 150)
            lineColor = QColor(255, 200, 0);
        else
            lineColor = QColor(255, 60, 60);

        if(dataRecords[i].pingMs == 2000)
            lineColor = QColor(255, 0, 0);

        // Semi-transparent fill under the line
        QColor fillColor = lineColor;
        fillColor.setAlpha(40);
        QPolygonF fill;
        fill << QPointF(x1, yBase) << QPointF(x1, y1) << QPointF(x2, y2) << QPointF(x2, yBase);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fillColor);
        painter.drawPolygon(fill);

        // Draw the line itself
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(lineColor, 2));
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
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

void LatencyGraphWidget::mouseMoveEvent(QMouseEvent *event)
{
    int leftMargin = 55;
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
    if(timeSpan <= 0) { QWidget::mouseMoveEvent(event); return; }

    qint64 targetTime = startTime + (qint64)((double)x / graphW * timeSpan);

    int nearest = 0;
    qint64 minDist = qAbs(dataRecords[0].timestamp - targetTime);
    for(int i = 1; i < dataRecords.size(); i++)
    {
        qint64 dist = qAbs(dataRecords[i].timestamp - targetTime);
        if(dist < minDist) { minDist = dist; nearest = i; }
    }

    const LatencyRecord &r = dataRecords[nearest];
    QString timeStr = QDateTime::fromSecsSinceEpoch(r.timestamp).toString("yyyy-MM-dd HH:mm:ss");
    QString pingStr = (r.pingMs >= 2000) ? "Timeout" : QString("%1ms").arg(r.pingMs);
    QToolTip::showText(event->globalPosition().toPoint(),
        QString("Ping: %1\nTime: %2").arg(pingStr, timeStr));
    update();

    QWidget::mouseMoveEvent(event);
}

// --- LatencyDialog ---

LatencyDialog::LatencyDialog(const QString &serverKey, const QString &serverName,
                             const QString &hostAddress, QWidget *parent)
    : QDialog(parent), serverKey(serverKey), hostAddress(hostAddress), traceProcess(nullptr)
{
    setWindowTitle(QString("Latency Stats - %1").arg(serverName));
    setMinimumSize(750, 550);
    resize(850, 650);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // Stats summary in a styled frame
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

    // Latency graph
    graphWidget = new LatencyGraphWidget(this);
    graphWidget->setMinimumHeight(220);
    layout->addWidget(graphWidget, 1);

    // Traceroute section
    QGroupBox *traceGroup = new QGroupBox("Traceroute", this);
    QVBoxLayout *traceLayout = new QVBoxLayout(traceGroup);
    traceLayout->setSpacing(6);

    QHBoxLayout *traceBtnLayout = new QHBoxLayout();
    traceBtn = new QPushButton("Run Traceroute", this);
    traceBtn->setFixedWidth(140);
    traceBtnLayout->addWidget(traceBtn);
    traceBtnLayout->addStretch();
    traceLayout->addLayout(traceBtnLayout);

    traceTable = new QTableWidget(this);
    traceTable->setColumnCount(5);
    traceTable->setHorizontalHeaderLabels({"Hop", "IP Address", "Loss %", "Avg (ms)", "Best / Worst (ms)"});
    traceTable->verticalHeader()->setVisible(false); // Hide row numbers
    traceTable->horizontalHeader()->setStretchLastSection(true);
    traceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    traceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    traceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    traceTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    traceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    traceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    traceTable->setAlternatingRowColors(true);
    traceTable->setMaximumHeight(220);
    traceLayout->addWidget(traceTable);

    layout->addWidget(traceGroup);

    connect(btn24h, &QPushButton::clicked, this, &LatencyDialog::onRange24h);
    connect(btn7D, &QPushButton::clicked, this, &LatencyDialog::onRange7D);
    connect(btn30D, &QPushButton::clicked, this, &LatencyDialog::onRange30D);
    connect(traceBtn, &QPushButton::clicked, this, &LatencyDialog::runTraceroute);

    loadData(LatencyGraphWidget::Range24h);
}

void LatencyDialog::onRange24h()
{
    btn24h->setChecked(true); btn7D->setChecked(false); btn30D->setChecked(false);
    loadData(LatencyGraphWidget::Range24h);
}

void LatencyDialog::onRange7D()
{
    btn24h->setChecked(false); btn7D->setChecked(true); btn30D->setChecked(false);
    loadData(LatencyGraphWidget::Range7D);
}

void LatencyDialog::onRange30D()
{
    btn24h->setChecked(false); btn7D->setChecked(false); btn30D->setChecked(true);
    loadData(LatencyGraphWidget::Range30D);
}

void LatencyDialog::loadData(LatencyGraphWidget::TimeRange range)
{
    qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 startTime;

    switch(range)
    {
        case LatencyGraphWidget::Range24h: startTime = now - (24 * 3600); break;
        case LatencyGraphWidget::Range7D:  startTime = now - (7 * 24 * 3600); break;
        case LatencyGraphWidget::Range30D: startTime = now - (30 * 24 * 3600); break;
    }

    QList<LatencyRecord> records = Database::instance()->getLatencySamples(serverKey, startTime, now);
    graphWidget->setTimeRange(range);
    graphWidget->setData(records);
    updateStats(records);
}

void LatencyDialog::updateStats(const QList<LatencyRecord> &records)
{
    if(records.isEmpty())
    {
        statsLabel->setText("No latency data available");
        return;
    }

    int minPing = INT_MAX, maxPing = 0;
    qint64 totalPing = 0;
    int validCount = 0;
    int timeoutCount = 0;
    int spikeCount = 0;

    for(const auto &r : records)
    {
        if(r.pingMs >= 2000)
        {
            timeoutCount++;
            continue;
        }
        if(r.pingMs < minPing) minPing = r.pingMs;
        if(r.pingMs > maxPing) maxPing = r.pingMs;
        totalPing += r.pingMs;
        validCount++;
    }

    int avgPing = validCount > 0 ? totalPing / validCount : 0;

    // Calculate jitter (standard deviation)
    double variance = 0;
    for(const auto &r : records)
    {
        if(r.pingMs >= 2000) continue;
        double diff = r.pingMs - avgPing;
        variance += diff * diff;
    }
    double jitter = validCount > 1 ? sqrt(variance / (validCount - 1)) : 0;

    // Count spikes (>2x average)
    for(const auto &r : records)
    {
        if(r.pingMs < 2000 && r.pingMs > avgPing * 2 && r.pingMs > 50)
            spikeCount++;
    }

    double lossPercent = records.size() > 0 ? (double)timeoutCount / records.size() * 100.0 : 0;

    // Color-code the avg ping
    QString avgColor = avgPing < 50 ? "#00c800" : avgPing < 150 ? "#ffc800" : "#ff3c3c";
    QString lossColor = lossPercent < 1.0 ? "#00c800" : lossPercent < 5.0 ? "#ffc800" : "#ff3c3c";

    QString stats = QString(
        "<table cellspacing='8'><tr>"
        "<td><b>Min:</b> %1 ms</td>"
        "<td><b>Max:</b> %2 ms</td>"
        "<td><b>Avg:</b> <span style='color:%8'>%3 ms</span></td>"
        "<td><b>Jitter:</b> %4 ms</td>"
        "<td><b>Loss:</b> <span style='color:%9'>%5%</span></td>"
        "<td><b>Spikes:</b> %6</td>"
        "<td><b>Samples:</b> %7</td>"
        "</tr></table>")
        .arg(minPing == INT_MAX ? 0 : minPing)
        .arg(maxPing)
        .arg(avgPing)
        .arg(QString::number(jitter, 'f', 1))
        .arg(QString::number(lossPercent, 'f', 1))
        .arg(spikeCount)
        .arg(records.size())
        .arg(avgColor)
        .arg(lossColor);

    statsLabel->setText(stats);
}

LatencyDialog::~LatencyDialog()
{
    if(traceProcess)
    {
        traceProcess->disconnect();
        traceProcess->kill();
        traceProcess->waitForFinished(1000);
        delete traceProcess;
        traceProcess = nullptr;
    }
}

void LatencyDialog::runTraceroute()
{
    if(traceProcess)
    {
        traceProcess->disconnect();
        traceProcess->kill();
        traceProcess->waitForFinished(1000);
        delete traceProcess;
        traceProcess = nullptr;
    }

    traceBtn->setEnabled(false);
    traceBtn->setText("Running traceroute...");
    traceTable->setRowCount(0);

    traceProcess = new QProcess(this);

    // Stream output line-by-line as hops come in
    connect(traceProcess, &QProcess::readyReadStandardOutput,
            this, &LatencyDialog::onTracerouteOutput);
    connect(traceProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LatencyDialog::onTracerouteFinished);

    // Extract IP from hostAddress (remove port if present)
    QString ip = hostAddress;
    if(ip.contains(":"))
        ip = ip.split(":").first();

#ifdef Q_OS_WIN
    traceProcess->start("tracert", {"-d", "-w", "1000", ip});
#else
    // -n: no DNS, -w 1: 1s timeout per probe, -q 1: single probe per hop, -m 20: max 20 hops
    traceProcess->start("traceroute", {"-n", "-w", "1", "-q", "1", "-m", "20", ip});
#endif
}

void LatencyDialog::onTracerouteOutput()
{
    if(!traceProcess)
        return;

    while(traceProcess->canReadLine())
    {
        QString line = traceProcess->readLine().trimmed();
        if(!line.isEmpty())
            parseTraceLine(line);
    }
}

void LatencyDialog::onTracerouteFinished(int exitCode, QProcess::ExitStatus)
{
    Q_UNUSED(exitCode);

    // Read any remaining output
    if(traceProcess)
    {
        QString remaining = traceProcess->readAllStandardOutput().trimmed();
        QStringList lines = remaining.split('\n', Qt::SkipEmptyParts);
        for(const QString &line : lines)
            parseTraceLine(line.trimmed());
    }

    traceBtn->setEnabled(true);
    traceBtn->setText("Run Traceroute");

    if(traceProcess)
    {
        traceProcess->deleteLater();
        traceProcess = nullptr;
    }
}

void LatencyDialog::parseTraceLine(const QString &line)
{
    // Skip header lines
    if(line.startsWith("traceroute") || line.startsWith("Tracing") || line.isEmpty())
        return;

    // traceroute format: " 1  10.0.0.1  1.234 ms"  or  " 2  * * *"
    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if(parts.size() < 2)
        return;

    // First part should be the hop number
    bool isHop;
    parts[0].toInt(&isHop);
    if(!isHop)
        return;

    int row = traceTable->rowCount();
    traceTable->insertRow(row);

    traceTable->setItem(row, 0, new QTableWidgetItem(parts[0])); // Hop

    // Find IP address (first token with dots that isn't "ms")
    QString ip = "*";
    for(int i = 1; i < parts.size(); i++)
    {
        if(parts[i].contains('.') && parts[i] != "ms" && !parts[i].endsWith("ms"))
        {
            ip = parts[i];
            break;
        }
    }
    traceTable->setItem(row, 1, new QTableWidgetItem(ip));
    traceTable->setItem(row, 2, new QTableWidgetItem("--")); // Loss N/A for traceroute

    // Parse RTT values (numbers before "ms")
    QList<double> rtts;
    for(int i = 1; i < parts.size(); i++)
    {
        if(parts[i] == "*") continue;
        bool ok;
        double val = parts[i].toDouble(&ok);
        if(ok && val > 0 && i + 1 < parts.size() && parts[i + 1] == "ms")
            rtts.append(val);
    }

    if(!rtts.isEmpty())
    {
        double total = 0, best = 99999, worst = 0;
        for(double v : rtts)
        {
            total += v;
            if(v < best) best = v;
            if(v > worst) worst = v;
        }
        traceTable->setItem(row, 3, new QTableWidgetItem(QString("%1 ms").arg(
            QString::number(total / rtts.size(), 'f', 1))));
        traceTable->setItem(row, 4, new QTableWidgetItem(QString("%1 / %2 ms").arg(
            QString::number(best, 'f', 1), QString::number(worst, 'f', 1))));

        // Color code high latency
        double avg = total / rtts.size();
        QColor color = avg < 50 ? QColor(0, 200, 0) : avg < 150 ? QColor(255, 200, 0) : QColor(255, 60, 60);
        traceTable->item(row, 3)->setForeground(color);
    }
    else
    {
        traceTable->setItem(row, 3, new QTableWidgetItem("*"));
        traceTable->setItem(row, 4, new QTableWidgetItem("*"));
        traceTable->item(row, 3)->setForeground(QColor(255, 60, 60));
    }

    // Auto-scroll to latest hop
    traceTable->scrollToBottom();
}
