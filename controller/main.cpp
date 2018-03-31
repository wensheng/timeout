#include "tmcontroller.h"
#include <QApplication>
#pragma execution_character_set("utf-8")

int main(int argc, char *argv[])
{
    //Q_INIT_RESOURCE(timepie);
    QApplication a(argc, argv);
    TMController w;
    w.show();

    return a.exec();
}
