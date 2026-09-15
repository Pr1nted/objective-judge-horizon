#ifndef OJH_TPM_H
#define OJH_TPM_H

#include <stddef.h>

#include "gamespec.h"
#include "json.h"
#include "procmeter.h"
#include "runner.h"

typedef enum {
    OJH_GAME_OPENDOCTRINES,
    OJH_GAME_GD5,
    OJH_GAME_FREECIV,
    OJH_GAME_UNCIV,
    OJH_GAME_COUNT,
    OJH_GAME_CUSTOM = 1000
} ojh_game;

const char *ojh_game_id(ojh_game g);
const char *ojh_game_name(ojh_game g);
int ojh_game_parse(const char *id, ojh_game *out);

typedef struct {
    const char *od_server;
    const char *od_data;
    const char *gd5_python;
    const char *gd5_dir;
    const char *freeciv_server;
    const char *unciv_jar;
    const char *java;
    const char *javac;
    const char *jar_tool;
    const char *drivers_dir;
    const char *work_dir;
    const char *ojh_path;
    const char *od_save;
    const char *od_game;
    const char *freeciv_prefix;
    const char *gd5_scenario;
    const char *od_map;
    const char *map_size;
    int turns;
    unsigned seed;
    int players;
    double timeout_seconds;
} ojh_tpm_options;

typedef struct {
    ojh_game game;
    char id[64];
    char name[128];
    char version[64];
    char license[64];
    char homepage[256];
    int exit_code;
    int turns;
    int timed_turns;
    double *turn_seconds;
    double play_seconds;
    double boot_seconds;
    double wall_seconds;
    int players;
    long regions;
    char region_kind[16];
    char how[320];
    int has_resources;
    ojh_procmeter_summary run_resources;
    ojh_procmeter_summary turn_resources;
} ojh_tpm;

int ojh_tpm_parse(ojh_game game, const ojh_line *lines, size_t count, ojh_tpm *out);
int ojh_gd5_tool(const char *gd5_dir, char *out, size_t n);

int ojh_tpm_run(ojh_game game, const ojh_tpm_options *options, ojh_tpm *out, char *error, size_t error_len);

int ojh_tpm_parse_spec(const ojh_gamespec *spec, const ojh_line *lines, size_t count, ojh_tpm *out);
int ojh_tpm_run_spec(const ojh_gamespec *spec, const ojh_tpm_options *options, ojh_tpm *out, char *error,
                     size_t error_len);

double ojh_tpm_value(const ojh_tpm *t);
void ojh_tpm_json(ojh_json *w, const ojh_tpm *t);
void ojh_tpm_free(ojh_tpm *t);

#endif
