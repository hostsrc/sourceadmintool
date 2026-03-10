#ifndef PLAYERHISTORYDIALOG_H
#define PLAYERHISTORYDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QList>
#include "database.h"

class HistoryGraphWidget : public QWidget
{
    Q_OBJECT
public:
    enum TimeRange { Range24h, Range7D, Range30D };

    HistoryGraphWidget(QWidget *parent = nullptr);
    void setData(const QList<PlayerCountRecord> &records, int maxPlayers);
    void setTimeRange(TimeRange range);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QList<PlayerCountRecord> dataRecords;
    int maxVal;
    TimeRange currentRange;
};

class PlayerHistoryDialog : public QDialog
{
    Q_OBJECT
public:
    PlayerHistoryDialog(const QString &serverKey, const QString &serverName, int maxPlayers, QWidget *parent = nullptr);

private slots:
    void onRange24h();
    void onRange7D();
    void onRange30D();

private:
    void loadData(HistoryGraphWidget::TimeRange range);
    void updateStats(const QList<PlayerCountRecord> &records);

    QString serverKey;
    int maxPlayers;
    HistoryGraphWidget *graphWidget;
    QLabel *statsLabel;
    QPushButton *btn24h;
    QPushButton *btn7D;
    QPushButton *btn30D;
};

#endif // PLAYERHISTORYDIALOG_H
