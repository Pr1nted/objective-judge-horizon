#ifndef OJH_NET_H
#define OJH_NET_H

#include <stddef.h>
#include <stdint.h>

#include "json.h"
#include "netmeter.h"

#define OJH_NET_MAX_CLIENTS 16

typedef enum { OJH_NET_RELAY, OJH_NET_REPORTED } ojh_net_mode;

typedef struct {
    ojh_net_mode mode;
    const char *const *server;
    const char *const *server_env;
    const char *server_cwd;
    int server_input;
    uint16_t server_port;
    const char *ready_when;
    double start_delay;
    const char *const *client;
    const char *client_cwd;
    int clients;
    const char *connected_when;
    int connected_count;
    const char *const *after_connect;
    int turns_from_protocol;
    const char *turn_ends;
    const char *data_line;
    int turns;
    double timeout_seconds;
    int players;
    const char *transport;
    const char *log_path;
    const char *finished_when;
} ojh_net_plan;

typedef struct {
    char id[64];
    char name[128];
    char version[64];
    int exit_code;
    int clients;
    int players;
    int turns;
    double play_seconds;
    uint64_t bytes[2];
    uint64_t reads[2];
    uint64_t largest_read;
    uint64_t head_bytes;
    uint64_t tail_bytes;
    uint64_t busiest_second_bytes;
    double nipm_bytes_per_minute;
    double nipm_up_per_minute;
    double nipm_down_per_minute;
    double reads_per_minute;
    int has_dpt;
    uint64_t dpt_lowest, dpt_median, dpt_highest;
    int dpt_lowest_turn, dpt_highest_turn;
    int has_delivery;
    double delivery_median, delivery_p95;
    char transport[64];
    char how[900];
} ojh_net;

int ojh_net_analyse(const ojh_net_event *events, size_t count, const double *marks, int mark_count, int clients,
                    int measure_delivery, ojh_net *out);
int ojh_net_run(const ojh_net_plan *plan, ojh_net *out, char *error, size_t error_len);
void ojh_net_json(ojh_json *w, const ojh_net *n);

#endif
