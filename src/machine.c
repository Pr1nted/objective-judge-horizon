#ifndef _WIN32
#  define _DEFAULT_SOURCE
#  define _DARWIN_C_SOURCE
#endif
#include "platform.h"
#include "machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sha256.h"

#ifdef _WIN32
#  include <windows.h>
#else
#  include <sys/utsname.h>
#  include <unistd.h>
#  ifdef __APPLE__
#    include <sys/sysctl.h>
#  endif
#endif

#ifndef _WIN32
static void copy_trimmed(char *dst, size_t n, const char *src) {
    while (*src == ' ' || *src == '\t') src++;
    size_t len = strlen(src);
    while (len > 0 && (src[len - 1] == '\n' || src[len - 1] == ' ' || src[len - 1] == '\r')) len--;
    if (len >= n) len = n - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static int command_field(const char *command, const char *label, char *out, size_t n) {
    FILE *p = popen(command, "r");
    if (!p) return 0;
    char line[512];
    size_t label_len = strlen(label);
    int found = 0;
    while (fgets(line, sizeof line, p)) {
        char *at = strstr(line, label);
        if (!at) continue;
        char *colon = at + label_len;
        while (*colon == ' ' || *colon == '\t') colon++;
        if (*colon == ':') {
            copy_trimmed(out, n, colon + 1);
            found = 1;
            break;
        }
    }
    pclose(p);
    return found;
}
#endif

#ifdef __APPLE__
static void sysctl_string(const char *name, char *out, size_t n) {
    size_t len = n;
    if (sysctlbyname(name, out, &len, NULL, 0) != 0) out[0] = '\0';
    else out[n - 1] = '\0';
}

static int64_t sysctl_int(const char *name) {
    unsigned char buf[8] = {0};
    size_t len = sizeof buf;
    if (sysctlbyname(name, buf, &len, NULL, 0) != 0) return 0;
    if (len == 4) {
        int32_t v;
        memcpy(&v, buf, 4);
        return v;
    }
    int64_t v;
    memcpy(&v, buf, 8);
    return v;
}
#endif

#ifdef _WIN32
static void registry_string(const char *key, const char *value, char *out, size_t n) {
    DWORD size = (DWORD)n;
    if (RegGetValueA(HKEY_LOCAL_MACHINE, key, value, RRF_RT_REG_SZ, NULL, out, &size) != ERROR_SUCCESS) {
        out[0] = '\0';
    }
}
#endif

void ojh_machine_read(ojh_machine *m) {
    memset(m, 0, sizeof *m);
    m->on_battery = -1;
#ifdef _WIN32
    char product[96] = "", build[32] = "", display_version[32] = "";
    registry_string("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName", product, sizeof product);
    registry_string("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuild", build, sizeof build);
    registry_string("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "DisplayVersion", display_version,
                    sizeof display_version);
    snprintf(m->os, sizeof m->os, "%s %s (build %s)", product[0] ? product : "Windows", display_version, build);
    registry_string("HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemProductName", m->model, sizeof m->model);
    registry_string("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString", m->cpu,
                    sizeof m->cpu);
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m->logical_cpus = (int)si.dwNumberOfProcessors;
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms)) m->memory_bytes = (uint64_t)ms.ullTotalPhys;
    DISPLAY_DEVICEA dd;
    memset(&dd, 0, sizeof dd);
    dd.cb = sizeof dd;
    if (EnumDisplayDevicesA(NULL, 0, &dd, 0)) snprintf(m->gpu, sizeof m->gpu, "%s", dd.DeviceString);
    snprintf(m->display, sizeof m->display, "%d x %d", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    SYSTEM_POWER_STATUS ps;
    if (GetSystemPowerStatus(&ps) && ps.ACLineStatus != 255) m->on_battery = ps.ACLineStatus == 0;
#else
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    m->logical_cpus = cpus > 0 ? (int)cpus : 1;
#  ifdef __APPLE__
    char version[32], build[32];
    sysctl_string("kern.osproductversion", version, sizeof version);
    sysctl_string("kern.osversion", build, sizeof build);
    snprintf(m->os, sizeof m->os, "macOS %s (%s)", version, build);
    sysctl_string("hw.model", m->model, sizeof m->model);
    sysctl_string("machdep.cpu.brand_string", m->cpu, sizeof m->cpu);
    m->memory_bytes = (uint64_t)sysctl_int("hw.memsize");
    m->performance_cpus = (int)sysctl_int("hw.perflevel0.logicalcpu");
    m->efficiency_cpus = (int)sysctl_int("hw.perflevel1.logicalcpu");
    command_field("system_profiler SPDisplaysDataType 2>/dev/null", "Chipset Model", m->gpu, sizeof m->gpu);
    command_field("system_profiler SPDisplaysDataType 2>/dev/null", "Total Number of Cores", m->gpu_cores,
                  sizeof m->gpu_cores);
    command_field("system_profiler SPDisplaysDataType 2>/dev/null", "Resolution", m->display, sizeof m->display);
    FILE *p = popen("pmset -g batt 2>/dev/null", "r");
    if (p) {
        char line[160];
        if (fgets(line, sizeof line, p)) m->on_battery = strstr(line, "Battery Power") != NULL;
        pclose(p);
    }
#  else
    struct utsname u;
    if (uname(&u) == 0) snprintf(m->os, sizeof m->os, "%s %s", u.sysname, u.release);
    command_field("cat /etc/os-release 2>/dev/null", "PRETTY_NAME", m->model, sizeof m->model);
    command_field("cat /proc/cpuinfo 2>/dev/null", "model name", m->cpu, sizeof m->cpu);
    char mem[64] = "";
    if (command_field("cat /proc/meminfo 2>/dev/null", "MemTotal", mem, sizeof mem)) {
        m->memory_bytes = strtoull(mem, NULL, 10) * 1024ull;
    }
    command_field("lspci 2>/dev/null", "VGA compatible controller", m->gpu, sizeof m->gpu);
    FILE *ac = fopen("/sys/class/power_supply/AC/online", "r");
    if (ac) {
        int online;
        if (fscanf(ac, "%d", &online) == 1) m->on_battery = online == 0;
        fclose(ac);
    }
#  endif
#endif
}

static int compare_u32(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

static uint64_t workload(double seconds) {
    enum { BLOCK = 1 << 16, NUMS = 4096 };
    uint8_t *block = malloc(BLOCK);
    uint32_t *nums = malloc(sizeof(uint32_t) * NUMS);
    if (!block || !nums) {
        free(block);
        free(nums);
        return 0;
    }
    for (int i = 0; i < BLOCK; i++) block[i] = (uint8_t)i;
    uint8_t digest[32];
    uint64_t rounds = 0;
    double deadline = ojh_now() + seconds;
    while (ojh_now() < deadline) {
        ojh_sha256(block, BLOCK, digest);
        for (uint32_t i = 0; i < NUMS; i++) {
            nums[i] = (uint32_t)digest[i % 32] * 2654435761u + i * (uint32_t)digest[(i * 7) % 32];
        }
        qsort(nums, NUMS, sizeof nums[0], compare_u32);
        for (int i = 0; i < BLOCK; i++) block[i] ^= (uint8_t)(nums[i % NUMS] >> (i % 24));
        rounds++;
    }
    free(block);
    free(nums);
    return rounds;
}

typedef struct {
    double seconds;
    uint64_t rounds;
    ojh_thread thread;
} worker;

static void *run_worker(void *p) {
    worker *w = p;
    w->rounds = workload(w->seconds);
    return NULL;
}

void ojh_reference_measure(ojh_reference *r, double seconds) {
    ojh_machine m;
    ojh_machine_read(&m);
    r->seconds = seconds;
    r->single_core = (double)workload(seconds) / seconds;
    int cores = m.logical_cpus > 0 ? m.logical_cpus : 1;
    r->cores = cores;
    worker *workers = calloc((size_t)cores, sizeof *workers);
    uint64_t total = 0;
    if (workers) {
        int started = 0;
        for (int i = 0; i < cores; i++) {
            workers[i].seconds = seconds;
            if (ojh_thread_start(&workers[i].thread, run_worker, &workers[i]) != 0) break;
            started++;
        }
        for (int i = 0; i < started; i++) {
            ojh_thread_join(&workers[i].thread);
            total += workers[i].rounds;
        }
    }
    r->all_cores = (double)total / seconds;
    free(workers);
}

void ojh_machine_json(ojh_json *w, const ojh_machine *m, const ojh_reference *r) {
    ojh_json_object(w);
    ojh_json_key(w, "os"); ojh_json_string(w, m->os);
    ojh_json_key(w, "model"); ojh_json_string(w, m->model);
    ojh_json_key(w, "cpu"); ojh_json_string(w, m->cpu);
    ojh_json_key(w, "logical_cpus"); ojh_json_int(w, m->logical_cpus);
    ojh_json_key(w, "performance_cpus"); ojh_json_int(w, m->performance_cpus);
    ojh_json_key(w, "efficiency_cpus"); ojh_json_int(w, m->efficiency_cpus);
    ojh_json_key(w, "memory_bytes"); ojh_json_uint(w, m->memory_bytes);
    ojh_json_key(w, "gpu"); ojh_json_string(w, m->gpu);
    ojh_json_key(w, "gpu_cores"); ojh_json_string(w, m->gpu_cores);
    ojh_json_key(w, "display"); ojh_json_string(w, m->display);
    ojh_json_key(w, "on_battery");
    if (m->on_battery < 0) ojh_json_null(w);
    else ojh_json_bool(w, m->on_battery);
    if (r) {
        ojh_json_key(w, "reference");
        ojh_json_object(w);
        ojh_json_key(w, "workload"); ojh_json_string(w, "sha256 + sort + mix of a 64 KiB buffer (src/machine.c)");
        ojh_json_key(w, "seconds"); ojh_json_double(w, r->seconds, 1);
        ojh_json_key(w, "cores"); ojh_json_int(w, r->cores);
        ojh_json_key(w, "single_core_rounds_per_second"); ojh_json_double(w, r->single_core, 1);
        ojh_json_key(w, "all_cores_rounds_per_second"); ojh_json_double(w, r->all_cores, 1);
        ojh_json_end_object(w);
    }
    ojh_json_end_object(w);
}
