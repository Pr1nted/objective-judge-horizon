#include "platform.h"
#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runner.h"

#define DELIVERY_GAP_SECONDS 0.1
#define DELIVERY_WINDOW_SECONDS 10.0
#define MAX_ARGS 64
#define LINE_TEXT 4096

static int fail(char *error, size_t len, const char *message) {
    if (error && len) snprintf(error, len, "%s", message);
    return -1;
}

static int compare_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static int compare_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int ojh_net_analyse(const ojh_net_event *events, size_t count, const double *marks, int mark_count, int clients,
                    int measure_delivery, ojh_net *out) {
    out->clients = clients;
    out->bytes[0] = out->bytes[1] = 0;
    out->reads[0] = out->reads[1] = 0;
    out->largest_read = 0;
    for (size_t i = 0; i < count; i++) {
        int d = events[i].direction ? 1 : 0;
        out->bytes[d] += events[i].bytes;
        out->reads[d]++;
        if (events[i].bytes > out->largest_read) out->largest_read = events[i].bytes;
    }
    double first = count ? events[0].t : 0, last = count ? events[count - 1].t : 0;
    if (count > 0) {
        size_t seconds = (size_t)(last - first) + 1;
        uint64_t *per_second = calloc(seconds, sizeof *per_second);
        if (per_second) {
            for (size_t i = 0; i < count; i++) per_second[(size_t)(events[i].t - first)] += events[i].bytes;
            for (size_t s = 0; s < seconds; s++) {
                if (per_second[s] > out->busiest_second_bytes) out->busiest_second_bytes = per_second[s];
            }
            free(per_second);
        }
    }
    if (mark_count < 2) {
        out->turns = 0;
        out->play_seconds = count > 1 ? last - first : 0;
    } else {
        out->turns = mark_count - 1;
        out->play_seconds = marks[mark_count - 1] - marks[0];
    }
    uint64_t in_play[2] = {0, 0}, reads_in_play = 0;
    for (size_t i = 0; i < count; i++) {
        int d = events[i].direction ? 1 : 0;
        if (mark_count >= 2 && events[i].t < marks[0]) out->head_bytes += events[i].bytes;
        else if (mark_count >= 2 && events[i].t >= marks[mark_count - 1]) out->tail_bytes += events[i].bytes;
        if (mark_count < 2 || (events[i].t >= marks[0] && events[i].t < marks[mark_count - 1])) {
            in_play[d] += events[i].bytes;
            reads_in_play++;
        }
    }
    if (out->play_seconds > 0) {
        double minutes = out->play_seconds / 60.0;
        out->nipm_up_per_minute = in_play[0] / minutes;
        out->nipm_down_per_minute = in_play[1] / minutes;
        out->nipm_bytes_per_minute = (in_play[0] + in_play[1]) / minutes;
        out->reads_per_minute = reads_in_play / minutes;
    }
    if (mark_count >= 2) {
        int turns = mark_count - 1;
        uint64_t *per_turn = calloc((size_t)turns, sizeof *per_turn);
        uint64_t *sorted = calloc((size_t)turns, sizeof *sorted);
        double *delivery = calloc((size_t)turns, sizeof *delivery);
        int delivered = 0;
        if (per_turn && sorted && delivery) {
            size_t e = 0;
            for (int k = 0; k < turns; k++) {
                while (e < count && events[e].t < marks[k]) e++;
                size_t f = e;
                while (f < count && events[f].t < marks[k + 1]) {
                    per_turn[k] += events[f].bytes;
                    f++;
                }
            }
            memcpy(sorted, per_turn, (size_t)turns * sizeof *sorted);
            qsort(sorted, (size_t)turns, sizeof *sorted, compare_u64);
            out->has_dpt = 1;
            out->dpt_lowest = sorted[0];
            out->dpt_highest = sorted[turns - 1];
            out->dpt_median = sorted[turns / 2];
            for (int k = 0; k < turns; k++) {
                if (per_turn[k] == out->dpt_lowest && out->dpt_lowest_turn == 0) out->dpt_lowest_turn = k + 1;
                if (per_turn[k] == out->dpt_highest && out->dpt_highest_turn == 0) out->dpt_highest_turn = k + 1;
            }
            if (measure_delivery) {
                for (int k = 1; k < mark_count; k++) {
                    size_t i = 0;
                    while (i < count && (events[i].t < marks[k] || !events[i].direction)) i++;
                    if (i >= count || events[i].t - marks[k] > DELIVERY_WINDOW_SECONDS) continue;
                    double end = events[i].t;
                    for (size_t j = i + 1; j < count; j++) {
                        if (!events[j].direction) continue;
                        if (events[j].t - end > DELIVERY_GAP_SECONDS) break;
                        end = events[j].t;
                    }
                    delivery[delivered++] = end - marks[k];
                }
                if (delivered > 0) {
                    qsort(delivery, (size_t)delivered, sizeof *delivery, compare_double);
                    out->has_delivery = 1;
                    out->delivery_median = delivery[delivered / 2];
                    out->delivery_p95 = delivery[(int)((delivered - 1) * 0.95)];
                }
            }
        }
        free(per_turn);
        free(sorted);
        free(delivery);
    }
    return out->turns > 0 ? 0 : -1;
}

static char *replace_all(const char *text, const char *key, const char *value) {
    size_t kl = strlen(key), vl = strlen(value), n = 0;
    for (const char *p = strstr(text, key); p; p = strstr(p + kl, key)) n++;
    char *out = malloc(strlen(text) + n * (vl > kl ? vl - kl : 0) + 1);
    if (!out) return NULL;
    char *w = out;
    for (const char *p = text;;) {
        const char *hit = strstr(p, key);
        if (!hit) {
            strcpy(w, p);
            break;
        }
        memcpy(w, p, (size_t)(hit - p));
        w += hit - p;
        memcpy(w, value, vl);
        w += vl;
        p = hit + kl;
    }
    return out;
}

static char **fill_args(const char *const *args, uint16_t server_port, uint16_t relay_port, int client) {
    int n = 0;
    while (args && args[n] && n < MAX_ARGS) n++;
    char **out = calloc((size_t)n + 1, sizeof *out);
    if (!out) return NULL;
    char sp[16], rp[16], ci[16];
    snprintf(sp, sizeof sp, "%u", (unsigned)server_port);
    snprintf(rp, sizeof rp, "%u", (unsigned)relay_port);
    snprintf(ci, sizeof ci, "%d", client);
    for (int i = 0; i < n; i++) {
        char *a = replace_all(args[i], "{server_port}", sp);
        char *b = a ? replace_all(a, "{relay_port}", rp) : NULL;
        char *c = b ? replace_all(b, "{client}", ci) : NULL;
        free(a);
        free(b);
        out[i] = c;
    }
    return out;
}

static void free_args(char **args) {
    for (int i = 0; args && args[i]; i++) free(args[i]);
    free(args);
}

static const char *protocol_value(const char *s, const char *word) {
    size_t n = strlen(word);
    for (const char *p = strstr(s, "OJH "); p; p = strstr(p + 1, "OJH ")) {
        int starts = p == s || p[-1] == ' ' || p[-1] == '\t' || p[-1] == ']' || p[-1] == ':';
        if (starts && strncmp(p + 4, word, n) == 0 && (p[4 + n] == ' ' || p[4 + n] == '\0')) return p + 4 + n;
    }
    return NULL;
}

typedef struct {
    double *marks;
    int count, cap;
} mark_list;

static void add_mark(mark_list *m, double t) {
    if (m->count == m->cap) {
        int cap = m->cap ? m->cap * 2 : 256;
        double *grown = realloc(m->marks, (size_t)cap * sizeof *grown);
        if (!grown) return;
        m->marks = grown;
        m->cap = cap;
    }
    m->marks[m->count++] = t;
}

static int is_turn_line(const ojh_net_plan *p, const char *text) {
    if (p->turns_from_protocol) return protocol_value(text, "turn") != NULL;
    return p->turn_ends && strstr(text, p->turn_ends) != NULL;
}

static int run_reported(const ojh_net_plan *p, ojh_net *out, char *error, size_t error_len) {
    ojh_run *server = ojh_run_start(p->server, p->server_env, p->server_cwd);
    if (!server) return fail(error, error_len, "the game did not start (is the path right?)");
    int code = ojh_run_wait(server, p->timeout_seconds > 0 ? p->timeout_seconds : 3600);
    out->exit_code = code;
    size_t lines = ojh_run_line_count(server);
    double base = ojh_run_started(server);
    mark_list marks = {0};
    ojh_net_event *events = calloc(lines * 2 + 1, sizeof *events);
    size_t count = 0;
    for (size_t i = 0; events && i < lines; i++) {
        const ojh_line *l = ojh_run_line(server, i);
        const char *v = protocol_value(l->text, p->data_line ? p->data_line : "data");
        if (v) {
            unsigned long long up = 0, down = 0;
            if (sscanf(v, "%llu %llu", &up, &down) >= 1) {
                ojh_net_event a = {base + l->t, 0, up > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)up};
                ojh_net_event b = {base + l->t, 1, down > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)down};
                if (up) events[count++] = a;
                if (down) events[count++] = b;
            }
        }
        if (is_turn_line(p, l->text)) add_mark(&marks, base + l->t);
    }
    if (code != 0 && error && error_len) {
        size_t at = (size_t)snprintf(error, error_len, "exit %d; last lines:", code);
        for (size_t i = lines > 4 ? lines - 4 : 0; i < lines && at < error_len; i++) {
            at += (size_t)snprintf(error + at, error_len - at, " | %s", ojh_run_line(server, i)->text);
        }
    }
    int status = ojh_net_analyse(events, count, marks.marks, marks.count, p->clients, 0, out);
    free(events);
    free(marks.marks);
    ojh_run_free(server);
    if (code != 0) return -1;
    return status == 0 ? 0 : fail(error, error_len, "no turns with data were reported");
}

static int count_lines_with(ojh_run *r, const char *text, size_t *scanned, int *found) {
    char line[LINE_TEXT];
    size_t n = ojh_run_line_count_now(r);
    for (; *scanned < n; (*scanned)++) {
        if (ojh_run_copy_line(r, *scanned, line, sizeof line, NULL, NULL) && strstr(line, text)) (*found)++;
    }
    return *found;
}

int ojh_net_run(const ojh_net_plan *p, ojh_net *out, char *error, size_t error_len) {
    out->exit_code = -1;
    out->players = p->players;
    snprintf(out->transport, sizeof out->transport, "%s", p->transport ? p->transport : "tcp");
    if (p->mode == OJH_NET_REPORTED) return run_reported(p, out, error, error_len);
    if (p->clients < 1 || p->clients > OJH_NET_MAX_CLIENTS) return fail(error, error_len, "a session needs 1 to 16 clients");
    if (ojh_net_init() != 0) return fail(error, error_len, "cannot start networking");

    double timeout = p->timeout_seconds > 0 ? p->timeout_seconds : 3600;
    double deadline = ojh_now() + timeout;
    ojh_relay *relay = ojh_relay_start(0, "127.0.0.1", p->server_port);
    if (!relay) return fail(error, error_len, "the relay could not listen on loopback");
    uint16_t relay_port = ojh_relay_port(relay);

    char **server_args = fill_args(p->server, p->server_port, relay_port, 0);
    ojh_run *server = server_args ? (p->server_input ? ojh_run_start_with_input((const char *const *)server_args, p->server_env, p->server_cwd)
                                                     : ojh_run_start((const char *const *)server_args, p->server_env, p->server_cwd))
                                  : NULL;
    free_args(server_args);
    if (!server) {
        ojh_relay_stop(relay);
        ojh_relay_free(relay);
        return fail(error, error_len, "the game server did not start (is the path right?)");
    }

    int status = -1;
    size_t scanned = 0;
    int found = 0;
    if (p->ready_when) {
        while (ojh_now() < deadline && ojh_run_running(server) && count_lines_with(server, p->ready_when, &scanned, &found) == 0) {
            ojh_sleep(0.02);
        }
        if (found == 0) {
            fail(error, error_len, "the game server never said it was ready");
            goto finish;
        }
    }
    if (p->start_delay > 0) ojh_sleep(p->start_delay);

    ojh_run *clients[OJH_NET_MAX_CLIENTS] = {0};
    int started = 0;
    for (int c = 0; c < p->clients; c++) {
        char **args = fill_args(p->client, p->server_port, relay_port, c + 1);
        clients[c] = args ? ojh_run_start((const char *const *)args, NULL, p->client_cwd) : NULL;
        free_args(args);
        if (clients[c]) started++;
    }
    if (started < p->clients) {
        fail(error, error_len, "a game client did not start (is the path right?)");
        goto stop_clients;
    }
    if (p->connected_when) {
        scanned = 0;
        found = 0;
        int wanted = p->connected_count > 0 ? p->connected_count : p->clients;
        while (ojh_now() < deadline && ojh_run_running(server) &&
               count_lines_with(server, p->connected_when, &scanned, &found) < wanted) {
            ojh_sleep(0.05);
        }
        if (found < wanted) {
            fail(error, error_len, "the clients never all connected to the server");
            goto stop_clients;
        }
    }
    for (int i = 0; p->after_connect && p->after_connect[i]; i++) {
        char **line = fill_args((const char *const[]){p->after_connect[i], NULL}, p->server_port, relay_port, 0);
        if (line && line[0]) {
            ojh_run_write(server, line[0]);
            ojh_run_write(server, "\n");
        }
        free_args(line);
    }

    mark_list marks = {0};
    size_t read_lines = 0;
    char text[LINE_TEXT];
    while (ojh_now() < deadline) {
        size_t n = ojh_run_line_count_now(server);
        for (; read_lines < n; read_lines++) {
            double t;
            if (ojh_run_copy_line(server, read_lines, text, sizeof text, &t, NULL) && is_turn_line(p, text)) {
                add_mark(&marks, ojh_run_started(server) + t);
            }
        }
        if (marks.count > p->turns || !ojh_run_running(server)) break;
        ojh_sleep(0.02);
    }
    ojh_sleep(1.0);
    size_t n = ojh_run_line_count_now(server);
    for (; read_lines < n; read_lines++) {
        double t;
        if (ojh_run_copy_line(server, read_lines, text, sizeof text, &t, NULL) && is_turn_line(p, text) &&
            marks.count <= p->turns) {
            add_mark(&marks, ojh_run_started(server) + t);
        }
    }
    ojh_relay_stop(relay);
    status = ojh_net_analyse(ojh_relay_events(relay), ojh_relay_event_count(relay), marks.marks, marks.count, p->clients,
                             1, out);
    if (status != 0) {
        char seen[LINE_TEXT] = "";
        size_t lines = ojh_run_line_count_now(server);
        size_t at = (size_t)snprintf(error, error_len, "%d turn marks in %llu bytes; last server lines:", marks.count,
                                     (unsigned long long)(out->bytes[0] + out->bytes[1]));
        for (size_t i = lines > 4 ? lines - 4 : 0; i < lines && at < error_len; i++) {
            if (ojh_run_copy_line(server, i, seen, sizeof seen, NULL, NULL)) {
                at += (size_t)snprintf(error + at, error_len - at, " | %s", seen);
            }
        }
    }
    free(marks.marks);

stop_clients:
    for (int c = 0; c < p->clients; c++) {
        if (!clients[c]) continue;
        ojh_run_wait(clients[c], 0.001);
        ojh_run_free(clients[c]);
    }
finish:
    ojh_run_close_input(server);
    out->exit_code = ojh_run_wait(server, ojh_run_running(server) ? 5.0 : 0.001);
    ojh_run_free(server);
    ojh_relay_stop(relay);
    ojh_relay_free(relay);
    return status;
}

void ojh_net_json(ojh_json *w, const ojh_net *n) {
    ojh_json_object(w);
    ojh_json_key(w, "game"); ojh_json_string(w, n->id);
    ojh_json_key(w, "name"); ojh_json_string(w, n->name);
    if (*n->version) {
        ojh_json_key(w, "version"); ojh_json_string(w, n->version);
    }
    ojh_json_key(w, "exit_code"); ojh_json_int(w, n->exit_code);
    ojh_json_key(w, "transport"); ojh_json_string(w, n->transport);
    ojh_json_key(w, "clients"); ojh_json_int(w, n->clients);
    ojh_json_key(w, "players");
    if (n->players > 0) ojh_json_int(w, n->players);
    else ojh_json_null(w);
    ojh_json_key(w, "turns"); ojh_json_int(w, n->turns);
    ojh_json_key(w, "play_seconds"); ojh_json_double(w, n->play_seconds, 3);
    ojh_json_key(w, "bytes_up"); ojh_json_uint(w, n->bytes[0]);
    ojh_json_key(w, "bytes_down"); ojh_json_uint(w, n->bytes[1]);
    ojh_json_key(w, "reads_up"); ojh_json_uint(w, n->reads[0]);
    ojh_json_key(w, "reads_down"); ojh_json_uint(w, n->reads[1]);
    ojh_json_key(w, "largest_read_bytes"); ojh_json_uint(w, n->largest_read);
    ojh_json_key(w, "before_first_turn_bytes"); ojh_json_uint(w, n->head_bytes);
    ojh_json_key(w, "after_last_turn_bytes"); ojh_json_uint(w, n->tail_bytes);
    ojh_json_key(w, "busiest_second_bytes"); ojh_json_uint(w, n->busiest_second_bytes);
    ojh_json_key(w, "nipm_bytes_per_minute"); ojh_json_double(w, n->nipm_bytes_per_minute, 0);
    ojh_json_key(w, "nipm_up_bytes_per_minute"); ojh_json_double(w, n->nipm_up_per_minute, 0);
    ojh_json_key(w, "nipm_down_bytes_per_minute"); ojh_json_double(w, n->nipm_down_per_minute, 0);
    ojh_json_key(w, "nipm_per_client_bytes");
    if (n->clients > 0) ojh_json_double(w, n->nipm_bytes_per_minute / n->clients, 0);
    else ojh_json_null(w);
    ojh_json_key(w, "reads_per_minute"); ojh_json_double(w, n->reads_per_minute, 1);
    ojh_json_key(w, "dpt");
    if (n->has_dpt) {
        ojh_json_object(w);
        ojh_json_key(w, "lowest_bytes"); ojh_json_uint(w, n->dpt_lowest);
        ojh_json_key(w, "median_bytes"); ojh_json_uint(w, n->dpt_median);
        ojh_json_key(w, "highest_bytes"); ojh_json_uint(w, n->dpt_highest);
        ojh_json_key(w, "lowest_turn"); ojh_json_int(w, n->dpt_lowest_turn);
        ojh_json_key(w, "highest_turn"); ojh_json_int(w, n->dpt_highest_turn);
        ojh_json_end_object(w);
    } else {
        ojh_json_null(w);
    }
    ojh_json_key(w, "delivery");
    if (n->has_delivery) {
        ojh_json_object(w);
        ojh_json_key(w, "median_seconds"); ojh_json_double(w, n->delivery_median, 5);
        ojh_json_key(w, "p95_seconds"); ojh_json_double(w, n->delivery_p95, 5);
        ojh_json_end_object(w);
    } else {
        ojh_json_null(w);
    }
    ojh_json_key(w, "how"); ojh_json_string(w, n->how);
    ojh_json_end_object(w);
}
