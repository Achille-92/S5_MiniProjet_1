#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include "mythread.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class Dialog;
}
QT_END_NAMESPACE

class Dialog : public QDialog
{
    Q_OBJECT
private:
    Mythread *pUDP;
public:
    explicit Dialog(QWidget *parent = nullptr);
    ~Dialog() override;
signals:
    void boutANGLE(int angle);
    void boutDIST(void);
    void boutSCAN(int debut,int fin,int pas);
    void IPchange(QString ip);
    void PORTchange(int port);

private slots:
    void on_bpDIST_clicked();

    void on_bpANGLE_clicked();

    void on_bpSCAN_clicked();

    void on_IPentree_editingFinished();

    void on_Portentree_editingFinished();

    void on_spANGLE_valueChanged(int arg1);

    void on_spDeb_valueChanged(int arg1);

    void on_spFin_valueChanged(int arg1);

    void on_spPas_valueChanged(int arg1);

private:
    Ui::Dialog *ui;
};
#endif // DIALOG_H
