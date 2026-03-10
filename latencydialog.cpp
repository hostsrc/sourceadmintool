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
    int margin = 40;
    int graphW = w - margin * 2;
    int graphH = h - margin * 2;

    if(dataRecords.isEmpty() || graphW <= 0 || graphH <= 0)
    {
        painter.setPen(Qt::gray);
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

    // Background
    painter.fillRect(QRect(margin, margin, graphW, graphH), QColor(30, 30, 34));

    // Grid lines
    painter.setPen(QPen(QColor(60, 60, 65), 1));
    int gridLines = 4;
    for(int i = 0; i <= gridLines; i++)
    {
        int y = margin + (graphH * i / gridLines);
        painter.drawLine(margin, y, margin + graphW, y);

        int val = maxPing - (maxPing * i / gridLines);
        painter.setPen(Qt::gray);
        painter.drawText(0, y - 8, margin - 5, 16, Qt::AlignRight | Qt::AlignVCenter, QString("%1ms").arg(val));
        painter.setPen(QPen(QColor(60, 60, 65), 1));
    }

    // X-axis labels
    qint64 startTime = dataRecords.first().timestamp;
    qint64 endTime = dataRecords.last().timestamp;
    qint64 timeSpan = endTime - startTime;

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

    // Draw latency line with color coding
    for(int i = 1; i < dataRecords.size(); i++)
    {
        float x1Ratio = (timeSpan > 0) ? (float)(dataRecords[i-1].timestamp - startTime) / timeSpan : 0;
        float x2Ratio = (timeSpan > 0) ? (float)(dataRecords[i].timestamp - startTime) / timeSpan : 0;

        int ping1 = qMin(dataRecords[i-1].pingMs, maxPing);
        int ping2 = qMin(dataRecords[i].pingMs, maxPing);

        float y1Ratio = (float)ping1 / maxPing;
        float y2Ratio = (float)ping2 / maxPing;

        float x1 = margin + x1Ratio * graphW;
        float x2 = margin + x2Ratio * graphW;
        float y1 = margin + graphH - (y1Ratio * graphH);
        float y2 = margin + graphH - (y2Ratio * graphH);

        // Color based on latency: green < 50ms, yellow 50-150ms, red > 150ms
        QColor lineColor;
        int avgPing = (ping1 + ping2) / 2;
        if(avgPing < 50)
            lineColor = QColor(0, 200, 0);
        else if(avgPing < 150)
            lineColor = QColor(255, 200, 0);
        else
            lineColor = QColor(255, 60, 60);

        // Highlight spikes (>2x average)
        if(dataRecords[i].pingMs == 2000)
            lineColor = QColor(255, 0, 0);

        painter.setPen(QPen(lineColor, 2));
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    // Crosshair
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

void LatencyGraphWidget::mouseMoveEvent(QMouseEvent *event)
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
    resize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(this);

    // Stats summary
    statsLabel = new QLabel(this);
    statsLabel->setTextFormat(Qt::RichText);
    layout->addWidget(statsLabel);

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

    // Latency graph
    graphWidget = new LatencyGraphWidget(this);
    layout->addWidget(graphWidget, 1);

    // Traceroute section
    QGroupBox *traceGroup = new QGroupBox("Traceroute / MTR", this);
    QVBoxLayout *traceLayout = new QVBoxLayout(traceGroup);

    QHBoxLayout *traceBtnLayout = new QHBoxLayout();
    traceBtn = new QPushButton("Run Traceroute", this);
    traceBtnLayout->addWidget(traceBtn);
    traceBtnLayout->addStretch();
    traceLayout->addLayout(traceBtnLayout);

    traceTable = new QTableWidget(this);
    traceTable->setColumnCount(5);
    traceTable->setHorizontalHeaderLabels({"Hop", "IP Address", "Loss %", "Avg (ms)", "Best/Worst (ms)"});
    traceTable->horizontalHeader()->setStretchLastSection(true);
    traceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    traceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    traceTable->setAlternatingRowColors(true);
    traceTable->setMaximumHeight(200);
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

    QString stats = QString(
        "<b>Min:</b> %1ms &nbsp; <b>Max:</b> %2ms &nbsp; <b>Avg:</b> %3ms &nbsp; "
        "<b>Jitter:</b> %4ms &nbsp; <b>Loss:</b> %5% &nbsp; "
        "<b>Spikes:</b> %6 &nbsp; <b>Samples:</b> %7")
        .arg(minPing == INT_MAX ? 0 : minPing)
        .arg(maxPing)
        .arg(avgPing)
        .arg(QString::number(jitter, 'f', 1))
        .arg(QString::number(lossPercent, 'f', 1))
        .arg(spikeCount)
        .arg(records.size());

    statsLabel->setText(stats);
}

void LatencyDialog::runTraceroute()
{
    if(traceProcess)
    {
        traceProcess->kill();
        traceProcess->deleteLater();
    }

    traceBtn->setEnabled(false);
    traceBtn->setText("Running traceroute...");
    traceTable->setRowCount(0);

    traceProcess = new QProcess(this);
    connect(traceProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LatencyDialog::onTracerouteFinished);

    // Extract IP from hostAddress (remove port if present)
    QString ip = hostAddress;
    if(ip.contains(":"))
        ip = ip.split(":").first();

#ifdef Q_OS_WIN
    traceProcess->start("tracert", {"-d", "-w", "2000", ip});
#else
    // Try mtr first (more detailed), fallback to traceroute
    if(QProcess::execute("which", {"mtr"}) == 0)
    {
        traceProcess->start("mtr", {"--report", "--report-cycles", "5", "--no-dns", ip});
    }
    else
    {
        traceProcess->start("traceroute", {"-n", "-w", "2", "-q", "3", ip});
    }
#endif
}

void LatencyDialog::onTracerouteFinished(int exitCode, QProcess::ExitStatus)
{
    Q_UNUSED(exitCode);

    traceBtn->setEnabled(true);
    traceBtn->setText("Run Traceroute");

    if(!traceProcess)
        return;

    QString output = traceProcess->readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    traceTable->setRowCount(0);

    bool isMtr = output.contains("Loss%") || output.contains("Snt");

    for(const QString &line : lines)
    {
        QString trimmed = line.trimmed();
        if(trimmed.isEmpty()) continue;

        // Skip header lines
        if(trimmed.startsWith("Start:") || trimmed.startsWith("HOST:") ||
           trimmed.startsWith("traceroute") || trimmed.startsWith("Tracing"))
            continue;

        int row = traceTable->rowCount();
        traceTable->insertRow(row);

        if(isMtr)
        {
            // MTR format: |-- IP Loss% Snt Last Avg Best Wrst StDev
            QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if(parts.size() >= 8)
            {
                // Remove |-- prefix
                QString hopStr = parts[0].replace("|--", "").replace(".", "");
                traceTable->setItem(row, 0, new QTableWidgetItem(hopStr));
                traceTable->setItem(row, 1, new QTableWidgetItem(parts[1])); // IP
                traceTable->setItem(row, 2, new QTableWidgetItem(parts[2])); // Loss%
                traceTable->setItem(row, 3, new QTableWidgetItem(parts[5])); // Avg
                traceTable->setItem(row, 4, new QTableWidgetItem(QString("%1 / %2").arg(parts[6], parts[7]))); // Best/Worst

                // Color code loss
                double loss = parts[2].replace("%", "").toDouble();
                if(loss > 0)
                {
                    QColor lossColor = loss > 5 ? QColor(255, 60, 60) : QColor(255, 200, 0);
                    traceTable->item(row, 2)->setForeground(lossColor);
                }
            }
        }
        else
        {
            // traceroute/tracert format: HOP IP time1 time2 time3
            QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if(parts.size() >= 2)
            {
                traceTable->setItem(row, 0, new QTableWidgetItem(parts[0])); // Hop

                // Find IP address
                QString ip = "*";
                for(const QString &p : parts)
                {
                    if(p.contains('.') && !p.contains("ms"))
                    {
                        ip = p;
                        break;
                    }
                }
                traceTable->setItem(row, 1, new QTableWidgetItem(ip));
                traceTable->setItem(row, 2, new QTableWidgetItem("N/A")); // Loss not available

                // Parse RTT values
                QStringList rtts;
                for(const QString &p : parts)
                {
                    bool ok;
                    double val = p.toDouble(&ok);
                    if(ok && val > 0)
                        rtts.append(QString::number(val, 'f', 1));
                }

                if(!rtts.isEmpty())
                {
                    double total = 0;
                    double best = 99999, worst = 0;
                    for(const QString &r : rtts)
                    {
                        double v = r.toDouble();
                        total += v;
                        if(v < best) best = v;
                        if(v > worst) worst = v;
                    }
                    traceTable->setItem(row, 3, new QTableWidgetItem(QString::number(total / rtts.size(), 'f', 1)));
                    traceTable->setItem(row, 4, new QTableWidgetItem(QString("%1 / %2").arg(
                        QString::number(best, 'f', 1), QString::number(worst, 'f', 1))));
                }
                else
                {
                    traceTable->setItem(row, 3, new QTableWidgetItem("*"));
                    traceTable->setItem(row, 4, new QTableWidgetItem("*"));
                }
            }
        }
    }

    traceProcess->deleteLater();
    traceProcess = nullptr;
}
