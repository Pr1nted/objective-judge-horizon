#include "platform.h"
#include "procmeter.h"

#include <stdlib.h>
#include <string.h>

#define MAX_TREE 256

typedef struct {
    double t;
    uint64_t memory_bytes;
    double cpu_percent;
    int processes;
} sample;

struct ojh_procmeter {
    ojh_pid pid;
    double interval;
    ojh_flag stop;
    ojh_thread thread;
    int started;
    int stopped;
    ojh_mutex lock;
    sample *samples;
    int count, cap;
};

static void *loop(void *p) {
    ojh_procmeter *m = p;
    double t0 = ojh_now();
    double last_t = t0;
    uint64_t last_cpu = 0;
    int first = 1;
    ojh_pid tree[MAX_TREE];
    while (!ojh_flag_get(&m->stop)) {
        int n = ojh_process_tree(m->pid, tree, MAX_TREE);
        uint64_t memory = 0, cpu = 0;
        int alive = 0;
        for (int i = 0; i < n; i++) {
            uint64_t mem = 0, c = 0;
            if (ojh_process_usage(tree[i], &mem, &c)) {
                memory += mem;
                cpu += c;
                alive++;
            }
        }
        if (alive == 0) break;
        double now = ojh_now();
        if (!first) {
            double percent = 0.0;
            if (now > last_t && cpu >= last_cpu) percent = (double)(cpu - last_cpu) / ((now - last_t) * 1e9) * 100.0;
            ojh_mutex_lock(&m->lock);
            if (m->count == m->cap) {
                int cap = m->cap ? m->cap * 2 : 256;
                sample *grown = realloc(m->samples, (size_t)cap * sizeof *grown);
                if (grown) {
                    m->samples = grown;
                    m->cap = cap;
                }
            }
            if (m->count < m->cap) {
                sample s = {now - t0, memory, percent, alive};
                m->samples[m->count++] = s;
            }
            ojh_mutex_unlock(&m->lock);
        }
        first = 0;
        last_t = now;
        last_cpu = cpu;
        ojh_sleep(m->interval);
    }
    return NULL;
}

ojh_procmeter *ojh_procmeter_start(ojh_pid pid, double interval_seconds) {
    ojh_procmeter *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->pid = pid;
    m->interval = interval_seconds > 0 ? interval_seconds : 0.5;
    ojh_flag_init(&m->stop, 0);
    ojh_mutex_init(&m->lock);
    if (ojh_thread_start(&m->thread, loop, m) != 0) {
        ojh_mutex_destroy(&m->lock);
        free(m);
        return NULL;
    }
    m->started = 1;
    return m;
}

void ojh_procmeter_stop(ojh_procmeter *m) {
    if (!m || m->stopped) return;
    m->stopped = 1;
    ojh_flag_exchange(&m->stop, 1);
    if (m->started) ojh_thread_join(&m->thread);
}

int ojh_procmeter_samples(const ojh_procmeter *m) { return m ? m->count : 0; }

static int compare_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

void ojh_procmeter_json(ojh_json *w, const ojh_procmeter *m) {
    ojh_json_object(w);
    ojh_json_key(w, "samples"); ojh_json_int(w, m->count);
    if (m->count > 0) {
        double *mem = malloc((size_t)m->count * sizeof *mem);
        double *cpu = malloc((size_t)m->count * sizeof *cpu);
        if (mem && cpu) {
            uint64_t peak = 0;
            int processes = 0;
            for (int i = 0; i < m->count; i++) {
                mem[i] = (double)m->samples[i].memory_bytes;
                cpu[i] = m->samples[i].cpu_percent;
                if (m->samples[i].memory_bytes > peak) peak = m->samples[i].memory_bytes;
                if (m->samples[i].processes > processes) processes = m->samples[i].processes;
            }
            qsort(mem, (size_t)m->count, sizeof *mem, compare_double);
            qsort(cpu, (size_t)m->count, sizeof *cpu, compare_double);
            int p95 = (int)((m->count - 1) * 0.95);
            ojh_json_key(w, "peak_memory_bytes"); ojh_json_uint(w, peak);
            ojh_json_key(w, "median_memory_bytes"); ojh_json_double(w, mem[m->count / 2], 0);
            ojh_json_key(w, "median_cpu_percent"); ojh_json_double(w, cpu[m->count / 2], 1);
            ojh_json_key(w, "p95_cpu_percent"); ojh_json_double(w, cpu[p95], 1);
            ojh_json_key(w, "max_processes"); ojh_json_int(w, processes);
        }
        free(mem);
        free(cpu);
    }
    ojh_json_end_object(w);
}

int ojh_procmeter_summarise(const ojh_procmeter *m, double from, double to, ojh_procmeter_summary *out) {
    memset(out, 0, sizeof *out);
    if (!m || m->count == 0) return 0;
    double *mem = malloc((size_t)m->count * sizeof *mem);
    double *cpu = malloc((size_t)m->count * sizeof *cpu);
    int n = 0;
    double previous = 0;
    for (int i = 0; mem && cpu && i < m->count; i++) {
        const sample *s = &m->samples[i];
        double dt = s->t - (i > 0 ? m->samples[i - 1].t : previous);
        if (s->t < from || (to >= 0 && s->t > to)) continue;
        mem[n] = (double)s->memory_bytes;
        cpu[n] = s->cpu_percent;
        n++;
        if (s->memory_bytes > out->peak_memory_bytes) out->peak_memory_bytes = s->memory_bytes;
        if (s->processes > out->max_processes) out->max_processes = s->processes;
        if (dt > 0) out->cpu_seconds += s->cpu_percent / 100.0 * dt;
    }
    if (n > 0) {
        qsort(mem, (size_t)n, sizeof *mem, compare_double);
        qsort(cpu, (size_t)n, sizeof *cpu, compare_double);
        out->samples = n;
        out->median_memory_bytes = (uint64_t)mem[n / 2];
        out->median_cpu_percent = cpu[n / 2];
        out->p95_cpu_percent = cpu[(int)((n - 1) * 0.95)];
    }
    free(mem);
    free(cpu);
    return n;
}

void ojh_procmeter_free(ojh_procmeter *m) {
    if (!m) return;
    ojh_procmeter_stop(m);
    free(m->samples);
    ojh_mutex_destroy(&m->lock);
    free(m);
}
