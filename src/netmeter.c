#include "platform.h"
#include "netmeter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUCKET_SECONDS 0.1
#define WAIT_MS 200
#define BUFFER_BYTES (1 << 16)

typedef struct {
    double t;
    char label[64];
    uint64_t bytes[2]; /* running totals when the mark was made */
} mark;

struct ojh_relay {
    ojh_socket listener;
    uint16_t port;
    char target_host[128];
    uint16_t target_port;
    ojh_flag stop;
    ojh_thread accept_thread;
    int accept_started;
    int stopped;
    ojh_mutex lock;
    double t0, t1;
    int connections;
    uint64_t bytes[2], reads[2], largest[2];
    uint64_t *buckets[2];
    size_t bucket_count[2];
    mark *marks;
    size_t mark_count, mark_cap;
    ojh_thread *pumps;
    size_t pump_count, pump_cap;
};

typedef struct {
    ojh_relay *r;
    ojh_socket client, upstream;
} pump_arg;

/* Counted before forwarding, so by the time a peer has the bytes they are in the totals
   and a turn mark made on receipt includes them. */
static void record(ojh_relay *r, int direction, size_t n) {
    double now = ojh_now() - r->t0;
    size_t index = (size_t)(now / BUCKET_SECONDS);
    ojh_mutex_lock(&r->lock);
    r->bytes[direction] += n;
    r->reads[direction]++;
    if (n > r->largest[direction]) r->largest[direction] = n;
    if (index >= r->bucket_count[direction]) {
        size_t grown = r->bucket_count[direction] ? r->bucket_count[direction] * 2 : 1024;
        while (grown <= index) grown *= 2;
        uint64_t *b = realloc(r->buckets[direction], grown * sizeof *b);
        if (b) {
            memset(b + r->bucket_count[direction], 0, (grown - r->bucket_count[direction]) * sizeof *b);
            r->buckets[direction] = b;
            r->bucket_count[direction] = grown;
        }
    }
    if (index < r->bucket_count[direction]) r->buckets[direction][index] += n;
    ojh_mutex_unlock(&r->lock);
}

static void *pump(void *p) {
    pump_arg a = *(pump_arg *)p;
    free(p);
    uint8_t *buf = malloc(BUFFER_BYTES);
    ojh_socket socks[2] = {a.client, a.upstream};
    while (buf && !ojh_flag_get(&a.r->stop)) {
        int readable[2];
        int ready = ojh_wait_readable(socks, 2, WAIT_MS, readable);
        if (ready < 0) break;
        int done = 0;
        for (int i = 0; i < 2 && !done; i++) {
            if (!readable[i]) continue;
            long n = ojh_recv(socks[i], buf, BUFFER_BYTES);
            if (n <= 0) {
                done = 1;
                break;
            }
            record(a.r, i == 0 ? OJH_UP : OJH_DOWN, (size_t)n);
            if (ojh_send_all(socks[1 - i], buf, (size_t)n) != 0) done = 1;
        }
        if (done) break;
    }
    free(buf);
    ojh_sock_close(a.client);
    ojh_sock_close(a.upstream);
    return NULL;
}

static void *accept_loop(void *p) {
    ojh_relay *r = p;
    while (!ojh_flag_get(&r->stop)) {
        int readable;
        if (ojh_wait_readable(&r->listener, 1, WAIT_MS, &readable) <= 0 || !readable) continue;
        ojh_socket client = ojh_accept(r->listener);
        if (client == OJH_INVALID_SOCKET) continue;
        ojh_socket upstream = ojh_connect_tcp(r->target_host, r->target_port);
        if (upstream == OJH_INVALID_SOCKET) {
            ojh_sock_close(client);
            continue;
        }
        pump_arg *arg = malloc(sizeof *arg);
        int ok = 0;
        ojh_mutex_lock(&r->lock);
        if (arg && r->pump_count == r->pump_cap) {
            size_t cap = r->pump_cap ? r->pump_cap * 2 : 16;
            ojh_thread *grown = realloc(r->pumps, cap * sizeof *grown);
            if (grown) {
                r->pumps = grown;
                r->pump_cap = cap;
            }
        }
        if (arg && r->pump_count < r->pump_cap) {
            arg->r = r;
            arg->client = client;
            arg->upstream = upstream;
            ok = ojh_thread_start(&r->pumps[r->pump_count], pump, arg) == 0;
            if (ok) {
                r->pump_count++;
                r->connections++;
            }
        }
        ojh_mutex_unlock(&r->lock);
        if (!ok) {
            free(arg);
            ojh_sock_close(client);
            ojh_sock_close(upstream);
        }
    }
    return NULL;
}

ojh_relay *ojh_relay_start(uint16_t listen_port, const char *target_host, uint16_t target_port) {
    if (ojh_net_init() != 0) return NULL;
    ojh_relay *r = calloc(1, sizeof *r);
    if (!r) return NULL;
    ojh_mutex_init(&r->lock);
    ojh_flag_init(&r->stop, 0);
    snprintf(r->target_host, sizeof r->target_host, "%s", target_host);
    r->target_port = target_port;
    r->listener = ojh_listen_loopback(listen_port, &r->port);
    if (r->listener == OJH_INVALID_SOCKET) {
        ojh_mutex_destroy(&r->lock);
        free(r);
        return NULL;
    }
    r->t0 = ojh_now();
    if (ojh_thread_start(&r->accept_thread, accept_loop, r) != 0) {
        ojh_sock_close(r->listener);
        ojh_mutex_destroy(&r->lock);
        free(r);
        return NULL;
    }
    r->accept_started = 1;
    return r;
}

uint16_t ojh_relay_port(const ojh_relay *r) { return r->port; }

void ojh_relay_mark(ojh_relay *r, const char *label) {
    double t = ojh_now() - r->t0;
    ojh_mutex_lock(&r->lock);
    if (r->mark_count == r->mark_cap) {
        size_t cap = r->mark_cap ? r->mark_cap * 2 : 64;
        mark *grown = realloc(r->marks, cap * sizeof *grown);
        if (grown) {
            r->marks = grown;
            r->mark_cap = cap;
        }
    }
    if (r->mark_count < r->mark_cap) {
        mark *m = &r->marks[r->mark_count++];
        m->t = t;
        snprintf(m->label, sizeof m->label, "%s", label);
        m->bytes[OJH_UP] = r->bytes[OJH_UP];
        m->bytes[OJH_DOWN] = r->bytes[OJH_DOWN];
    }
    ojh_mutex_unlock(&r->lock);
}

void ojh_relay_stop(ojh_relay *r) {
    if (r->stopped) return;
    r->stopped = 1;
    ojh_flag_exchange(&r->stop, 1);
    if (r->accept_started) ojh_thread_join(&r->accept_thread);
    ojh_mutex_lock(&r->lock);
    size_t count = r->pump_count;
    ojh_mutex_unlock(&r->lock);
    for (size_t i = 0; i < count; i++) ojh_thread_join(&r->pumps[i]);
    ojh_sock_close(r->listener);
    r->t1 = ojh_now();
}

uint64_t ojh_relay_bytes(const ojh_relay *r, int direction) { return r->bytes[direction]; }

static int compare_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

int ojh_relay_dpt(const ojh_relay *r, ojh_dpt *out) {
    memset(out, 0, sizeof *out);
    out->lowest_turn = out->highest_turn = -1;
    size_t n = r->mark_count;
    if (n == 0) return 0;
    uint64_t *per[3];
    for (int d = 0; d < 3; d++) per[d] = malloc(n * sizeof(uint64_t));
    if (!per[0] || !per[1] || !per[2]) {
        for (int d = 0; d < 3; d++) free(per[d]);
        return 0;
    }
    uint64_t previous[2] = {0, 0};
    for (size_t i = 0; i < n; i++) {
        for (int d = 0; d < 2; d++) {
            per[d][i] = r->marks[i].bytes[d] - previous[d];
            previous[d] = r->marks[i].bytes[d];
        }
        per[2][i] = per[0][i] + per[1][i];
        if (out->lowest_turn < 0 || per[2][i] < per[2][out->lowest_turn]) out->lowest_turn = (int)i;
        if (out->highest_turn < 0 || per[2][i] > per[2][out->highest_turn]) out->highest_turn = (int)i;
    }
    out->unmarked_tail = (r->bytes[0] - previous[0]) + (r->bytes[1] - previous[1]);
    for (int d = 0; d < 3; d++) {
        qsort(per[d], n, sizeof(uint64_t), compare_u64);
        out->lowest[d] = per[d][0];
        out->highest[d] = per[d][n - 1];
        out->median[d] = per[d][n / 2];
        free(per[d]);
    }
    out->turns = (int)n;
    return out->turns;
}

static void pair(ojh_json *w, const char *key, double up, double down, int decimals) {
    ojh_json_key(w, key);
    ojh_json_object(w);
    ojh_json_key(w, "up"); ojh_json_double(w, up, decimals);
    ojh_json_key(w, "down"); ojh_json_double(w, down, decimals);
    ojh_json_end_object(w);
}

static void trio(ojh_json *w, const char *key, const uint64_t v[3]) {
    ojh_json_key(w, key);
    ojh_json_object(w);
    ojh_json_key(w, "up"); ojh_json_uint(w, v[OJH_UP]);
    ojh_json_key(w, "down"); ojh_json_uint(w, v[OJH_DOWN]);
    ojh_json_key(w, "both"); ojh_json_uint(w, v[2]);
    ojh_json_end_object(w);
}

void ojh_relay_json(ojh_json *w, const ojh_relay *r, int clients) {
    double end = r->t1 > 0 ? r->t1 : ojh_now();
    double seconds = end - r->t0;
    double minutes = seconds / 60.0 > 1e-9 ? seconds / 60.0 : 1e-9;
    uint64_t peak[2] = {0, 0};
    for (int d = 0; d < 2; d++) {
        for (size_t i = 0; i < r->bucket_count[d]; i++) {
            if (r->buckets[d][i] > peak[d]) peak[d] = r->buckets[d][i];
        }
    }
    ojh_json_object(w);
    ojh_json_key(w, "seconds"); ojh_json_double(w, seconds, 3);
    ojh_json_key(w, "connections"); ojh_json_int(w, r->connections);
    ojh_json_key(w, "clients");
    if (clients > 0) ojh_json_int(w, clients);
    else ojh_json_null(w);
    pair(w, "bytes", (double)r->bytes[OJH_UP], (double)r->bytes[OJH_DOWN], 0);
    pair(w, "reads", (double)r->reads[OJH_UP], (double)r->reads[OJH_DOWN], 0);
    pair(w, "largest_read_bytes", (double)r->largest[OJH_UP], (double)r->largest[OJH_DOWN], 0);
    pair(w, "nipm_bytes_per_minute", (double)r->bytes[OJH_UP] / minutes, (double)r->bytes[OJH_DOWN] / minutes, 0);
    pair(w, "reads_per_minute", (double)r->reads[OJH_UP] / minutes, (double)r->reads[OJH_DOWN] / minutes, 1);
    pair(w, "peak_bytes_per_second", (double)peak[OJH_UP] / BUCKET_SECONDS, (double)peak[OJH_DOWN] / BUCKET_SECONDS, 0);
    if (clients > 0) {
        pair(w, "nipm_bytes_per_minute_per_client", (double)r->bytes[OJH_UP] / minutes / clients,
             (double)r->bytes[OJH_DOWN] / minutes / clients, 0);
    }
    ojh_dpt dpt;
    ojh_json_key(w, "dpt");
    if (ojh_relay_dpt(r, &dpt) > 0) {
        ojh_json_object(w);
        ojh_json_key(w, "turns"); ojh_json_int(w, dpt.turns);
        trio(w, "lowest_bytes", dpt.lowest);
        trio(w, "highest_bytes", dpt.highest);
        trio(w, "median_bytes", dpt.median);
        ojh_json_key(w, "lowest_turn"); ojh_json_string(w, r->marks[dpt.lowest_turn].label);
        ojh_json_key(w, "highest_turn"); ojh_json_string(w, r->marks[dpt.highest_turn].label);
        ojh_json_key(w, "unmarked_tail_bytes"); ojh_json_uint(w, dpt.unmarked_tail);
        ojh_json_end_object(w);
    } else {
        ojh_json_null(w);
    }
    ojh_json_key(w, "turn_marks");
    ojh_json_array(w);
    for (size_t i = 0; i < r->mark_count; i++) {
        ojh_json_object(w);
        ojh_json_key(w, "t"); ojh_json_double(w, r->marks[i].t, 3);
        ojh_json_key(w, "label"); ojh_json_string(w, r->marks[i].label);
        ojh_json_key(w, "bytes_up"); ojh_json_uint(w, r->marks[i].bytes[OJH_UP]);
        ojh_json_key(w, "bytes_down"); ojh_json_uint(w, r->marks[i].bytes[OJH_DOWN]);
        ojh_json_end_object(w);
    }
    ojh_json_end_array(w);
    ojh_json_end_object(w);
}

void ojh_relay_free(ojh_relay *r) {
    if (!r) return;
    ojh_relay_stop(r);
    free(r->buckets[0]);
    free(r->buckets[1]);
    free(r->marks);
    free(r->pumps);
    ojh_mutex_destroy(&r->lock);
    free(r);
}
