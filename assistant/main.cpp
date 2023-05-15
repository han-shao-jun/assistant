#include "widget.h"
#include "mainwin.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWin w;
    w.show();

    return QApplication::exec();
}

