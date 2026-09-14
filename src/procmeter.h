#ifndef OJH_PROCMETER_H
#define OJH_PROCMETER_H

#include "json.h"
#include "platform.h"

typedef struct ojh_procmeter ojh_procmeter;

ojh_procmeter *ojh_procmeter_start(ojh_pid pid, double interval_seconds);
void ojh_procmeter_stop(ojh_procmeter *m);
int ojh_procmeter_samples(const ojh_procmeter *m);
void ojh_procmeter_json(ojh_json *w, const ojh_procmeter *m);

typedef struct {
    int samples;
    uint64_t peak_memory_bytes;
    uint64_t median_memory_bytes;
    double median_cpu_percent;
    double p95_cpu_percent;
    double cpu_seconds;
    int max_processes;
} ojh_procmeter_summary;

int ojh_procmeter_summarise(const ojh_procmeter *m, double from, double to, ojh_procmeter_summary *out);
void ojh_procmeter_free(ojh_procmeter *m);

#endif
