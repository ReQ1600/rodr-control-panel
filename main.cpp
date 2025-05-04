#include "rodrcontrolpanel.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    RODRControlPanel w;
    w.show();
    return a.exec();
}
