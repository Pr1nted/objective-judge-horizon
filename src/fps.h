#ifndef OJH_FPS_H
#define OJH_FPS_H

#include <stddef.h>

#include "gamespec.h"
#include "json.h"
#include "tpm.h"

#define OJH_FPS_MAX_SCENES 16

typedef struct {
    char name[32];
    int frames;
    double seconds;
    double p50_ms, p95_ms, p99_ms;
    double low1_fps;
} ojh_fps_scene;

typedef struct {
    char name[32];
    char reason[240];
} ojh_fps_missing;

typedef struct {
    char id[64];
    char name[128];
    char version[64];
    int exit_code;
    char renderer[240];
    char resolution[32];
    char vsync[16];
    ojh_fps_scene scenes[OJH_FPS_MAX_SCENES];
    int scene_count;
    ojh_fps_missing missing[OJH_FPS_MAX_SCENES];
    int missing_count;
    char how[900];
} ojh_fps;

void ojh_fps_parse_line(ojh_fps *f, const char *text);
double ojh_fps_map_average(const ojh_fps *f);
double ojh_fps_worst_low(const ojh_fps *f);
double ojh_fps_worst_p99_seconds(const ojh_fps *f);
int ojh_fps_run(ojh_game game, const ojh_tpm_options *options, double seconds_per_scene, ojh_fps *out, char *error,
                size_t error_len);
int ojh_fps_run_spec(const ojh_gamespec *spec, const ojh_tpm_options *options, double seconds_per_scene, ojh_fps *out,
                     char *error, size_t error_len);
void ojh_fps_json(ojh_json *w, const ojh_fps *f);

#endif
