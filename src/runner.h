#ifndef OJH_RUNNER_H
#define OJH_RUNNER_H

#include <stddef.h>

#include "platform.h"

enum { OJH_STDOUT = 1, OJH_STDERR = 2 };

typedef struct {
    double t;
    int stream;
    char *text;
} ojh_line;

typedef struct ojh_run ojh_run;

ojh_run *ojh_run_start(const char *const *argv, const char *const *env, const char *cwd);
ojh_run *ojh_run_start_with_input(const char *const *argv, const char *const *env, const char *cwd);
ojh_pid ojh_run_pid(const ojh_run *r);
double ojh_run_started(const ojh_run *r);
int ojh_run_write(ojh_run *r, const char *text);
void ojh_run_close_input(ojh_run *r);
int ojh_run_running(ojh_run *r);
size_t ojh_run_line_count_now(const ojh_run *r);
int ojh_run_copy_line(const ojh_run *r, size_t index, char *out, size_t n, double *t, int *stream);

int ojh_run_wait(ojh_run *r, double timeout_seconds);

size_t ojh_run_line_count(const ojh_run *r);
const ojh_line *ojh_run_line(const ojh_run *r, size_t index);
void ojh_run_free(ojh_run *r);

#endif
