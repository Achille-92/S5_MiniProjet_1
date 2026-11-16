#include "dialog.h"
#include <winsock2.h>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Dialog w;
    w.show();
    return QApplication::exec();
}
