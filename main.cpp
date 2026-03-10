#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>

QPalette defaultPalette;
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("SourceAdminTool");
    a.setStyle(QStyleFactory::create("Fusion"));

    defaultPalette = a.palette();

    defaultPalette.setColor(QPalette::Link, QColor(0, 100, 200));
    defaultPalette.setColor(QPalette::LinkVisited, QColor(0, 80, 160));

    a.setPalette(defaultPalette);

    MainWindow w;

    return a.exec();
}
