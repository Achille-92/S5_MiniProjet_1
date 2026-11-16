/*
 * udp_client.c : client UDP en C, voir udp_client.h.
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L   /* clock_gettime, getaddrinfo */
#endif
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601       /* getaddrinfo, GetTickCount64 */
#endif
#endif

#include "udp_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
typedef int addr_len_t;
#define SOCK_INVALID INVALID_SOCKET
#define sock_close closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
typedef int sock_t;
typedef socklen_t addr_len_t;
#define SOCK_INVALID (-1)
#define sock_close close
#endif

#define ABORT_POLL_MS 50      /* période de vérification de udp_client_abort */
#define DRAIN_MAX_DATAGRAMS 16
#define DRAIN_BUFFER_SIZE 2048

struct udp_client {
    sock_t sock;
    struct sockaddr_in dest;
    char host[UDP_HOST_MAXLEN];
    uint16_t port;
    volatile int aborted;
    char error[UDP_ERROR_MAXLEN];
#ifdef _WIN32
    int wsa_started;
#endif
};

/* ---------------------------------------------------------------------------
 * Messages d'erreur
 * ------------------------------------------------------------------------- */

static void set_text(udp_client_t *c, const char *text)
{
    snprintf(c->error, sizeof c->error, "%s", text);
}

/* Texte de la dernière erreur système, en UTF-8. */
static void os_error_text(char *buf, size_t size)
{
#ifdef _WIN32
    wchar_t wide[200];
    int code = WSAGetLastError();
    DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, (DWORD)code, 0, wide, 200, NULL);
    while (n > 0 && (wide[n - 1] == L'\r' || wide[n - 1] == L'\n' || wide[n - 1] == L' ')) {
        wide[--n] = L'\0';
    }
    if (n == 0 || WideCharToMultiByte(CP_UTF8, 0, wide, -1, buf, (int)size, NULL, NULL) == 0) {
        snprintf(buf, size, "erreur %d", code);
    }
#else
    snprintf(buf, size, "%s", strerror(errno));
#endif
}

/* À appeler tout de suite après l'appel système qui a échoué. */
static void set_os_error(udp_client_t *c, const char *what)
{
    char os[160];
    os_error_text(os, sizeof os);
    snprintf(c->error, sizeof c->error, "%s : %s", what, os);
}

/* Erreur de recvfrom à ignorer (Windows signale ainsi un ICMP « port
 * inaccessible » reçu pour un envoi précédent) : on continue d'attendre. */
static int recv_error_is_transient(void)
{
#ifdef _WIN32
    return WSAGetLastError() == WSAECONNRESET;
#else
    return errno == EINTR;
#endif
}

const char *udp_status_text(udp_status_t status)
{
    switch (status) {
    case UDP_OK:          return "OK";
    case UDP_ERR_ARG:     return "paramètre invalide";
    case UDP_ERR_INIT:    return "initialisation réseau impossible";
    case UDP_ERR_RESOLVE: return "adresse introuvable";
    case UDP_ERR_SOCKET:  return "socket impossible";
    case UDP_ERR_SEND:    return "envoi impossible";
    case UDP_ERR_RECV:    return "réception impossible";
    case UDP_ERR_TIMEOUT: return "pas de réponse";
    case UDP_ERR_ABORTED: return "opération annulée";
    }
    return "erreur inconnue";
}

const char *udp_client_last_error(const udp_client_t *client)
{
    return client ? client->error : "client nul";
}

/* ---------------------------------------------------------------------------
 * Cycle de vie
 * ------------------------------------------------------------------------- */

udp_client_t *udp_client_create(void)
{
    udp_client_t *c = (udp_client_t *)calloc(1, sizeof *c);
    if (c == NULL) {
        return NULL;
    }
    c->sock = SOCK_INVALID;
#ifdef _WIN32
    {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            free(c);
            return NULL;
        }
        c->wsa_started = 1;
    }
#endif
    return c;
}

void udp_client_close(udp_client_t *c)
{
    if (c != NULL && c->sock != SOCK_INVALID) {
        sock_close(c->sock);
        c->sock = SOCK_INVALID;
    }
}

void udp_client_destroy(udp_client_t *c)
{
    if (c == NULL) {
        return;
    }
    udp_client_close(c);
#ifdef _WIN32
    if (c->wsa_started) {
        WSACleanup();
    }
#endif
    free(c);
}

udp_status_t udp_client_open(udp_client_t *c, const char *host, uint16_t port)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char service[8];
    int rc;

    if (c == NULL) {
        return UDP_ERR_ARG;
    }
    if (host == NULL || host[0] == '\0' || strlen(host) >= UDP_HOST_MAXLEN || port == 0) {
        set_text(c, "Adresse ou port invalide.");
        return UDP_ERR_ARG;
    }
    udp_client_close(c);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    snprintf(service, sizeof service, "%u", (unsigned)port);

    rc = getaddrinfo(host, service, &hints, &res);
    if (rc != 0 || res == NULL || (size_t)res->ai_addrlen < sizeof c->dest) {
        snprintf(c->error, sizeof c->error, "Adresse '%s' introuvable (%s).", host, gai_strerror(rc));
        if (res != NULL) {
            freeaddrinfo(res);
        }
        return UDP_ERR_RESOLVE;
    }
    memcpy(&c->dest, res->ai_addr, sizeof c->dest);
    freeaddrinfo(res);

    c->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (c->sock == SOCK_INVALID) {
        set_os_error(c, "Création du socket impossible");
        return UDP_ERR_SOCKET;
    }
    snprintf(c->host, sizeof c->host, "%s", host);
    c->port = port;
    c->error[0] = '\0';
    return UDP_OK;
}

int udp_client_is_open_to(const udp_client_t *c, const char *host, uint16_t port)
{
    return c != NULL && host != NULL && c->sock != SOCK_INVALID
        && c->port == port && strcmp(c->host, host) == 0;
}

void udp_client_abort(udp_client_t *c)
{
    if (c != NULL) {
        c->aborted = 1;
    }
}

/* ---------------------------------------------------------------------------
 * Attente et réception
 * ------------------------------------------------------------------------- */

static uint64_t now_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}

/* >0 : un datagramme est disponible, 0 : délai écoulé, <0 : erreur. */
static int select_readable(sock_t s, int ms)
{
    fd_set readfds;
    struct timeval tv;
    int r;

    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    r = select((int)s + 1, &readfds, NULL, NULL, &tv);
#ifndef _WIN32
    if (r < 0 && errno == EINTR) {
        return 0;
    }
#endif
    return r;
}

/* Attend jusqu'à ms millisecondes en vérifiant régulièrement l'annulation.
 * 1 : prêt, 0 : délai écoulé, -1 : erreur, -2 : annulé. */
static int wait_readable(udp_client_t *c, int ms)
{
    while (ms > 0) {
        int slice = ms < ABORT_POLL_MS ? ms : ABORT_POLL_MS;
        int r;
        if (c->aborted) {
            return -2;
        }
        r = select_readable(c->sock, slice);
        if (r != 0) {
            return r < 0 ? -1 : 1;
        }
        ms -= slice;
    }
    return c->aborted ? -2 : 0;
}

/* Jette les datagrammes en retard (réponses d'un essai précédent) pour ne pas
 * les confondre avec la réponse à la commande qu'on va envoyer. */
static void drain_pending(udp_client_t *c)
{
    char junk[DRAIN_BUFFER_SIZE];
    int i;
    for (i = 0; i < DRAIN_MAX_DATAGRAMS; i++) {
        if (select_readable(c->sock, 0) <= 0) {
            break;
        }
        (void)recvfrom(c->sock, junk, sizeof junk, 0, NULL, NULL);
    }
}

static int from_expected_peer(const udp_client_t *c, const struct sockaddr_in *from)
{
    return from->sin_addr.s_addr == c->dest.sin_addr.s_addr
        && from->sin_port == c->dest.sin_port;
}

/* Attend une réponse de la carte pendant timeout_ms au maximum. */
static udp_status_t receive_reply(udp_client_t *c, char *reply, size_t size, int timeout_ms)
{
    const uint64_t deadline = now_ms() + (uint64_t)timeout_ms;

    for (;;) {
        struct sockaddr_in from;
        addr_len_t from_len = (addr_len_t)sizeof from;
        uint64_t now = now_ms();
        int ready;
        int n;

        if (now >= deadline) {
            return UDP_ERR_TIMEOUT;
        }
        ready = wait_readable(c, (int)(deadline - now));
        if (ready == -2) {
            set_text(c, "Opération annulée.");
            return UDP_ERR_ABORTED;
        }
        if (ready < 0) {
            set_os_error(c, "Attente de la réponse impossible");
            return UDP_ERR_RECV;
        }
        if (ready == 0) {
            return UDP_ERR_TIMEOUT;
        }

        n = (int)recvfrom(c->sock, reply, (int)(size - 1), 0, (struct sockaddr *)&from, &from_len);
        if (n < 0) {
            if (recv_error_is_transient()) {
                continue;
            }
            set_os_error(c, "Réception impossible");
            return UDP_ERR_RECV;
        }
        if (!from_expected_peer(c, &from)) {
            continue;   /* datagramme étranger : on l'ignore */
        }
        reply[n] = '\0';
        return UDP_OK;
    }
}

static udp_status_t send_command(udp_client_t *c, const char *cmd)
{
    const int len = (int)strlen(cmd);
    const int sent = (int)sendto(c->sock, cmd, len, 0, (const struct sockaddr *)&c->dest, sizeof c->dest);
    if (sent != len) {
        set_os_error(c, "Envoi impossible");
        return UDP_ERR_SEND;
    }
    return UDP_OK;
}

udp_status_t udp_client_request(udp_client_t *c, const char *cmd,
                                char *reply, size_t reply_size,
                                int timeout_ms, int tries, int *tries_used)
{
    int attempt;

    if (tries_used != NULL) {
        *tries_used = 0;
    }
    if (c == NULL) {
        return UDP_ERR_ARG;
    }
    if (c->sock == SOCK_INVALID || cmd == NULL || reply == NULL
        || reply_size < 2 || timeout_ms <= 0 || tries <= 0) {
        set_text(c, "Requête invalide ou socket fermée.");
        return UDP_ERR_ARG;
    }

    for (attempt = 1; attempt <= tries; attempt++) {
        udp_status_t st;

        if (c->aborted) {
            set_text(c, "Opération annulée.");
            return UDP_ERR_ABORTED;
        }
        if (tries_used != NULL) {
            *tries_used = attempt;
        }
        drain_pending(c);
        st = send_command(c, cmd);
        if (st != UDP_OK) {
            return st;
        }
        st = receive_reply(c, reply, reply_size, timeout_ms);
        if (st != UDP_ERR_TIMEOUT) {
            return st;   /* succès ou erreur définitive */
        }
    }
    snprintf(c->error, sizeof c->error,
             "Aucune réponse de %s:%u après %d essai(s) de %d ms.",
             c->host, (unsigned)c->port, tries, timeout_ms);
    return UDP_ERR_TIMEOUT;
}
