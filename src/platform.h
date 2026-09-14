#ifndef OJH_PLATFORM_H
#define OJH_PLATFORM_H

/* Everything that differs between Windows, macOS and Linux lives behind this header:
   the clock, threads, locks, sockets, starting processes, and reading a process's CPU
   and memory. The rest of OJH is plain C11. */

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0601
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
typedef SOCKET ojh_socket;
#  define OJH_INVALID_SOCKET INVALID_SOCKET
typedef DWORD ojh_pid;
typedef struct { HANDLE h; } ojh_thread;
typedef SRWLOCK ojh_mutex;
typedef struct { LONG v; } ojh_flag;
typedef struct { ojh_pid pid; HANDLE h; } ojh_process;
#else
#  include <pthread.h>
#  include <stdatomic.h>
#  include <sys/types.h>
typedef int ojh_socket;
#  define OJH_INVALID_SOCKET (-1)
typedef pid_t ojh_pid;
typedef struct { pthread_t t; } ojh_thread;
typedef pthread_mutex_t ojh_mutex;
typedef struct { atomic_int v; } ojh_flag;
typedef struct { ojh_pid pid; } ojh_process;
#endif

/* ---- time */
double ojh_now(void);            /* seconds on a monotonic clock, for durations only */
void ojh_sleep(double seconds);

/* ---- threads, locks and a shared flag */
int ojh_thread_start(ojh_thread *t, void *(*fn)(void *), void *arg); /* 0 on success */
void ojh_thread_join(ojh_thread *t);
void ojh_mutex_init(ojh_mutex *m);
void ojh_mutex_lock(ojh_mutex *m);
void ojh_mutex_unlock(ojh_mutex *m);
void ojh_mutex_destroy(ojh_mutex *m);
void ojh_flag_init(ojh_flag *f, int value);
int ojh_flag_get(ojh_flag *f);
int ojh_flag_exchange(ojh_flag *f, int value); /* returns the previous value */

/* ---- sockets (TCP) */
int ojh_net_init(void);          /* call once at startup; 0 on success */
void ojh_ignore_sigpipe(void);   /* a closed peer must not kill the process */
void ojh_sock_close(ojh_socket s);
ojh_socket ojh_listen_loopback(uint16_t port, uint16_t *bound_port); /* port 0: any free port */
ojh_socket ojh_connect_tcp(const char *host, uint16_t port);
ojh_socket ojh_accept(ojh_socket listener);
/* Waits up to timeout_ms for any of n sockets (at most 8) to be readable or closed.
   readable[i] is set for each. Returns how many are ready, 0 on timeout, -1 on error. */
int ojh_wait_readable(const ojh_socket *socks, int n, int timeout_ms, int *readable);
long ojh_recv(ojh_socket s, uint8_t *buf, size_t len); /* bytes, 0 when closed, -1 on error */
int ojh_send_all(ojh_socket s, const uint8_t *data, size_t len); /* 0 on success */

/* ---- processes */
int ojh_self_path(char *out, size_t n);                        /* this executable; 0 on success */
int ojh_spawn(const char *const *argv, ojh_process *p);        /* argv ends with NULL; 0 on success */
int ojh_wait(ojh_process *p);                                  /* the exit code, -1 on error */
/* The process and all its descendants, root first. Returns how many were written. */
int ojh_process_tree(ojh_pid root, ojh_pid *out, int max);
/* Private memory in bytes and CPU time (user + system) in nanoseconds. 0 if the
   process is gone or unreadable, 1 otherwise. */
int ojh_process_usage(ojh_pid pid, uint64_t *memory_bytes, uint64_t *cpu_ns);

#endif
