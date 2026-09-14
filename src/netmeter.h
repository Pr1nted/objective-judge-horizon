#ifndef OJH_NETMETER_H
#define OJH_NETMETER_H

#include <stdint.h>

#include "json.h"

typedef struct ojh_relay ojh_relay;

enum { OJH_UP = 0, OJH_DOWN = 1 };

ojh_relay *ojh_relay_start(uint16_t listen_port, const char *target_host, uint16_t target_port);
uint16_t ojh_relay_port(const ojh_relay *r);
void ojh_relay_mark(ojh_relay *r, const char *label);
void ojh_relay_stop(ojh_relay *r);
uint64_t ojh_relay_bytes(const ojh_relay *r, int direction);

typedef struct {
    int turns;
    uint64_t lowest[3], highest[3], median[3];
    int lowest_turn, highest_turn;
    uint64_t unmarked_tail;
} ojh_dpt;
int ojh_relay_dpt(const ojh_relay *r, ojh_dpt *out);

void ojh_relay_json(ojh_json *w, const ojh_relay *r, int clients);
void ojh_relay_free(ojh_relay *r);

#endif
