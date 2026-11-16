/*
 * udp_cli.c : client en ligne de commande pour tester udp_client + radar_proto
 * sans interface graphique (étape « tester d'abord sans interface »).
 *
 * Compilation (depuis le dossier appli_client) :
 *   Linux   : gcc -Wall -Wextra -o udp_cli cli/udp_cli.c udp_client.c radar_proto.c -I.
 *   Windows : gcc -Wall -Wextra -o udp_cli.exe cli/udp_cli.c udp_client.c radar_proto.c -I. -lws2_32
 *
 * Utilisation :
 *   udp_cli <adresse> <port> ANGLE <a>
 *   udp_cli <adresse> <port> DIST
 *   udp_cli <adresse> <port> SCAN <début> <fin> <pas>
 *   udp_cli <adresse> <port> RAW <texte...>   (envoyé tel quel, pour tester les
 *                                              commandes invalides : la carte doit répondre ERR)
 *   options facultatives (avant la commande) : -t <délai ms> -r <essais>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "radar_proto.h"
#include "udp_client.h"

#define DEFAULT_TIMEOUT_MS 1000
#define DEFAULT_TRIES 3
#define SCAN_MS_PER_POINT 500   /* marge pour le déplacement du servo */

typedef struct {
    const char *host;
    uint16_t port;
    int timeout_ms;
    int tries;
    char cmd[PROTO_CMD_MAXLEN];
    int is_scan;
    int is_raw;
    int scan_points;
} options_t;

static void usage(void)
{
    fprintf(stderr,
            "Usage : udp_cli [-t délai_ms] [-r essais] <adresse> <port> <commande>\n"
            "  commande : ANGLE <a> | DIST | SCAN <début> <fin> <pas> | RAW <texte...>\n");
}

static int to_int(const char *s, int *out)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return 0;
    }
    *out = (int)v;
    return 1;
}

/* Commande RAW : les arguments sont recollés avec des espaces, sans contrôle. */
static int build_raw_command(options_t *opt, int argc, char **argv)
{
    size_t used = 0;
    int i;

    for (i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (used + len + 2 > sizeof opt->cmd) {   /* espace + '\n' */
            return 0;
        }
        memcpy(opt->cmd + used, argv[i], len);
        used += len;
        opt->cmd[used++] = (i + 1 < argc) ? ' ' : '\n';
    }
    opt->cmd[used] = '\0';
    opt->is_raw = 1;
    return 1;
}

/* Remplit opt->cmd à partir des arguments de la commande. 0 si invalide. */
static int build_command(options_t *opt, int argc, char **argv)
{
    int a, b, c;

    if (argc >= 2 && strcmp(argv[0], "RAW") == 0) {
        return build_raw_command(opt, argc - 1, argv + 1);
    }
    if (argc == 2 && strcmp(argv[0], "ANGLE") == 0 && to_int(argv[1], &a)) {
        return proto_build_angle(opt->cmd, sizeof opt->cmd, a) > 0;
    }
    if (argc == 1 && strcmp(argv[0], "DIST") == 0) {
        return proto_build_dist(opt->cmd, sizeof opt->cmd) > 0;
    }
    if (argc == 4 && strcmp(argv[0], "SCAN") == 0
        && to_int(argv[1], &a) && to_int(argv[2], &b) && to_int(argv[3], &c)) {
        opt->is_scan = 1;
        opt->scan_points = proto_scan_point_count(a, b, c);
        return proto_build_scan(opt->cmd, sizeof opt->cmd, a, b, c) > 0;
    }
    return 0;
}

static int parse_args(options_t *opt, int argc, char **argv)
{
    int i = 1;
    int port;

    opt->timeout_ms = DEFAULT_TIMEOUT_MS;
    opt->tries = DEFAULT_TRIES;
    while (i + 1 < argc && argv[i][0] == '-') {
        int *target = strcmp(argv[i], "-t") == 0 ? &opt->timeout_ms
                    : strcmp(argv[i], "-r") == 0 ? &opt->tries : NULL;
        if (target == NULL || !to_int(argv[i + 1], target) || *target <= 0) {
            return 0;
        }
        i += 2;
    }
    if (argc - i < 3 || !to_int(argv[i + 1], &port) || port < 1 || port > 65535) {
        return 0;
    }
    opt->host = argv[i];
    opt->port = (uint16_t)port;
    return build_command(opt, argc - i - 2, argv + i + 2);
}

static int report_scan(const char *reply)
{
    scan_point_t points[PROTO_SCAN_MAX_POINTS];
    char detail[PROTO_DETAIL_MAXLEN] = "";
    size_t n = 0;
    size_t i;
    proto_status_t st = proto_parse_scan(reply, points, PROTO_SCAN_MAX_POINTS, &n, detail, sizeof detail);

    for (i = 0; i < n; i++) {
        if (points[i].has_echo) {
            printf("%3d° : %.1f cm\n", points[i].angle, points[i].cm);
        } else {
            printf("%3d° : pas d'écho\n", points[i].angle);
        }
    }
    if (st != PROTO_OK) {
        fprintf(stderr, "SCAN : %s (%s)\n", st == PROTO_BOARD_ERROR ? "erreur de la carte" : "réponse invalide", detail);
        return 1;
    }
    printf("%u mesure(s).\n", (unsigned)n);
    return 0;
}

static int report_reply(const options_t *opt, const char *reply)
{
    char detail[PROTO_DETAIL_MAXLEN] = "";
    double cm = 0.0;
    proto_status_t st;

    if (opt->is_raw) {
        printf("%s", reply);   /* réponse brute, telle que reçue */
        return 0;
    }
    if (opt->is_scan) {
        return report_scan(reply);
    }
    if (strncmp(opt->cmd, "DIST", 4) == 0) {
        st = proto_parse_dist(reply, &cm, detail, sizeof detail);
        if (st == PROTO_OK) {
            printf("Distance : %.1f cm\n", cm);
        } else if (st == PROTO_NO_ECHO) {
            printf("Pas d'écho (TIMEOUT)\n");
        }
    } else {
        st = proto_parse_ack(reply, detail, sizeof detail);
        if (st == PROTO_OK) {
            printf("OK\n");
        }
    }
    if (st == PROTO_BOARD_ERROR) {
        fprintf(stderr, "Erreur de la carte : %s\n", detail);
    } else if (st == PROTO_MALFORMED) {
        fprintf(stderr, "Réponse invalide : %s\n", detail);
    }
    return (st == PROTO_OK || st == PROTO_NO_ECHO) ? 0 : 1;
}

int main(int argc, char **argv)
{
    options_t opt;
    udp_client_t *client;
    char reply[PROTO_REPLY_MAXLEN];
    int tries_used = 0;
    udp_status_t st;
    int exit_code;

    memset(&opt, 0, sizeof opt);
    if (!parse_args(&opt, argc, argv)) {
        usage();
        return 2;
    }
    if (opt.is_scan) {
        opt.timeout_ms += opt.scan_points * SCAN_MS_PER_POINT;
    }
    client = udp_client_create();
    if (client == NULL) {
        fprintf(stderr, "Initialisation réseau impossible.\n");
        return 1;
    }
    st = udp_client_open(client, opt.host, opt.port);
    if (st == UDP_OK) {
        st = udp_client_request(client, opt.cmd, reply, sizeof reply, opt.timeout_ms, opt.tries, &tries_used);
    }
    if (st != UDP_OK) {
        fprintf(stderr, "Erreur : %s\n", udp_client_last_error(client));
        udp_client_destroy(client);
        return 1;
    }
    if (tries_used > 1) {
        fprintf(stderr, "(réponse obtenue à l'essai %d)\n", tries_used);
    }
    exit_code = report_reply(&opt, reply);
    udp_client_destroy(client);
    return exit_code;
}
