#include "dialog.h"
#include "ui_dialog.h"

#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QMetaObject>
#include <QRegularExpression>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTime>
#include <utility>

#include "udpworker.h"

extern "C" {
#include "radar_proto.h"
}

namespace {

// Le servo met du temps à se déplacer entre deux mesures : on laisse ce temps
// à la carte avant de considérer qu'un SCAN est perdu.
constexpr int kScanMsPerPoint = 500;

const char *const kOkStyle = "color: #2e7d32;";
const char *const kErrorStyle = "color: #d32f2f; font-weight: bold;";

enum TableColumn { ColumnAngle = 0, ColumnDistance, ColumnCount };

} // namespace

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    qRegisterMetaType<QVector<ScanPoint>>("QVector<ScanPoint>");

    // Tableau angle / distance
    ui->tableScan->setColumnCount(ColumnCount);
    ui->tableScan->setHorizontalHeaderLabels({tr("Angle (°)"), tr("Distance (cm)")});
    ui->tableScan->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableScan->verticalHeader()->hide();
    ui->tableScan->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableScan->setAlternatingRowColors(true);
    ui->teLog->setMaximumBlockCount(500);

    // Thread réseau : tous les échanges UDP s'y déroulent.
    m_worker = new UdpWorker;
    m_worker->moveToThread(&m_thread);
    connect(m_worker, &UdpWorker::angleDone, this, &Dialog::onAngleDone);
    connect(m_worker, &UdpWorker::distanceDone, this, &Dialog::onDistanceDone);
    connect(m_worker, &UdpWorker::scanDone, this, &Dialog::onScanDone);
    connect(m_worker, &UdpWorker::logMessage, this, &Dialog::appendLog);
    m_thread.start();

    connect(ui->btnAngle, &QPushButton::clicked, this, &Dialog::onAngleClicked);
    connect(ui->btnDist, &QPushButton::clicked, this, &Dialog::onDistClicked);
    connect(ui->btnScan, &QPushButton::clicked, this, &Dialog::onScanClicked);
    connect(ui->btnExport, &QPushButton::clicked, this, &Dialog::onExportClicked);
}

Dialog::~Dialog()
{
    // Fermeture propre : on interrompt l'attente en cours, on arrête le thread,
    // puis on détruit le worker, ce qui ferme la socket.
    m_worker->abort();
    m_thread.quit();
    m_thread.wait();
    delete m_worker;
    delete ui;
}

// ---------------------------------------------------------------------------
// Saisies
// ---------------------------------------------------------------------------

bool Dialog::validateAddress(const QString &host)
{
    static const QRegularExpression numeric(QStringLiteral("^[0-9.]+$"));
    static const QRegularExpression ipv4(
        QStringLiteral("^(25[0-5]|2[0-4]\\d|1?\\d?\\d)(\\.(25[0-5]|2[0-4]\\d|1?\\d?\\d)){3}$"));
    static const QRegularExpression hostname(
        QStringLiteral("^[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?$"));

    if (host.isEmpty()) {
        showError(tr("Saisissez l'adresse WAN du routeur (par exemple 192.168.1.1)."));
        return false;
    }
    if (numeric.match(host).hasMatch()) {
        if (!ipv4.match(host).hasMatch()) {
            showError(tr("Adresse IPv4 invalide : « %1 » (attendu : quatre nombres de 0 à 255 séparés par des points).")
                          .arg(host));
            return false;
        }
        return true;
    }
    if (!hostname.match(host).hasMatch()) {
        showError(tr("Adresse invalide : « %1 ». Utilisez une adresse IPv4 ou un nom d'hôte.").arg(host));
        return false;
    }
    return true;
}

// Lit et valide l'adresse, le port, le délai et le nombre d'essais.
bool Dialog::readTarget(UdpTarget *target)
{
    const QString host = ui->leAddress->text().trimmed();
    if (!validateAddress(host)) {
        ui->leAddress->setFocus();
        return false;
    }
    target->host = host;
    target->port = static_cast<quint16>(ui->sbPort->value());   // borné à 1..65535 par le spinbox
    target->timeoutMs = ui->sbTimeout->value();
    target->tries = ui->sbTries->value();
    return true;
}

// ---------------------------------------------------------------------------
// Boutons
// ---------------------------------------------------------------------------

void Dialog::onAngleClicked()
{
    UdpTarget target;
    if (!readTarget(&target)) {
        return;
    }
    const int angle = ui->sbAngle->value();
    setBusy(true, tr("Envoi de ANGLE %1…").arg(angle));
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, target, angle] {
        worker->sendAngle(target, angle);
    });
}

void Dialog::onDistClicked()
{
    UdpTarget target;
    if (!readTarget(&target)) {
        return;
    }
    setBusy(true, tr("Mesure de la distance…"));
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, target] {
        worker->measureDistance(target);
    });
}

void Dialog::onScanClicked()
{
    UdpTarget target;
    if (!readTarget(&target)) {
        return;
    }
    const int start = ui->sbStart->value();
    const int end = ui->sbEnd->value();
    const int step = ui->sbStep->value();

    if (start > end) {
        showError(tr("Le début (%1°) doit être inférieur ou égal à la fin (%2°).").arg(start).arg(end));
        return;
    }
    const int points = proto_scan_point_count(start, end, step);
    if (points == 0) {
        showError(tr("Balayage trop long : limité à %1 mesures pour que la réponse tienne dans un seul "
                     "datagramme. Augmentez le pas ou réduisez la plage.")
                      .arg(PROTO_SCAN_MAX_POINTS));
        return;
    }

    // Le balayage dure plus longtemps qu'une simple commande : on allonge l'attente.
    target.timeoutMs += points * kScanMsPerPoint;
    setBusy(true, tr("Balayage de %1° à %2° (%3 mesures)…").arg(start).arg(end).arg(points));
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, target, start, end, step] {
        worker->scan(target, start, end, step);
    });
}

void Dialog::onExportClicked()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Exporter le balayage"),
                                                      QStringLiteral("balayage.csv"),
                                                      tr("Fichiers CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(tr("Impossible d'écrire « %1 » : %2").arg(path, file.errorString()));
        return;
    }
    QTextStream out(&file);
    out << "angle_deg,distance_cm\n";
    for (const ScanPoint &p : std::as_const(m_lastScan)) {
        out << p.angle << ',' << (p.hasEcho ? QString::number(p.cm, 'f', 1) : QString()) << '\n';
    }
    out.flush();
    if (out.status() != QTextStream::Ok) {
        showError(tr("Erreur pendant l'écriture de « %1 ».").arg(path));
        return;
    }
    ui->lbStatus->setStyleSheet(kOkStyle);
    ui->lbStatus->setText(tr("%1 mesure(s) exportée(s) dans « %2 ».").arg(m_lastScan.size()).arg(path));
}

// ---------------------------------------------------------------------------
// Résultats venant du thread réseau
// ---------------------------------------------------------------------------

void Dialog::onAngleDone(bool ok, const QString &message)
{
    setBusy(false);
    if (ok) {
        ui->lbStatus->setStyleSheet(kOkStyle);
        ui->lbStatus->setText(message);
    } else {
        showError(message);
    }
}

void Dialog::onDistanceDone(bool ok, bool hasEcho, double cm, const QString &message)
{
    setBusy(false);
    if (!ok) {
        ui->lbDist->setText(tr("Distance : —"));
        showError(message);
        return;
    }
    ui->lbDist->setText(hasEcho ? tr("Distance : %1 cm").arg(cm, 0, 'f', 1)
                                : tr("Distance : pas d'écho"));
    ui->lbStatus->setStyleSheet(kOkStyle);
    ui->lbStatus->setText(message);
}

void Dialog::onScanDone(bool ok, const QVector<ScanPoint> &points, const QString &message)
{
    setBusy(false);
    if (!points.isEmpty()) {
        showScan(points);   // aussi pour un balayage incomplet : on garde ce qui est arrivé
    }
    if (ok) {
        ui->lbStatus->setStyleSheet(kOkStyle);
        ui->lbStatus->setText(message);
    } else {
        showError(message);
    }
}

void Dialog::appendLog(const QString &line)
{
    ui->teLog->appendPlainText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")) + QLatin1Char(' ') + line);
}

// ---------------------------------------------------------------------------
// Affichage
// ---------------------------------------------------------------------------

void Dialog::setBusy(bool busy, const QString &message)
{
    ui->groupConnection->setEnabled(!busy);
    ui->groupAngle->setEnabled(!busy);
    ui->groupScan->setEnabled(!busy);
    setCursor(busy ? Qt::BusyCursor : Qt::ArrowCursor);
    if (!message.isEmpty()) {
        ui->lbStatus->setStyleSheet(QString());
        ui->lbStatus->setText(message);
    }
}

void Dialog::showError(const QString &message)
{
    ui->lbStatus->setStyleSheet(kErrorStyle);
    ui->lbStatus->setText(message);
}

void Dialog::showScan(const QVector<ScanPoint> &points)
{
    m_lastScan = points;
    ui->tableScan->setRowCount(points.size());
    for (int row = 0; row < points.size(); ++row) {
        const ScanPoint &p = points.at(row);
        auto *angle = new QTableWidgetItem(QString::number(p.angle));
        auto *distance = new QTableWidgetItem(p.hasEcho ? QString::number(p.cm, 'f', 1) : tr("pas d'écho"));
        angle->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        distance->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ui->tableScan->setItem(row, ColumnAngle, angle);
        ui->tableScan->setItem(row, ColumnDistance, distance);
    }
    ui->radar->setPoints(points);
    ui->btnExport->setEnabled(true);
}
