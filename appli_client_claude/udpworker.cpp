#include "udpworker.h"

extern "C" {
#include "radar_proto.h"
}

namespace {

QString fromC(const char *text)
{
    return QString::fromUtf8(text);
}

// Message d'erreur affiché quand la carte a répondu ERR ou n'a pas été comprise.
QString protocolError(proto_status_t status, const char *detail)
{
    if (status == PROTO_BOARD_ERROR) {
        return QObject::tr("Erreur de la carte : %1").arg(fromC(detail));
    }
    return QObject::tr("Réponse inattendue de la carte : « %1 »").arg(fromC(detail));
}

} // namespace

UdpWorker::UdpWorker(QObject *parent)
    : QObject(parent)
    , m_client(udp_client_create())
{
}

UdpWorker::~UdpWorker()
{
    udp_client_destroy(m_client);   // ferme la socket
}

void UdpWorker::abort()
{
    udp_client_abort(m_client);
}

// Ouvre la socket vers la cible, ou réutilise celle qui est déjà ouverte.
bool UdpWorker::ensureOpen(const UdpTarget &target, QString *error)
{
    if (m_client == nullptr) {
        *error = tr("Initialisation du réseau impossible.");
        return false;
    }
    const QByteArray host = target.host.toUtf8();
    if (udp_client_is_open_to(m_client, host.constData(), target.port)) {
        return true;
    }
    if (udp_client_open(m_client, host.constData(), target.port) != UDP_OK) {
        *error = fromC(udp_client_last_error(m_client));
        return false;
    }
    return true;
}

// Envoie une commande et récupère la réponse brute dans reply.
bool UdpWorker::exchange(const UdpTarget &target, const char *command,
                         char *reply, size_t replySize, QString *error)
{
    if (!ensureOpen(target, error)) {
        emit logMessage(QStringLiteral("✗ ") + *error);
        return false;
    }
    emit logMessage(QStringLiteral("→ ") + fromC(command).trimmed());

    int triesUsed = 0;
    const udp_status_t status = udp_client_request(m_client, command, reply, replySize,
                                                   target.timeoutMs, target.tries, &triesUsed);
    if (status != UDP_OK) {
        *error = fromC(udp_client_last_error(m_client));
        emit logMessage(QStringLiteral("✗ ") + *error);
        return false;
    }

    // Réponse multi-lignes (SCAN) : on n'en journalise que la première ligne.
    const QString text = fromC(reply).trimmed();
    QString line = QStringLiteral("← ") + text.section(QLatin1Char('\n'), 0, 0).trimmed();
    if (text.contains(QLatin1Char('\n'))) {
        line += tr(" … (%1 octets)").arg(qstrlen(reply));
    }
    if (triesUsed > 1) {
        line += tr("  [essai %1/%2]").arg(triesUsed).arg(target.tries);
    }
    emit logMessage(line);
    return true;
}

void UdpWorker::sendAngle(const UdpTarget &target, int angle)
{
    char command[PROTO_CMD_MAXLEN];
    char reply[PROTO_REPLY_MAXLEN];
    char detail[PROTO_DETAIL_MAXLEN];
    QString error;

    if (proto_build_angle(command, sizeof command, angle) < 0) {
        emit angleDone(false, tr("Angle invalide : %1 (attendu : %2 à %3).")
                                  .arg(angle).arg(PROTO_ANGLE_MIN).arg(PROTO_ANGLE_MAX));
        return;
    }
    if (!exchange(target, command, reply, sizeof reply, &error)) {
        emit angleDone(false, error);
        return;
    }
    const proto_status_t status = proto_parse_ack(reply, detail, sizeof detail);
    if (status == PROTO_OK) {
        emit angleDone(true, tr("Servo orienté à %1°.").arg(angle));
    } else {
        emit angleDone(false, protocolError(status, detail));
    }
}

void UdpWorker::measureDistance(const UdpTarget &target)
{
    char command[PROTO_CMD_MAXLEN];
    char reply[PROTO_REPLY_MAXLEN];
    char detail[PROTO_DETAIL_MAXLEN];
    QString error;
    double cm = 0.0;

    proto_build_dist(command, sizeof command);
    if (!exchange(target, command, reply, sizeof reply, &error)) {
        emit distanceDone(false, false, 0.0, error);
        return;
    }
    const proto_status_t status = proto_parse_dist(reply, &cm, detail, sizeof detail);
    switch (status) {
    case PROTO_OK:
        emit distanceDone(true, true, cm, tr("Distance mesurée : %1 cm.").arg(cm, 0, 'f', 1));
        break;
    case PROTO_NO_ECHO:
        emit distanceDone(true, false, 0.0, tr("Aucun écho (TIMEOUT) : pas d'obstacle à portée."));
        break;
    default:
        emit distanceDone(false, false, 0.0, protocolError(status, detail));
        break;
    }
}

void UdpWorker::scan(const UdpTarget &target, int start, int end, int step)
{
    char command[PROTO_CMD_MAXLEN];
    char reply[PROTO_REPLY_MAXLEN];
    char detail[PROTO_DETAIL_MAXLEN];
    scan_point_t raw[PROTO_SCAN_MAX_POINTS];
    size_t count = 0;
    QString error;

    if (proto_build_scan(command, sizeof command, start, end, step) < 0) {
        emit scanDone(false, {}, tr("Paramètres de balayage invalides."));
        return;
    }
    if (!exchange(target, command, reply, sizeof reply, &error)) {
        emit scanDone(false, {}, error);
        return;
    }
    const proto_status_t status = proto_parse_scan(reply, raw, PROTO_SCAN_MAX_POINTS,
                                                   &count, detail, sizeof detail);

    QVector<ScanPoint> points;
    points.reserve(static_cast<int>(count));
    for (size_t i = 0; i < count; ++i) {
        points.append({raw[i].angle, raw[i].cm, raw[i].has_echo != 0});
    }

    if (status == PROTO_OK) {
        emit scanDone(true, points, tr("Balayage terminé : %1 mesure(s).").arg(points.size()));
    } else if (status == PROTO_INCOMPLETE) {
        emit scanDone(false, points,
                      tr("Réponse incomplète (%1) : %2 mesure(s) reçue(s).")
                          .arg(fromC(detail)).arg(points.size()));
    } else {
        emit scanDone(false, points, protocolError(status, detail));
    }
}
