#ifndef _WIN32
#  define _DEFAULT_SOURCE
#  define _DARWIN_C_SOURCE
#endif
#include "platform.h"
#include "runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#  include <errno.h>
#  include <fcntl.h>
#  include <poll.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <unistd.h>
extern char **environ;
#endif

#define LINE_BYTES (1 << 16)

struct ojh_run {
    ojh_pid pid;
#ifdef _WIN32
    HANDLE process;
    HANDLE job;
    HANDLE input;
#else
    int input;
#endif
    int reaped;
    int status;
    double started;
    ojh_flag stop;
    ojh_flag done[2];
    ojh_thread readers[2];
    int reader_started[2];
    int waited;
    ojh_mutex lock;
    ojh_line *lines;
    size_t count, cap;
};

typedef struct {
    ojh_run *r;
    int index;
#ifdef _WIN32
    HANDLE h;
#else
    int fd;
#endif
} reader_arg;

static void add_line(ojh_run *r, int stream, const char *text, size_t len) {
    while (len > 0 && text[len - 1] == '\r') len--;
    char *copy = malloc(len + 1);
    if (!copy) return;
    memcpy(copy, text, len);
    copy[len] = '\0';
    double t = ojh_now() - r->started;
    ojh_mutex_lock(&r->lock);
    if (r->count == r->cap) {
        size_t cap = r->cap ? r->cap * 2 : 1024;
        ojh_line *grown = realloc(r->lines, cap * sizeof *grown);
        if (grown) {
            r->lines = grown;
            r->cap = cap;
        }
    }
    if (r->count < r->cap) {
        r->lines[r->count].t = t;
        r->lines[r->count].stream = stream;
        r->lines[r->count].text = copy;
        r->count++;
        copy = NULL;
    }
    ojh_mutex_unlock(&r->lock);
    free(copy);
}

static void *reader(void *p) {
    reader_arg a = *(reader_arg *)p;
    free(p);
    int stream = a.index == 0 ? OJH_STDOUT : OJH_STDERR;
    char *line = malloc(LINE_BYTES);
    char chunk[4096];
    size_t used = 0;
    while (line) {
        long n;
#ifdef _WIN32
        DWORD available = 0;
        if (!PeekNamedPipe(a.h, NULL, 0, NULL, &available, NULL)) break;
        if (available == 0) {
            if (ojh_flag_get(&a.r->stop)) break;
            Sleep(2);
            continue;
        }
        DWORD got = 0;
        DWORD want = available < sizeof chunk ? available : (DWORD)sizeof chunk;
        if (!ReadFile(a.h, chunk, want, &got, NULL) || got == 0) break;
        n = (long)got;
#else
        struct pollfd pfd = {a.fd, POLLIN, 0};
        int ready = poll(&pfd, 1, 50);
        if (ready == 0) {
            if (ojh_flag_get(&a.r->stop)) break;
            continue;
        }
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }
        n = (long)read(a.fd, chunk, sizeof chunk);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
#endif
        for (long i = 0; i < n; i++) {
            if (chunk[i] == '\n') {
                add_line(a.r, stream, line, used);
                used = 0;
            } else {
                if (used == LINE_BYTES) {
                    add_line(a.r, stream, line, used);
                    used = 0;
                }
                line[used++] = chunk[i];
            }
        }
    }
    if (line && used > 0) add_line(a.r, stream, line, used);
    free(line);
#ifdef _WIN32
    CloseHandle(a.h);
#else
    close(a.fd);
#endif
    ojh_flag_exchange(&a.r->done[a.index], 1);
    return NULL;
}

static int start_readers(ojh_run *r,
#ifdef _WIN32
                         HANDLE out, HANDLE err
#else
                         int out, int err
#endif
) {
    for (int i = 0; i < 2; i++) {
        reader_arg *a = malloc(sizeof *a);
        if (!a) return -1;
        a->r = r;
        a->index = i;
#ifdef _WIN32
        a->h = i == 0 ? out : err;
#else
        a->fd = i == 0 ? out : err;
#endif
        if (ojh_thread_start(&r->readers[i], reader, a) != 0) {
            free(a);
            return -1;
        }
        r->reader_started[i] = 1;
    }
    return 0;
}

static int same_name(const char *a, const char *b) {
    while (*a && *a != '=' && *a == *b) {
        a++;
        b++;
    }
    return (*a == '=' || *a == '\0') && (*b == '=' || *b == '\0');
}

static int overridden(const char *entry, const char *const *env) {
    for (size_t k = 0; env && env[k]; k++) {
        if (same_name(entry, env[k])) return 1;
    }
    return 0;
}

#ifdef _WIN32
static ojh_run *start_run(const char *const *argv, const char *const *env, const char *cwd, int with_input) {
    char command[32768];
    if (ojh_command_line(argv, command, sizeof command) != 0) return NULL;

    char *inherited = GetEnvironmentStringsA();
    size_t size = 2;
    for (char *e = inherited; e && *e; e += strlen(e) + 1) size += strlen(e) + 1;
    for (size_t k = 0; env && env[k]; k++) size += strlen(env[k]) + 1;
    char *block = malloc(size);
    if (!block) {
        if (inherited) FreeEnvironmentStringsA(inherited);
        return NULL;
    }
    size_t at = 0;
    for (char *e = inherited; e && *e; e += strlen(e) + 1) {
        if (overridden(e, env)) continue;
        size_t len = strlen(e) + 1;
        memcpy(block + at, e, len);
        at += len;
    }
    for (size_t k = 0; env && env[k]; k++) {
        size_t len = strlen(env[k]) + 1;
        memcpy(block + at, env[k], len);
        at += len;
    }
    block[at++] = '\0';
    if (at == 1) block[at++] = '\0';
    if (inherited) FreeEnvironmentStringsA(inherited);

    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE out_r = NULL, out_w = NULL, err_r = NULL, err_w = NULL;
    if (!CreatePipe(&out_r, &out_w, &sa, 0) || !CreatePipe(&err_r, &err_w, &sa, 0)) {
        free(block);
        return NULL;
    }
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);
    HANDLE in_r = NULL, in_w = NULL;
    if (with_input && CreatePipe(&in_r, &in_w, &sa, 0)) SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_r ? in_r : nul;
    si.hStdOutput = out_w;
    si.hStdError = err_w;

    ojh_run *r = calloc(1, sizeof *r);
    if (!r) {
        free(block);
        return NULL;
    }
    ojh_mutex_init(&r->lock);
    ojh_flag_init(&r->stop, 0);
    ojh_flag_init(&r->done[0], 0);
    ojh_flag_init(&r->done[1], 0);
    r->started = ojh_now();
    BOOL ok = CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW, block, cwd, &si, &pi);
    free(block);
    CloseHandle(out_w);
    CloseHandle(err_w);
    if (in_r) CloseHandle(in_r);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) {
        if (in_w) CloseHandle(in_w);
        CloseHandle(out_r);
        CloseHandle(err_r);
        ojh_mutex_destroy(&r->lock);
        free(r);
        return NULL;
    }
    r->job = CreateJobObjectA(NULL, NULL);
    if (r->job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
        memset(&limits, 0, sizeof limits);
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(r->job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
        AssignProcessToJobObject(r->job, pi.hProcess);
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    r->process = pi.hProcess;
    r->pid = pi.dwProcessId;
    r->input = in_w;
    if (start_readers(r, out_r, err_r) != 0) {
        ojh_flag_exchange(&r->stop, 1);
    }
    return r;
}
#else
static ojh_run *start_run(const char *const *argv, const char *const *env, const char *cwd, int with_input) {
    int out[2] = {-1, -1}, err[2] = {-1, -1}, in[2] = {-1, -1};
    if (pipe(out) != 0) return NULL;
    if (pipe(err) != 0) {
        close(out[0]);
        close(out[1]);
        return NULL;
    }
    if (with_input && pipe(in) != 0) in[0] = in[1] = -1;
    size_t base = 0, extra = 0;
    while (environ && environ[base]) base++;
    while (env && env[extra]) extra++;
    char **envp = calloc(base + extra + 1, sizeof *envp);
    ojh_run *r = calloc(1, sizeof *r);
    if (!envp || !r) {
        free(envp);
        free(r);
        close(out[0]); close(out[1]); close(err[0]); close(err[1]);
        return NULL;
    }
    size_t n = 0;
    for (size_t i = 0; i < base; i++) {
        if (!overridden(environ[i], env)) envp[n++] = environ[i];
    }
    for (size_t k = 0; k < extra; k++) envp[n++] = (char *)env[k];
    envp[n] = NULL;

    ojh_mutex_init(&r->lock);
    ojh_flag_init(&r->stop, 0);
    ojh_flag_init(&r->done[0], 0);
    ojh_flag_init(&r->done[1], 0);
    r->started = ojh_now();
    pid_t pid = fork();
    if (pid < 0) {
        free(envp);
        close(out[0]); close(out[1]); close(err[0]); close(err[1]);
        ojh_mutex_destroy(&r->lock);
        free(r);
        return NULL;
    }
    if (pid == 0) {
        setpgid(0, 0);
        dup2(out[1], STDOUT_FILENO);
        dup2(err[1], STDERR_FILENO);
        close(out[0]); close(out[1]); close(err[0]); close(err[1]);
        if (in[0] >= 0) {
            dup2(in[0], STDIN_FILENO);
            close(in[0]);
            close(in[1]);
        } else {
            int devnull = open("/dev/null", O_RDONLY);
            if (devnull >= 0) {
                dup2(devnull, STDIN_FILENO);
                close(devnull);
            }
        }
        if (cwd && chdir(cwd) != 0) _exit(126);
        environ = envp;
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    setpgid(pid, pid);
    free(envp);
    close(out[1]);
    close(err[1]);
    if (in[0] >= 0) close(in[0]);
    if (in[1] >= 0) fcntl(in[1], F_SETFD, FD_CLOEXEC);
    r->input = in[1];
    r->pid = pid;
    if (start_readers(r, out[0], err[0]) != 0) {
        ojh_flag_exchange(&r->stop, 1);
    }
    return r;
}
#endif

ojh_run *ojh_run_start(const char *const *argv, const char *const *env, const char *cwd) {
    return start_run(argv, env, cwd, 0);
}

ojh_run *ojh_run_start_with_input(const char *const *argv, const char *const *env, const char *cwd) {
    return start_run(argv, env, cwd, 1);
}

ojh_pid ojh_run_pid(const ojh_run *r) { return r->pid; }

double ojh_run_started(const ojh_run *r) { return r->started; }

int ojh_run_write(ojh_run *r, const char *text) {
    size_t len = strlen(text);
#ifdef _WIN32
    if (!r->input) return -1;
    while (len > 0) {
        DWORD wrote = 0;
        if (!WriteFile(r->input, text, (DWORD)len, &wrote, NULL)) return -1;
        text += wrote;
        len -= wrote;
    }
#else
    if (r->input < 0) return -1;
    while (len > 0) {
        ssize_t n = write(r->input, text, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        text += n;
        len -= (size_t)n;
    }
#endif
    return 0;
}

void ojh_run_close_input(ojh_run *r) {
#ifdef _WIN32
    if (r->input) {
        CloseHandle(r->input);
        r->input = NULL;
    }
#else
    if (r->input >= 0) {
        close(r->input);
        r->input = -1;
    }
#endif
}

int ojh_run_running(ojh_run *r) {
    if (r->waited) return 0;
#ifdef _WIN32
    return WaitForSingleObject(r->process, 0) == WAIT_TIMEOUT;
#else
    if (r->reaped) return 0;
    int status;
    pid_t w = waitpid(r->pid, &status, WNOHANG);
    if (w == r->pid) {
        r->reaped = 1;
        r->status = status;
        return 0;
    }
    return w == 0;
#endif
}

int ojh_run_copy_line(const ojh_run *r, size_t index, char *out, size_t n, double *t, int *stream) {
    ojh_run *m = (ojh_run *)r;
    int found = 0;
    ojh_mutex_lock(&m->lock);
    if (index < m->count) {
        snprintf(out, n, "%s", m->lines[index].text);
        if (t) *t = m->lines[index].t;
        if (stream) *stream = m->lines[index].stream;
        found = 1;
    }
    ojh_mutex_unlock(&m->lock);
    return found;
}

size_t ojh_run_line_count_now(const ojh_run *r) {
    ojh_run *m = (ojh_run *)r;
    ojh_mutex_lock(&m->lock);
    size_t count = m->count;
    ojh_mutex_unlock(&m->lock);
    return count;
}

int ojh_run_wait(ojh_run *r, double timeout_seconds) {
    if (r->waited) return -1;
    r->waited = 1;
    int code = -1, timed_out = 0;
#ifdef _WIN32
    DWORD ms = timeout_seconds > 0 ? (DWORD)(timeout_seconds * 1000.0) : INFINITE;
    if (WaitForSingleObject(r->process, ms) == WAIT_TIMEOUT) {
        timed_out = 1;
        if (r->job) TerminateJobObject(r->job, 124);
        else TerminateProcess(r->process, 124);
        WaitForSingleObject(r->process, 10000);
    }
    DWORD exit_code;
    if (GetExitCodeProcess(r->process, &exit_code)) code = (int)exit_code;
#else
    double deadline = ojh_now() + timeout_seconds;
    int status;
    for (;;) {
        pid_t w = r->reaped ? r->pid : waitpid(r->pid, &status, WNOHANG);
        if (r->reaped) status = r->status;
        if (w == r->pid) {
            if (WIFEXITED(status)) code = WEXITSTATUS(status);
            else if (WIFSIGNALED(status)) code = 128 + WTERMSIG(status);
            break;
        }
        if (w < 0 && errno != EINTR) break;
        if (timeout_seconds > 0 && ojh_now() >= deadline) {
            timed_out = 1;
            kill(-r->pid, SIGTERM);
            double grace = ojh_now() + 3.0;
            while (waitpid(r->pid, &status, WNOHANG) == 0 && ojh_now() < grace) ojh_sleep(0.02);
            kill(-r->pid, SIGKILL);
            waitpid(r->pid, &status, 0);
            break;
        }
        ojh_sleep(0.005);
    }
    if (timed_out) kill(-r->pid, SIGKILL);
#endif
    double grace = ojh_now() + 2.0;
    while (!(ojh_flag_get(&r->done[0]) && ojh_flag_get(&r->done[1])) && ojh_now() < grace) ojh_sleep(0.005);
    ojh_flag_exchange(&r->stop, 1);
    for (int i = 0; i < 2; i++) {
        if (r->reader_started[i]) {
            ojh_thread_join(&r->readers[i]);
            r->reader_started[i] = 0;
        }
    }
    return timed_out ? -2 : code;
}

size_t ojh_run_line_count(const ojh_run *r) { return r->count; }

const ojh_line *ojh_run_line(const ojh_run *r, size_t index) {
    return index < r->count ? &r->lines[index] : NULL;
}

void ojh_run_free(ojh_run *r) {
    if (!r) return;
    if (!r->waited) ojh_run_wait(r, 0.001);
    for (size_t i = 0; i < r->count; i++) free(r->lines[i].text);
    free(r->lines);
    ojh_run_close_input(r);
#ifdef _WIN32
    if (r->process) CloseHandle(r->process);
    if (r->job) CloseHandle(r->job);
#endif
    ojh_mutex_destroy(&r->lock);
    free(r);
}
