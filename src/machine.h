#ifndef OJH_MACHINE_H
#define OJH_MACHINE_H

#include <stdint.h>

#include "json.h"

/* The machine a result was measured on. Every report carries one. */
typedef struct {
    char os[128];
    char model[128];
    char cpu[128];
    char gpu[128];
    char gpu_cores[16];
    char display[64];
    int logical_cpus;
    int performance_cpus; /* 0 where the platform does not report core types */
    int efficiency_cpus;
    uint64_t memory_bytes;
    int on_battery; /* 1, 0, or -1 when unknown */
} ojh_machine;

/* A fixed, deterministic CPU workload timed on this machine, so results can be given
   per unit of CPU and compared across hardware. */
typedef struct {
    double seconds;
    int cores;
    double single_core; /* workload rounds per second on one thread */
    double all_cores;   /* summed over one thread per logical CPU */
} ojh_reference;

void ojh_machine_read(ojh_machine *m);
void ojh_reference_measure(ojh_reference *r, double seconds);
void ojh_machine_json(ojh_json *w, const ojh_machine *m, const ojh_reference *r);

#endif
