#ifndef OJH_NETMETER_H
#define OJH_NETMETER_H

#include <stdint.h>

#include "json.h"

/* Counts what a game's netcode sends, on loopback, so the internet is not in the number.

   A game's clients connect to the relay instead of the server. The relay forwards every
   byte unchanged and records how many went each way, when, and in how many reads. No
   latency, loss or bandwidth limit applies other than the machine itself, which is what
   makes one game's netcode comparable with another's.

   A game driver calls ojh_relay_mark() when each turn ends. The bytes between two marks
   are what that turn took: DPT, data per turn, reported as the lowest and the highest.

     ojh_relay *r = ojh_relay_start(37015, "127.0.0.1", 27015);
     ... point the clients at 127.0.0.1:37015 and play, marking each turn ...
     ojh_relay_stop(r);
     ojh_relay_json(w, r, clients);
     ojh_relay_free(r);
*/
typedef struct ojh_relay ojh_relay;

enum { OJH_UP = 0, OJH_DOWN = 1 }; /* client to server, server to client */

ojh_relay *ojh_relay_start(uint16_t listen_port, const char *target_host, uint16_t target_port);
uint16_t ojh_relay_port(const ojh_relay *r); /* the port actually bound (for listen_port 0) */
void ojh_relay_mark(ojh_relay *r, const char *label);
void ojh_relay_stop(ojh_relay *r);
uint64_t ojh_relay_bytes(const ojh_relay *r, int direction);

/* Data per turn in bytes, from the marks. Returns the number of turns (0 without marks). */
typedef struct {
    int turns;
    uint64_t lowest[3], highest[3], median[3]; /* [OJH_UP], [OJH_DOWN], [2] = both ways */
    int lowest_turn, highest_turn;             /* index of the turn with the fewest / most bytes both ways */
    uint64_t unmarked_tail;                    /* bytes after the last mark: part of no turn */
} ojh_dpt;
int ojh_relay_dpt(const ojh_relay *r, ojh_dpt *out);

void ojh_relay_json(ojh_json *w, const ojh_relay *r, int clients);
void ojh_relay_free(ojh_relay *r);

#endif
