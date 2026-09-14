#ifndef OJH_MACHINE_H
#define OJH_MACHINE_H

#include <stdint.h>

#include "json.h"

typedef struct {
    char os[512];
    char model[128];
    char cpu[128];
    char gpu[128];
    char gpu_cores[16];
    char display[64];
    int logical_cpus;
    int performance_cpus;
    int efficiency_cpus;
    uint64_t memory_bytes;
    int on_battery;
} ojh_machine;

typedef struct {
    double seconds;
    int cores;
    double single_core;
    double all_cores;
} ojh_reference;

void ojh_machine_read(ojh_machine *m);
void ojh_reference_measure(ojh_reference *r, double seconds);
void ojh_machine_json(ojh_json *w, const ojh_machine *m, const ojh_reference *r);

#endif
