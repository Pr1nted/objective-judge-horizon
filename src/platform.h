#ifndef OJH_PLATFORM_H
#define OJH_PLATFORM_H

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

double ojh_now(void);
void ojh_sleep(double seconds);

int ojh_thread_start(ojh_thread *t, void *(*fn)(void *), void *arg);
void ojh_thread_join(ojh_thread *t);
void ojh_mutex_init(ojh_mutex *m);
void ojh_mutex_lock(ojh_mutex *m);
void ojh_mutex_unlock(ojh_mutex *m);
void ojh_mutex_destroy(ojh_mutex *m);
void ojh_flag_init(ojh_flag *f, int value);
int ojh_flag_get(ojh_flag *f);
int ojh_flag_exchange(ojh_flag *f, int value);

int ojh_net_init(void);
void ojh_ignore_sigpipe(void);
void ojh_sock_close(ojh_socket s);
ojh_socket ojh_listen_loopback(uint16_t port, uint16_t *bound_port);
ojh_socket ojh_connect_tcp(const char *host, uint16_t port);
ojh_socket ojh_accept(ojh_socket listener);
int ojh_wait_readable(const ojh_socket *socks, int n, int timeout_ms, int *readable);
long ojh_recv(ojh_socket s, uint8_t *buf, size_t len);
int ojh_send_all(ojh_socket s, const uint8_t *data, size_t len);

int ojh_self_path(char *out, size_t n);
int ojh_spawn(const char *const *argv, ojh_process *p);
int ojh_wait(ojh_process *p);
#ifdef _WIN32
int ojh_command_line(const char *const *argv, char *out, size_t cap);
#endif
int ojh_make_dir(const char *path);
int ojh_list_dir(const char *dir, void (*fn)(const char *name, void *user), void *user);
int ojh_process_tree(ojh_pid root, ojh_pid *out, int max);
int ojh_process_usage(ojh_pid pid, uint64_t *memory_bytes, uint64_t *cpu_ns);

#endif
