#include "dialog.h"
#include "ui_dialog.h"
#include <QThread>
int angle=0,debut=0,fin=0,pas=0;
Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
{printf("6");
    ui->setupUi(this);

    pUDP = new Mythread;

    QObject::connect(this,SIGNAL(boutANGLE(int)),pUDP,SLOT(bANGLE(int)));
    QObject::connect(this,SIGNAL(boutDIST()),pUDP,SLOT(bDIST()));
    QObject::connect(this,SIGNAL(boutSCAN(int,int,int)),pUDP,SLOT(bSCAN(int,int,int)));
    QObject::connect(this,SIGNAL(IPchange(QString)),pUDP,SLOT(ipchange(QString)));
    QObject::connect(this,SIGNAL(PORTchange(int)),pUDP,SLOT(portchange(int)));
    printf("5");
    pUDP->start();
}

Dialog::~Dialog()
{
    delete ui;
}

void Dialog::on_bpDIST_clicked()
{printf("1");
emit boutDIST();
    printf("7");
}


void Dialog::on_bpANGLE_clicked()
{
    emit boutANGLE(angle);
}


void Dialog::on_bpSCAN_clicked()
{
    emit boutSCAN(debut,fin,pas);
}


void Dialog::on_IPentree_editingFinished()
{

}


void Dialog::on_Portentree_editingFinished()
{

}


void Dialog::on_spANGLE_valueChanged(int arg1)
{
    angle=arg1;
}


void Dialog::on_spDeb_valueChanged(int arg1)
{
    debut=arg1;
}


void Dialog::on_spFin_valueChanged(int arg1)
{
    fin=arg1;
}


void Dialog::on_spPas_valueChanged(int arg1)
{
    pas=arg1;
}

