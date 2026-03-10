#ifndef LATENCYDIALOG_H
#define LATENCYDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QProcess>
#include <QList>
#include "database.h"

class LatencyGraphWidget : public QWidget
{
    Q_OBJECT
public:
    enum TimeRange { Range24h, Range7D, Range30D };

    LatencyGraphWidget(QWidget *parent = nullptr);
    void setData(const QList<LatencyRecord> &records);
    void setTimeRange(TimeRange range);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QList<LatencyRecord> dataRecords;
    TimeRange currentRange;
};

class LatencyDialog : public QDialog
{
    Q_OBJECT
public:
    LatencyDialog(const QString &serverKey, const QString &serverName,
                  const QString &hostAddress, QWidget *parent = nullptr);
    ~LatencyDialog();

private slots:
    void onRange24h();
    void onRange7D();
    void onRange30D();
    void runTraceroute();
    void onTracerouteOutput();
    void onTracerouteFinished(int exitCode, QProcess::ExitStatus status);

private:
    void loadData(LatencyGraphWidget::TimeRange range);
    void updateStats(const QList<LatencyRecord> &records);
    void parseTraceLine(const QString &line);

    QString serverKey;
    QString hostAddress;
    LatencyGraphWidget *graphWidget;
    QPushButton *btn24h;
    QPushButton *btn7D;
    QPushButton *btn30D;
    QPushButton *traceBtn;
    QLabel *statsLabel;
    QTableWidget *traceTable;
    QProcess *traceProcess;
};

#endif // LATENCYDIALOG_H
