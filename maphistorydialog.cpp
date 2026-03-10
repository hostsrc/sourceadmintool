#include "maphistorydialog.h"
#include "database.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QLabel>
#include <QMap>
#include <QApplication>

static QString formatDuration(qint64 seconds)
{
    if(seconds < 60)
        return QString("%1s").arg(seconds);
    if(seconds < 3600)
        return QString("%1m %2s").arg(seconds / 60).arg(seconds % 60);
    if(seconds < 86400)
        return QString("%1h %2m").arg(seconds / 3600).arg((seconds % 3600) / 60);
    return QString("%1d %2h").arg(seconds / 86400).arg((seconds % 86400) / 3600);
}

MapHistoryDialog::MapHistoryDialog(const QString &serverKey, const QString &serverName, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Map History - %1").arg(serverName));
    setMinimumSize(550, 450);
    resize(650, 500);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // Load data (newest first)
    QList<MapChangeRecord> records = Database::instance()->getMapHistory(serverKey, 200);

    // Stats summary
    QLabel *statsLabel = new QLabel(this);
    statsLabel->setTextFormat(Qt::RichText);
    QPalette pal = qApp->palette();
    QColor statsBg = pal.color(QPalette::Base);
    QColor statsBorder = pal.color(QPalette::Mid);
    statsLabel->setStyleSheet(
        QString("QLabel { background: %1; border: 1px solid %2; border-radius: 4px; padding: 8px 12px; }")
        .arg(statsBg.name(), statsBorder.name()));
    layout->addWidget(statsLabel);

    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(3);
    tableWidget->setHorizontalHeaderLabels({"Map Name", "Changed At", "Duration"});
    tableWidget->verticalHeader()->setVisible(false); // Hide row numbers
    tableWidget->horizontalHeader()->setStretchLastSection(true);
    tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setAlternatingRowColors(true);

    layout->addWidget(tableWidget);

    if(records.isEmpty())
    {
        statsLabel->setText("No map history recorded yet");
        tableWidget->setRowCount(1);
        tableWidget->setSpan(0, 0, 1, 3);
        tableWidget->setItem(0, 0, new QTableWidgetItem("No map history recorded yet"));
        return;
    }

    tableWidget->setRowCount(records.size());

    // Track unique maps and find longest-played map
    QMap<QString, qint64> mapDurations;

    for(int i = 0; i < records.size(); i++)
    {
        const MapChangeRecord &r = records[i];

        // Map name
        tableWidget->setItem(i, 0, new QTableWidgetItem(r.mapName));

        // Timestamp
        QString timeStr = QDateTime::fromSecsSinceEpoch(r.timestamp).toString("yyyy-MM-dd HH:mm");
        tableWidget->setItem(i, 1, new QTableWidgetItem(timeStr));

        // Duration: time from this entry to the next one (records are newest first)
        qint64 duration;
        if(i == 0)
        {
            duration = QDateTime::currentSecsSinceEpoch() - r.timestamp;
            QTableWidgetItem *item = new QTableWidgetItem(formatDuration(duration) + " (current)");
            item->setForeground(QColor(0, 200, 0));
            tableWidget->setItem(i, 2, item);
        }
        else
        {
            duration = records[i - 1].timestamp - r.timestamp;
            tableWidget->setItem(i, 2, new QTableWidgetItem(formatDuration(duration)));
        }

        mapDurations[r.mapName] += duration;
    }

    // Find most played map
    QString mostPlayed;
    qint64 longestTime = 0;
    for(auto it = mapDurations.constBegin(); it != mapDurations.constEnd(); ++it)
    {
        if(it.value() > longestTime)
        {
            longestTime = it.value();
            mostPlayed = it.key();
        }
    }

    QString stats = QString(
        "<table cellspacing='8'><tr>"
        "<td><b>Total changes:</b> %1</td>"
        "<td><b>Unique maps:</b> %2</td>"
        "<td><b>Most played:</b> <span style='color:#0096ff'>%3</span> (%4)</td>"
        "</tr></table>")
        .arg(records.size())
        .arg(mapDurations.size())
        .arg(mostPlayed)
        .arg(formatDuration(longestTime));

    statsLabel->setText(stats);
}
