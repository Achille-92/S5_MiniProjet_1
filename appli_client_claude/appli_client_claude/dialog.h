#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QThread>
#include <QVector>

#include "scanpoint.h"

struct UdpTarget;
class UdpWorker;

QT_BEGIN_NAMESPACE
namespace Ui {
class Dialog;
}
QT_END_NAMESPACE

class Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = nullptr);
    ~Dialog() override;

private slots:
    // Boutons
    void onAngleClicked();
    void onDistClicked();
    void onScanClicked();
    void onExportClicked();

    // Résultats renvoyés par le thread réseau
    void onAngleDone(bool ok, const QString &message);
    void onDistanceDone(bool ok, bool hasEcho, double cm, const QString &message);
    void onScanDone(bool ok, const QVector<ScanPoint> &points, const QString &message);
    void appendLog(const QString &line);

private:
    bool readTarget(UdpTarget *target);
    bool validateAddress(const QString &host);
    void setBusy(bool busy, const QString &message = QString());
    void showError(const QString &message);
    void showScan(const QVector<ScanPoint> &points);

    Ui::Dialog *ui;
    QThread m_thread;                // thread réseau : l'interface ne se fige pas
    UdpWorker *m_worker = nullptr;   // vit dans m_thread
    QVector<ScanPoint> m_lastScan;
};

#endif // DIALOG_H
