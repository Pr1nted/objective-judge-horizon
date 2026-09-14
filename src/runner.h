#ifndef OJH_RUNNER_H
#define OJH_RUNNER_H

#include <stddef.h>

#include "platform.h"

/* Runs a game's program and keeps everything it prints, one line at a time, each line
   stamped with the moment it arrived. Game drivers read turn boundaries and timings out
   of these lines. The child gets extra environment variables and its own working
   directory, and a timeout ends it along with every process it started. */

enum { OJH_STDOUT = 1, OJH_STDERR = 2 };

typedef struct {
    double t;    /* seconds after the program started */
    int stream;  /* OJH_STDOUT or OJH_STDERR */
    char *text;  /* the line, without its line ending */
} ojh_line;

typedef struct ojh_run ojh_run;

/* argv and env end with NULL; env entries are "NAME=value" and replace inherited ones of
   the same name. env and cwd may be NULL. Returns NULL if the program cannot start. */
ojh_run *ojh_run_start(const char *const *argv, const char *const *env, const char *cwd);
ojh_pid ojh_run_pid(const ojh_run *r);

/* Waits for the program to exit, at most timeout_seconds (0 waits forever). On timeout
   the program and its descendants are ended. Returns the exit code, -2 on timeout, -1 on
   error. Every line printed before exit has been collected when this returns. */
int ojh_run_wait(ojh_run *r, double timeout_seconds);

size_t ojh_run_line_count(const ojh_run *r);
const ojh_line *ojh_run_line(const ojh_run *r, size_t index);
void ojh_run_free(ojh_run *r);

#endif
