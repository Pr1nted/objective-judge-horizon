#ifndef OJH_PROCMETER_H
#define OJH_PROCMETER_H

#include "json.h"
#include "platform.h"

/* CPU and memory of a game while it runs, sampled from outside the process. It samples
   the process and every descendant (games often start a server, AI clients or a browser
   helper), so the figure is what the whole game costs the machine. */
typedef struct ojh_procmeter ojh_procmeter;

ojh_procmeter *ojh_procmeter_start(ojh_pid pid, double interval_seconds);
void ojh_procmeter_stop(ojh_procmeter *m);
int ojh_procmeter_samples(const ojh_procmeter *m);
void ojh_procmeter_json(ojh_json *w, const ojh_procmeter *m);
void ojh_procmeter_free(ojh_procmeter *m);

#endif
