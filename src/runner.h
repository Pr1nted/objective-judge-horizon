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
ojh_pid ojh_run_pid(const ojh_run *r);

int ojh_run_wait(ojh_run *r, double timeout_seconds);

size_t ojh_run_line_count(const ojh_run *r);
const ojh_line *ojh_run_line(const ojh_run *r, size_t index);
void ojh_run_free(ojh_run *r);

#endif
