/*
 * radar_proto.h : langage de commandes textuelles de la carte (port UDP 5000).
 *
 *   ANGLE <a>                 -> OK | ERR <motif>
 *   DIST                      -> DIST <cm> | DIST TIMEOUT | ERR <motif>
 *   SCAN <début> <fin> <pas>  -> lignes "<angle> <cm>" puis END | ERR <motif>
 *
 * Le client accepte aussi le format actuel de la carte : "distance <cm>" pour
 * DIST, et "angle <a> dist <cm>" pour chaque ligne de SCAN.
 *
 * Messages en ASCII terminés par '\n' (la carte ou netcat peuvent envoyer
 * "\r\n" : les deux sont acceptés). Distances en cm, une décimale.
 *
 * Ce module ne fait aucun accès réseau : il construit les commandes et
 * analyse les réponses, ce qui permet de le tester seul.
 */
#ifndef RADAR_PROTO_H
#define RADAR_PROTO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_ANGLE_MIN 0
#define PROTO_ANGLE_MAX 180
#define PROTO_SCAN_MAX_POINTS 100   /* la réponse doit tenir dans un datagramme */
#define PROTO_CMD_MAXLEN 64
#define PROTO_REPLY_MAXLEN 2048
#define PROTO_DETAIL_MAXLEN 128     /* taille conseillée pour le paramètre detail */

typedef enum {
    PROTO_OK = 0,
    PROTO_BOARD_ERROR,  /* la carte a répondu ERR <motif> : motif dans detail */
    PROTO_NO_ECHO,      /* TIMEOUT : le capteur n'a reçu aucun écho */
    PROTO_MALFORMED,    /* réponse incompréhensible : ligne fautive dans detail */
    PROTO_INCOMPLETE    /* balayage sans ligne END : mesures déjà lues conservées */
} proto_status_t;

typedef struct {
    int angle;      /* degrés */
    double cm;      /* distance, significative seulement si has_echo */
    int has_echo;   /* 0 si la carte a signalé TIMEOUT pour cet angle */
} scan_point_t;

/* Nombre de mesures d'un balayage, ou 0 si les paramètres sont invalides
 * (angle hors 0..180, début > fin, pas < 1, trop de mesures). */
int proto_scan_point_count(int start, int end, int step);

/* Construisent la commande terminée par '\n'. Renvoient sa longueur,
 * ou -1 si un paramètre est invalide ou si le tampon est trop petit. */
int proto_build_angle(char *buf, size_t size, int angle);
int proto_build_dist(char *buf, size_t size);
int proto_build_scan(char *buf, size_t size, int start, int end, int step);

/* Analysent la réponse (terminée par '\0'). detail peut être NULL. */
proto_status_t proto_parse_ack(const char *reply, char *detail, size_t detail_size);
proto_status_t proto_parse_dist(const char *reply, double *cm, char *detail, size_t detail_size);
proto_status_t proto_parse_scan(const char *reply, scan_point_t *points, size_t max_points,
                                size_t *count, char *detail, size_t detail_size);

#ifdef __cplusplus
}
#endif

#endif /* RADAR_PROTO_H */
