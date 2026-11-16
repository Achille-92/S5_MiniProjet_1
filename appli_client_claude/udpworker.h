#ifndef UDPWORKER_H
#define UDPWORKER_H

#include <QObject>
#include <QString>
#include <QVector>

#include "scanpoint.h"

extern "C" {
#include "udp_client.h"
}

// Où et comment joindre la carte (saisi dans l'interface).
struct UdpTarget
{
    QString host;           // adresse WAN du routeur (IPv4 ou nom)
    quint16 port = 0;       // port externe de la redirection
    int timeoutMs = 1000;   // attente d'une réponse, par essai
    int tries = 3;          // nombre total d'envois (1 = pas de nouvel essai)
};

// Exécute les échanges UDP dans un thread à part, pour que l'interface reste
// réactive pendant l'attente d'une réponse. Les méthodes d'échange sont
// appelées depuis ce thread (voir Dialog) ; les résultats reviennent par
// signaux.
class UdpWorker : public QObject
{
    Q_OBJECT

public:
    explicit UdpWorker(QObject *parent = nullptr);
    ~UdpWorker() override;

    // Interrompt l'attente en cours. Utilisable depuis n'importe quel thread,
    // définitif : à n'appeler qu'avant la destruction.
    void abort();

    void sendAngle(const UdpTarget &target, int angle);
    void measureDistance(const UdpTarget &target);
    void scan(const UdpTarget &target, int start, int end, int step);

signals:
    void angleDone(bool ok, const QString &message);
    void distanceDone(bool ok, bool hasEcho, double cm, const QString &message);
    void scanDone(bool ok, const QVector<ScanPoint> &points, const QString &message);
    void logMessage(const QString &line);

private:
    bool ensureOpen(const UdpTarget &target, QString *error);
    bool exchange(const UdpTarget &target, const char *command,
                  char *reply, size_t replySize, QString *error);

    udp_client_t *m_client = nullptr;
};

#endif // UDPWORKER_H
