#ifndef OJH_PROCMETER_H
#define OJH_PROCMETER_H

#include "json.h"
#include "platform.h"

/* CPU and memory of a game while it runs, sampled from outside the process. It samples
   the process and every descendant (games often start a server, AI clients or a browser
   helper), so the figure is what the whole game costs the machine. */
typedef struct ojh_procmeter ojh_procmeter;

ojh_procmeter *ojh_procmeter_start(ojh_pid pid, double interval_seconds);
void ojh_procmeter_stop(ojh_procmeter *m);
int ojh_procmeter_samples(const ojh_procmeter *m);
void ojh_procmeter_json(ojh_json *w, const ojh_procmeter *m);

typedef struct {
    int samples;
    uint64_t peak_memory_bytes;
    uint64_t median_memory_bytes;
    double median_cpu_percent; /* 100 = one core busy */
    double p95_cpu_percent;
    double cpu_seconds;        /* summed over every core, over the whole window asked for */
    int max_processes;
} ojh_procmeter_summary;

/* The samples taken between from and to seconds after the meter started (to < 0 for the
   end). Returns the number of samples in that window. */
int ojh_procmeter_summarise(const ojh_procmeter *m, double from, double to, ojh_procmeter_summary *out);
void ojh_procmeter_free(ojh_procmeter *m);

#endif
