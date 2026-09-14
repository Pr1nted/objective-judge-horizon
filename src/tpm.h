#ifndef OJH_TPM_H
#define OJH_TPM_H

#include <stddef.h>

#include "gamespec.h"
#include "json.h"
#include "procmeter.h"
#include "runner.h"

/* TPM, turns per minute: how many complete turns a game plays in a minute with every
   player controlled by its own AI. Each game is run through its own program and timed
   from what it prints; start-up and world generation are timed separately and are not
   part of the turns. */

typedef enum {
    OJH_GAME_OPENDOCTRINES,
    OJH_GAME_GD5,
    OJH_GAME_FREECIV,
    OJH_GAME_UNCIV,
    OJH_GAME_COUNT,
    OJH_GAME_CUSTOM = 1000 /* a game described by a spec file (gamespec.h) */
} ojh_game;

const char *ojh_game_id(ojh_game g);   /* "opendoctrines", "gd5", "freeciv", "unciv"; "custom" */
const char *ojh_game_name(ojh_game g); /* "Open Doctrines", ... */
int ojh_game_parse(const char *id, ojh_game *out); /* 0 when the id is known */

typedef struct {
    const char *od_server;      /* OpenDoctrinesServer, a Release build */
    const char *od_data;        /* its data directory */
    const char *gd5_python;     /* a Python with GD5's requirements installed */
    const char *gd5_dir;        /* the Greater Diplomacy 5 checkout */
    const char *freeciv_server; /* freeciv-server */
    const char *unciv_jar;      /* Unciv.jar */
    const char *java;           /* java, to run Unciv */
    const char *javac;          /* javac, to build OJH's Unciv driver */
    const char *jar_tool;       /* jar, to extract Unciv's rulesets */
    const char *drivers_dir;    /* OJH's drivers/ folder */
    const char *work_dir;       /* scratch space for scripts, saves and the compiled driver */
    const char *ojh_path;       /* this executable, for a spec's {ojh} */
    int turns;
    unsigned seed;
    int players;                /* where the game lets it be chosen */
    double timeout_seconds;
} ojh_tpm_options;

typedef struct {
    ojh_game game;
    char id[64];            /* the game's id and name: built in, or from its spec */
    char name[128];
    char version[64];       /* from a spec; empty when not given */
    char license[64];
    char homepage[256];
    int exit_code;
    int turns;              /* turns timed */
    int timed_turns;        /* how many of them have their own time (0: the game gives an average) */
    double *turn_seconds;   /* [timed_turns] */
    double play_seconds;    /* time spent in those turns */
    double boot_seconds;    /* program start to the first turn; -1 when unknown */
    double wall_seconds;    /* program start to exit */
    int players;            /* 0 when the game does not say */
    long regions;           /* provinces or tiles; 0 when the game does not say */
    char region_kind[16];
    char how[320];          /* what was timed, in words */
    int has_resources;      /* CPU and memory were sampled while the game ran */
    ojh_procmeter_summary run_resources;  /* the whole run */
    ojh_procmeter_summary turn_resources; /* only while turns were being played */
} ojh_tpm;

/* Reads a finished run's lines into the result. Returns 0 when turns were found. */
int ojh_tpm_parse(ojh_game game, const ojh_line *lines, size_t count, ojh_tpm *out);

/* Runs the game for options->turns and times it. Returns 0 on success; otherwise the
   reason is in error. */
int ojh_tpm_run(ojh_game game, const ojh_tpm_options *options, ojh_tpm *out, char *error, size_t error_len);

/* The same for a game described by a spec. Lines are read the way the spec says: OJH's
   protocol lines ("OJH ready", "OJH turn N [seconds]", "OJH players N", "OJH regions N
   kind"), or the gaps between marker lines. options gives turns, seed, players, timeout
   (0 takes the spec's), work_dir, and ojh_path for {ojh}. */
int ojh_tpm_parse_spec(const ojh_gamespec *spec, const ojh_line *lines, size_t count, ojh_tpm *out);
int ojh_tpm_run_spec(const ojh_gamespec *spec, const ojh_tpm_options *options, ojh_tpm *out, char *error,
                     size_t error_len);

double ojh_tpm_value(const ojh_tpm *t); /* turns per minute of play; 0 when nothing was timed */
void ojh_tpm_json(ojh_json *w, const ojh_tpm *t);
void ojh_tpm_free(ojh_tpm *t);

#endif
