/*
 * udp_client.h : client UDP en C (API sockets : socket / sendto / recvfrom).
 *
 * Module de transport uniquement : il envoie une chaîne de caractères à la
 * carte et rend la réponse brute. Il ne connaît pas le protocole ANGLE / DIST
 * / SCAN (voir radar_proto.h).
 *
 * Portable Windows (Winsock) et Linux. Sous Windows, ajouter -lws2_32.
 *
 * Principe : un datagramme UDP peut être perdu. udp_client_request() attend
 * donc la réponse pendant un délai, puis renvoie la commande (nouvel essai).
 */
#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UDP_HOST_MAXLEN 128   /* taille max d'une adresse ou d'un nom d'hôte */
#define UDP_ERROR_MAXLEN 256  /* taille du dernier message d'erreur */

typedef enum {
    UDP_OK = 0,
    UDP_ERR_ARG,      /* paramètre invalide ou socket non ouverte */
    UDP_ERR_INIT,     /* initialisation de Winsock impossible */
    UDP_ERR_RESOLVE,  /* adresse introuvable */
    UDP_ERR_SOCKET,   /* création du socket impossible */
    UDP_ERR_SEND,     /* échec de sendto */
    UDP_ERR_RECV,     /* échec de recvfrom ou select */
    UDP_ERR_TIMEOUT,  /* aucune réponse après tous les essais */
    UDP_ERR_ABORTED   /* udp_client_abort() a été appelée */
} udp_status_t;

/* Type opaque : le contenu est caché dans udp_client.c. */
typedef struct udp_client udp_client_t;

/* Crée un client (socket encore fermée). Renvoie NULL en cas d'échec. */
udp_client_t *udp_client_create(void);

/* Ferme la socket si besoin et libère le client. NULL accepté. */
void udp_client_destroy(udp_client_t *client);

/* Résout l'adresse (IPv4 ou nom d'hôte) et crée la socket UDP.
 * Referme d'abord une éventuelle socket déjà ouverte. */
udp_status_t udp_client_open(udp_client_t *client, const char *host, uint16_t port);

/* Ferme la socket (le client peut être rouvert ensuite). */
void udp_client_close(udp_client_t *client);

/* Vrai si la socket est ouverte vers exactement cette adresse et ce port. */
int udp_client_is_open_to(const udp_client_t *client, const char *host, uint16_t port);

/* Envoie cmd et attend la réponse.
 *  - timeout_ms : durée d'attente par essai ;
 *  - tries      : nombre total d'envois (1 = pas de nouvel essai) ;
 *  - reply      : tampon de réception, toujours terminé par '\0' si UDP_OK ;
 *  - tries_used : (facultatif, peut être NULL) nombre d'envois effectués.
 * Les datagrammes qui ne viennent pas de l'adresse visée sont ignorés. */
udp_status_t udp_client_request(udp_client_t *client, const char *cmd,
                                char *reply, size_t reply_size,
                                int timeout_ms, int tries, int *tries_used);

/* Demande l'arrêt de la requête en cours (utilisable depuis un autre thread).
 * Définitif : le client doit ensuite être détruit. */
void udp_client_abort(udp_client_t *client);

/* Dernier message d'erreur, en français (UTF-8). Jamais NULL. */
const char *udp_client_last_error(const udp_client_t *client);

/* Texte court correspondant à un code de retour. */
const char *udp_status_text(udp_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* UDP_CLIENT_H */
