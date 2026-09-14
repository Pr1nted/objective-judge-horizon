#ifndef _WIN32
#  define _DEFAULT_SOURCE
#  define _DARWIN_C_SOURCE
#endif
#include "platform.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#  include <psapi.h>
#  include <tlhelp32.h>
#else
#  include <errno.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <poll.h>
#  include <signal.h>
#  include <spawn.h>
#  include <sys/socket.h>
#  include <sys/wait.h>
#  include <time.h>
#  include <unistd.h>
#  ifdef __APPLE__
#    include <libproc.h>
#    include <mach-o/dyld.h>
#    include <mach/mach_time.h>
#    include <sys/resource.h>
#  else
#    include <dirent.h>
#  endif
extern char **environ;
#endif

/* ---------------------------------------------------------------- time */

double ojh_now(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    LARGE_INTEGER count;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

void ojh_sleep(double seconds) {
    if (seconds <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)(seconds * 1000.0 + 0.5));
#else
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
    }
#endif
}

/* ---------------------------------------------------------------- threads */

#ifdef _WIN32
typedef struct {
    void *(*fn)(void *);
    void *arg;
} trampoline;

static unsigned __stdcall thread_main(void *p) {
    trampoline t = *(trampoline *)p;
    free(p);
    t.fn(t.arg);
    return 0;
}

int ojh_thread_start(ojh_thread *t, void *(*fn)(void *), void *arg) {
    trampoline *tr = malloc(sizeof *tr);
    if (!tr) return -1;
    tr->fn = fn;
    tr->arg = arg;
    uintptr_t h = _beginthreadex(NULL, 0, thread_main, tr, 0, NULL);
    if (h == 0) {
        free(tr);
        return -1;
    }
    t->h = (HANDLE)h;
    return 0;
}

void ojh_thread_join(ojh_thread *t) {
    WaitForSingleObject(t->h, INFINITE);
    CloseHandle(t->h);
}

void ojh_mutex_init(ojh_mutex *m) { InitializeSRWLock(m); }
void ojh_mutex_lock(ojh_mutex *m) { AcquireSRWLockExclusive(m); }
void ojh_mutex_unlock(ojh_mutex *m) { ReleaseSRWLockExclusive(m); }
void ojh_mutex_destroy(ojh_mutex *m) { (void)m; }

void ojh_flag_init(ojh_flag *f, int value) { f->v = value; }
int ojh_flag_get(ojh_flag *f) { return (int)InterlockedCompareExchange(&f->v, 0, INT_MIN); }
int ojh_flag_exchange(ojh_flag *f, int value) { return (int)InterlockedExchange(&f->v, value); }
#else
int ojh_thread_start(ojh_thread *t, void *(*fn)(void *), void *arg) {
    return pthread_create(&t->t, NULL, fn, arg) == 0 ? 0 : -1;
}

void ojh_thread_join(ojh_thread *t) { pthread_join(t->t, NULL); }
void ojh_mutex_init(ojh_mutex *m) { pthread_mutex_init(m, NULL); }
void ojh_mutex_lock(ojh_mutex *m) { pthread_mutex_lock(m); }
void ojh_mutex_unlock(ojh_mutex *m) { pthread_mutex_unlock(m); }
void ojh_mutex_destroy(ojh_mutex *m) { pthread_mutex_destroy(m); }

void ojh_flag_init(ojh_flag *f, int value) { atomic_init(&f->v, value); }
int ojh_flag_get(ojh_flag *f) { return atomic_load(&f->v); }
int ojh_flag_exchange(ojh_flag *f, int value) { return atomic_exchange(&f->v, value); }
#endif

/* ---------------------------------------------------------------- sockets */

int ojh_net_init(void) {
#ifdef _WIN32
    static int started;
    if (started) return 0;
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return -1;
    started = 1;
#endif
    return 0;
}

void ojh_ignore_sigpipe(void) {
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
}

void ojh_sock_close(ojh_socket s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

static void tune(ojh_socket s) {
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
#ifdef SO_NOSIGPIPE
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
}

ojh_socket ojh_listen_loopback(uint16_t port, uint16_t *bound_port) {
    ojh_socket s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == OJH_INVALID_SOCKET) return s;
#ifndef _WIN32
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
#endif
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (bind(s, (struct sockaddr *)&addr, sizeof addr) != 0 || listen(s, 64) != 0) {
        ojh_sock_close(s);
        return OJH_INVALID_SOCKET;
    }
    if (bound_port) {
        socklen_t len = sizeof addr;
        *bound_port = getsockname(s, (struct sockaddr *)&addr, &len) == 0 ? ntohs(addr.sin_port) : port;
    }
    return s;
}

ojh_socket ojh_connect_tcp(const char *host, uint16_t port) {
    char service[8];
    snprintf(service, sizeof service, "%u", (unsigned)port);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(host, service, &hints, &res) != 0) return OJH_INVALID_SOCKET;
    ojh_socket s = OJH_INVALID_SOCKET;
    for (struct addrinfo *a = res; a; a = a->ai_next) {
        s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == OJH_INVALID_SOCKET) continue;
        if (connect(s, a->ai_addr, (int)a->ai_addrlen) == 0) break;
        ojh_sock_close(s);
        s = OJH_INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if (s != OJH_INVALID_SOCKET) tune(s);
    return s;
}

ojh_socket ojh_accept(ojh_socket listener) {
    ojh_socket c = accept(listener, NULL, NULL);
    if (c != OJH_INVALID_SOCKET) tune(c);
    return c;
}

int ojh_wait_readable(const ojh_socket *socks, int n, int timeout_ms, int *readable) {
    if (n < 1 || n > 8) return -1;
#ifdef _WIN32
    WSAPOLLFD fds[8];
#else
    struct pollfd fds[8];
#endif
    for (int i = 0; i < n; i++) {
        fds[i].fd = socks[i];
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }
#ifdef _WIN32
    int ready = WSAPoll(fds, (ULONG)n, timeout_ms);
#else
    int ready;
    do {
        ready = poll(fds, (nfds_t)n, timeout_ms);
    } while (ready < 0 && errno == EINTR);
#endif
    for (int i = 0; i < n; i++) {
        readable[i] = ready > 0 && (fds[i].revents & (POLLIN | POLLHUP | POLLERR)) != 0;
    }
    return ready;
}

long ojh_recv(ojh_socket s, uint8_t *buf, size_t len) {
#ifdef _WIN32
    int n = recv(s, (char *)buf, len > INT_MAX ? INT_MAX : (int)len, 0);
    return n == SOCKET_ERROR ? -1 : (long)n;
#else
    ssize_t n;
    do {
        n = recv(s, buf, len, 0);
    } while (n < 0 && errno == EINTR);
    return (long)n;
#endif
}

int ojh_send_all(ojh_socket s, const uint8_t *data, size_t len) {
    while (len > 0) {
#ifdef _WIN32
        int chunk = len > INT_MAX ? INT_MAX : (int)len;
        int n = send(s, (const char *)data, chunk, 0);
        if (n == SOCKET_ERROR) return -1;
#else
#  ifdef MSG_NOSIGNAL
        ssize_t n = send(s, data, len, MSG_NOSIGNAL);
#  else
        ssize_t n = send(s, data, len, 0);
#  endif
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
#endif
        data += n;
        len -= (size_t)n;
    }
    return 0;
}

/* ---------------------------------------------------------------- processes */

int ojh_self_path(char *out, size_t n) {
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, out, (DWORD)n);
    return (len > 0 && len < n) ? 0 : -1;
#elif defined(__APPLE__)
    uint32_t size = (uint32_t)n;
    return _NSGetExecutablePath(out, &size) == 0 ? 0 : -1;
#else
    ssize_t len = readlink("/proc/self/exe", out, n - 1);
    if (len <= 0) return -1;
    out[len] = '\0';
    return 0;
#endif
}

#ifdef _WIN32
/* One argument, quoted the way the Microsoft C runtime parses a command line. */
static int append_quoted(char *line, size_t cap, size_t *len, const char *arg) {
    size_t need = strlen(arg) * 2 + 3;
    if (*len + need + 1 > cap) return -1;
    if (*len > 0) line[(*len)++] = ' ';
    line[(*len)++] = '"';
    for (const char *p = arg;; p++) {
        size_t slashes = 0;
        while (*p == '\\') {
            p++;
            slashes++;
        }
        if (*p == '\0') {
            for (size_t i = 0; i < slashes * 2; i++) line[(*len)++] = '\\';
            break;
        }
        if (*p == '"') {
            for (size_t i = 0; i < slashes * 2 + 1; i++) line[(*len)++] = '\\';
        } else {
            for (size_t i = 0; i < slashes; i++) line[(*len)++] = '\\';
        }
        line[(*len)++] = *p;
    }
    line[(*len)++] = '"';
    line[*len] = '\0';
    return 0;
}
#endif

int ojh_spawn(const char *const *argv, ojh_process *p) {
#ifdef _WIN32
    char line[32768];
    size_t len = 0;
    line[0] = '\0';
    for (int i = 0; argv[i]; i++) {
        if (append_quoted(line, sizeof line, &len, argv[i]) != 0) return -1;
    }
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(NULL, line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return -1;
    CloseHandle(pi.hThread);
    p->pid = pi.dwProcessId;
    p->h = pi.hProcess;
    return 0;
#else
    return posix_spawn(&p->pid, argv[0], NULL, NULL, (char *const *)argv, environ) == 0 ? 0 : -1;
#endif
}

int ojh_wait(ojh_process *p) {
#ifdef _WIN32
    DWORD code = 0;
    WaitForSingleObject(p->h, INFINITE);
    int ok = GetExitCodeProcess(p->h, &code);
    CloseHandle(p->h);
    return ok ? (int)code : -1;
#else
    int status;
    if (waitpid(p->pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

#if defined(_WIN32) || !defined(__APPLE__)
typedef struct {
    ojh_pid pid, parent;
} pid_pair;

/* Breadth first from root over (pid, parent) pairs. */
static int tree_from_pairs(ojh_pid root, const pid_pair *pairs, int count, ojh_pid *out, int max) {
    int n = 0;
    out[n++] = root;
    for (int i = 0; i < n && n < max; i++) {
        for (int k = 0; k < count && n < max; k++) {
            if (pairs[k].parent == out[i] && pairs[k].pid != out[i]) {
                int seen = 0;
                for (int j = 0; j < n; j++) seen |= out[j] == pairs[k].pid;
                if (!seen) out[n++] = pairs[k].pid;
            }
        }
    }
    return n;
}
#endif

int ojh_process_tree(ojh_pid root, ojh_pid *out, int max) {
    if (max < 1) return 0;
#ifdef _WIN32
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        out[0] = root;
        return 1;
    }
    int cap = 1024, count = 0;
    pid_pair *pairs = malloc((size_t)cap * sizeof *pairs);
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof entry;
    for (BOOL more = Process32First(snap, &entry); more && pairs; more = Process32Next(snap, &entry)) {
        if (count == cap) {
            cap *= 2;
            pid_pair *grown = realloc(pairs, (size_t)cap * sizeof *grown);
            if (!grown) break;
            pairs = grown;
        }
        pairs[count].pid = entry.th32ProcessID;
        pairs[count].parent = entry.th32ParentProcessID;
        count++;
    }
    CloseHandle(snap);
    int n = pairs ? tree_from_pairs(root, pairs, count, out, max) : (out[0] = root, 1);
    free(pairs);
    return n;
#elif defined(__APPLE__)
    int n = 0;
    out[n++] = root;
    enum { KIDS = 512 };
    pid_t kids[KIDS];
    for (int i = 0; i < n && n < max; i++) {
        memset(kids, 0, sizeof kids);
        proc_listchildpids(out[i], kids, (int)sizeof kids);
        for (int k = 0; k < KIDS && n < max; k++) {
            if (kids[k] > 0) out[n++] = kids[k];
        }
    }
    return n;
#else
    DIR *proc = opendir("/proc");
    if (!proc) {
        out[0] = root;
        return 1;
    }
    int cap = 1024, count = 0;
    pid_pair *pairs = malloc((size_t)cap * sizeof *pairs);
    struct dirent *e;
    while (pairs && (e = readdir(proc))) {
        if (e->d_name[0] < '1' || e->d_name[0] > '9') continue;
        char path[300], buf[512];
        snprintf(path, sizeof path, "/proc/%s/stat", e->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        size_t len = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[len] = '\0';
        char *after = strrchr(buf, ')');
        int parent;
        if (!after || sscanf(after + 2, "%*c %d", &parent) != 1) continue;
        if (count == cap) {
            cap *= 2;
            pid_pair *grown = realloc(pairs, (size_t)cap * sizeof *grown);
            if (!grown) break;
            pairs = grown;
        }
        pairs[count].pid = (ojh_pid)atoi(e->d_name);
        pairs[count].parent = (ojh_pid)parent;
        count++;
    }
    closedir(proc);
    int n = pairs ? tree_from_pairs(root, pairs, count, out, max) : (out[0] = root, 1);
    free(pairs);
    return n;
#endif
}

int ojh_process_usage(ojh_pid pid, uint64_t *memory_bytes, uint64_t *cpu_ns) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return 0;
    DWORD code = 0;
    if (!GetExitCodeProcess(h, &code) || code != STILL_ACTIVE) {
        CloseHandle(h);
        return 0;
    }
    FILETIME created, exited, kernel, user;
    PROCESS_MEMORY_COUNTERS_EX pmc;
    memset(&pmc, 0, sizeof pmc);
    pmc.cb = sizeof pmc;
    int ok = GetProcessTimes(h, &created, &exited, &kernel, &user) &&
             GetProcessMemoryInfo(h, (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof pmc);
    CloseHandle(h);
    if (!ok) return 0;
    uint64_t k = ((uint64_t)kernel.dwHighDateTime << 32) | kernel.dwLowDateTime;
    uint64_t u = ((uint64_t)user.dwHighDateTime << 32) | user.dwLowDateTime;
    *cpu_ns = (k + u) * 100u;
    *memory_bytes = (uint64_t)pmc.PrivateUsage;
    return 1;
#elif defined(__APPLE__)
    struct rusage_info_v2 ri;
    if (proc_pid_rusage(pid, RUSAGE_INFO_V2, (rusage_info_t *)&ri) != 0) return 0;
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0) mach_timebase_info(&tb);
    *memory_bytes = ri.ri_phys_footprint;
    *cpu_ns = (ri.ri_user_time + ri.ri_system_time) * tb.numer / tb.denom;
    return 1;
#else
    char path[64], buf[1024];
    snprintf(path, sizeof path, "/proc/%d/stat", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t len = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[len] = '\0';
    char *after = strrchr(buf, ')');
    if (!after) return 0;
    unsigned long utime = 0, stime = 0;
    long rss_pages = 0;
    /* after the command: state(3) ppid pgrp session tty tpgid flags minflt cminflt majflt
       cmajflt utime(14) stime(15) cutime cstime priority nice threads itrealvalue
       starttime vsize rss(24) */
    if (sscanf(after + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %*d %*d %*u %*u %ld",
               &utime, &stime, &rss_pages) != 3) {
        return 0;
    }
    long ticks = sysconf(_SC_CLK_TCK);
    *cpu_ns = (uint64_t)((double)(utime + stime) / (double)(ticks > 0 ? ticks : 100) * 1e9);
    *memory_bytes = (uint64_t)rss_pages * (uint64_t)sysconf(_SC_PAGESIZE);
    return 1;
#endif
}
