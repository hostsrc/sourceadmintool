#include "maphistorydialog.h"
#include "database.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QLabel>

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
    setMinimumSize(500, 400);
    resize(550, 450);

    QVBoxLayout *layout = new QVBoxLayout(this);

    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(3);
    tableWidget->setHorizontalHeaderLabels({"Map Name", "Changed At", "Duration"});
    tableWidget->horizontalHeader()->setStretchLastSection(true);
    tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setAlternatingRowColors(true);

    layout->addWidget(tableWidget);

    // Load data (newest first)
    QList<MapChangeRecord> records = Database::instance()->getMapHistory(serverKey, 200);

    if(records.isEmpty())
    {
        tableWidget->setRowCount(1);
        tableWidget->setSpan(0, 0, 1, 3);
        tableWidget->setItem(0, 0, new QTableWidgetItem("No map history recorded yet"));
        return;
    }

    tableWidget->setRowCount(records.size());

    for(int i = 0; i < records.size(); i++)
    {
        const MapChangeRecord &r = records[i];

        // Map name
        tableWidget->setItem(i, 0, new QTableWidgetItem(r.mapName));

        // Timestamp
        QString timeStr = QDateTime::fromSecsSinceEpoch(r.timestamp).toString("yyyy-MM-dd HH:mm:ss");
        tableWidget->setItem(i, 1, new QTableWidgetItem(timeStr));

        // Duration: time from this entry to the next one (records are newest first)
        if(i == 0)
        {
            // Most recent map - show time since change
            qint64 elapsed = QDateTime::currentSecsSinceEpoch() - r.timestamp;
            tableWidget->setItem(i, 2, new QTableWidgetItem(formatDuration(elapsed) + " (current)"));
        }
        else
        {
            qint64 duration = records[i - 1].timestamp - r.timestamp;
            tableWidget->setItem(i, 2, new QTableWidgetItem(formatDuration(duration)));
        }
    }

    // Summary label
    QLabel *summary = new QLabel(QString("Total map changes: %1").arg(records.size()), this);
    layout->addWidget(summary);
}
