#ifndef MAPHISTORYDIALOG_H
#define MAPHISTORYDIALOG_H

#include <QDialog>
#include <QTableWidget>

class MapHistoryDialog : public QDialog
{
    Q_OBJECT
public:
    MapHistoryDialog(const QString &serverKey, const QString &serverName, QWidget *parent = nullptr);

private:
    QTableWidget *tableWidget;
};

#endif // MAPHISTORYDIALOG_H
