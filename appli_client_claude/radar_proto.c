/*
 * radar_proto.c : construction des commandes et analyse des réponses.
 * Voir radar_proto.h pour la description du protocole.
 */
#include "radar_proto.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAXLEN 128

/* ---------------------------------------------------------------------------
 * Outils de lecture
 * ------------------------------------------------------------------------- */

static void set_detail(char *detail, size_t size, const char *text)
{
    if (detail != NULL && size > 0) {
        snprintf(detail, size, "%s", text);
    }
}

/* Retire les espaces (et le \r d'un \r\n) au début et à la fin. */
static void trim(char *s)
{
    size_t len = strlen(s);
    size_t lead = 0;

    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    while (isspace((unsigned char)s[lead])) {
        lead++;
    }
    if (lead > 0) {
        memmove(s, s + lead, len - lead + 1);
    }
}

/* Copie la prochaine ligne non vide dans line (tronquée si trop longue).
 * Renvoie 0 quand il n'y a plus rien à lire. */
static int next_line(const char **cursor, char *line, size_t size)
{
    for (;;) {
        const char *p = *cursor;
        size_t n = 0;

        if (*p == '\0') {
            return 0;
        }
        while (*p != '\0' && *p != '\n') {
            if (n + 1 < size) {
                line[n++] = *p;
            }
            p++;
        }
        if (*p == '\n') {
            p++;
        }
        *cursor = p;
        line[n] = '\0';
        trim(line);
        if (line[0] != '\0') {
            return 1;
        }
    }
}

/* Si la ligne commence par le mot kw (mot entier), renvoie le texte qui suit
 * (sans espaces de tête), sinon NULL. */
static const char *after_keyword(const char *line, const char *kw)
{
    const size_t len = strlen(kw);

    if (strncmp(line, kw, len) != 0) {
        return NULL;
    }
    if (line[len] != '\0' && !isspace((unsigned char)line[len])) {
        return NULL;
    }
    line += len;
    while (isspace((unsigned char)*line)) {
        line++;
    }
    return line;
}

static int parse_number(const char *s, double *out)
{
    char *end;
    double v = strtod(s, &end);

    if (end == s || *end != '\0' || !isfinite(v)) {
        return 0;
    }
    *out = v;
    return 1;
}

static proto_status_t board_error(const char *reason, char *detail, size_t size)
{
    set_detail(detail, size, reason[0] != '\0' ? reason : "(sans motif)");
    return PROTO_BOARD_ERROR;
}

static proto_status_t malformed(const char *line, char *detail, size_t size)
{
    set_detail(detail, size, line);
    return PROTO_MALFORMED;
}

/* ---------------------------------------------------------------------------
 * Construction des commandes
 * ------------------------------------------------------------------------- */

static int angle_is_valid(int angle)
{
    return angle >= PROTO_ANGLE_MIN && angle <= PROTO_ANGLE_MAX;
}

/* Convertit le retour de snprintf en longueur, ou -1 si tronqué. */
static int checked_length(int written, size_t size)
{
    return (written < 0 || (size_t)written >= size) ? -1 : written;
}

int proto_scan_point_count(int start, int end, int step)
{
    int count;

    if (!angle_is_valid(start) || !angle_is_valid(end) || start > end || step < 1) {
        return 0;
    }
    count = (end - start) / step + 1;
    return count > PROTO_SCAN_MAX_POINTS ? 0 : count;
}

int proto_build_angle(char *buf, size_t size, int angle)
{
    if (buf == NULL || !angle_is_valid(angle)) {
        return -1;
    }
    return checked_length(snprintf(buf, size, "ANGLE %d\n", angle), size);
}

int proto_build_dist(char *buf, size_t size)
{
    if (buf == NULL) {
        return -1;
    }
    return checked_length(snprintf(buf, size, "DIST\n"), size);
}

int proto_build_scan(char *buf, size_t size, int start, int end, int step)
{
    if (buf == NULL || proto_scan_point_count(start, end, step) == 0) {
        return -1;
    }
    return checked_length(snprintf(buf, size, "SCAN %d %d %d\n", start, end, step), size);
}

/* ---------------------------------------------------------------------------
 * Analyse des réponses
 * ------------------------------------------------------------------------- */

proto_status_t proto_parse_ack(const char *reply, char *detail, size_t detail_size)
{
    char line[LINE_MAXLEN];
    const char *cursor = reply;
    const char *rest;

    if (reply == NULL || !next_line(&cursor, line, sizeof line)) {
        return malformed("réponse vide", detail, detail_size);
    }
    rest = after_keyword(line, "ERR");
    if (rest != NULL) {
        return board_error(rest, detail, detail_size);
    }
    if (after_keyword(line, "OK") != NULL) {
        return PROTO_OK;
    }
    return malformed(line, detail, detail_size);
}

proto_status_t proto_parse_dist(const char *reply, double *cm, char *detail, size_t detail_size)
{
    char line[LINE_MAXLEN];
    const char *cursor = reply;
    const char *rest;
    double value;

    if (reply == NULL || cm == NULL || !next_line(&cursor, line, sizeof line)) {
        return malformed("réponse vide", detail, detail_size);
    }
    rest = after_keyword(line, "ERR");
    if (rest != NULL) {
        return board_error(rest, detail, detail_size);
    }
    rest = after_keyword(line, "DIST");
    if (rest == NULL) {
        return malformed(line, detail, detail_size);
    }
    if (strcmp(rest, "TIMEOUT") == 0) {
        return PROTO_NO_ECHO;
    }
    if (!parse_number(rest, &value) || value < 0.0) {
        return malformed(line, detail, detail_size);
    }
    *cm = value;
    return PROTO_OK;
}

/* Une ligne de balayage : "<angle> <cm>" ou "<angle> TIMEOUT". */
static int parse_scan_line(const char *line, scan_point_t *point)
{
    char *end;
    long angle = strtol(line, &end, 10);

    if (end == line || !isspace((unsigned char)*end) || !angle_is_valid((int)angle)) {
        return 0;
    }
    while (isspace((unsigned char)*end)) {
        end++;
    }
    point->angle = (int)angle;
    if (strcmp(end, "TIMEOUT") == 0) {
        point->cm = 0.0;
        point->has_echo = 0;
        return 1;
    }
    if (!parse_number(end, &point->cm) || point->cm < 0.0) {
        return 0;
    }
    point->has_echo = 1;
    return 1;
}

proto_status_t proto_parse_scan(const char *reply, scan_point_t *points, size_t max_points,
                                size_t *count, char *detail, size_t detail_size)
{
    char line[LINE_MAXLEN];
    const char *cursor = reply;
    const char *rest;
    size_t n = 0;
    int first = 1;
    int saw_end = 0;

    if (count != NULL) {
        *count = 0;
    }
    if (reply == NULL || points == NULL || count == NULL) {
        return malformed("réponse vide", detail, detail_size);
    }
    while (!saw_end && next_line(&cursor, line, sizeof line)) {
        if (first) {
            first = 0;
            rest = after_keyword(line, "ERR");
            if (rest != NULL) {
                return board_error(rest, detail, detail_size);
            }
        }
        if (after_keyword(line, "END") != NULL) {
            saw_end = 1;
        } else if (n >= max_points) {
            *count = n;
            return malformed("trop de mesures", detail, detail_size);
        } else if (!parse_scan_line(line, &points[n])) {
            *count = n;
            return malformed(line, detail, detail_size);
        } else {
            n++;
        }
    }
    *count = n;
    if (!saw_end) {
        set_detail(detail, detail_size, "ligne END manquante");
        return PROTO_INCOMPLETE;
    }
    return PROTO_OK;
}
