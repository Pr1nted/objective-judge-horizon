#ifndef OJH_STATS_H
#define OJH_STATS_H

#include "chart.h"
#include "jsonread.h"

/* Every statistic OJH reports, in one catalogue: where it lives in a result file, its
   unit, and whether more is better, less is better, or neither (shown, never ranked).
   The report, the rankings, the graphs and the score all read statistics through this
   list, so a new statistic is one entry here. */

typedef enum {
    OJH_METRIC_TPM,       /* ojh tpm: turn speed, and CPU and memory while the turns ran */
    OJH_METRIC_FPS,       /* ojh fps: frame rate in the same scenes in every game */
    OJH_METRIC_NET,       /* ojh net: netcode traffic through the loopback relay */
    OJH_METRIC_FOOTPRINT, /* ojh footprint: install size, save size, save and load time */
    OJH_METRIC_COUNT
} ojh_metric;

const char *ojh_metric_id(ojh_metric m);  /* "tpm", "fps", "net", "footprint" */
const char *ojh_metric_name(ojh_metric m);
int ojh_metric_parse(const char *id, ojh_metric *out);

#define OJH_MAX_GAMES 64

/* Everything measured for one game: one result file per metric at most. */
typedef struct {
    char id[64];
    char name[128];
    char version[64];
    const ojh_jvalue *result[OJH_METRIC_COUNT]; /* file roots; NULL when not measured */
    const char *file[OJH_METRIC_COUNT];
    int replaced; /* result files set aside because a later file measured the same metric */
} ojh_game_results;

/* Groups result file roots by the game they measured. Files are taken in the order given;
   a later file for the same game and metric replaces an earlier one. Returns the number
   of games. */
int ojh_group_results(const ojh_jvalue *const *roots, const char *const *files, int count, ojh_game_results *games,
                      int max_games);

typedef enum { OJH_NOT_RANKED = 0, OJH_MORE_IS_BETTER = 1, OJH_LESS_IS_BETTER = -1 } ojh_better;

typedef struct {
    const char *id;
    ojh_metric metric;
    const char *group;   /* the report section and its ranking table */
    const char *name;
    const char *path;    /* dotted path under the result file's root; NULL for a computed statistic */
    ojh_unit unit;
    const char *word;    /* written after plain numbers: "turns/min" */
    ojh_better better;
    const char *meaning; /* one sentence for the reader */
} ojh_stat;

int ojh_stat_count(void);
const ojh_stat *ojh_stat_at(int index);
const ojh_stat *ojh_stat_find(const char *id);

/* 1 with the value when the game has this statistic, 0 when it does not. */
int ojh_stat_value(const ojh_stat *s, const ojh_game_results *game, double *out);

typedef struct {
    int game;     /* index into the games array */
    double value;
    int place;    /* 1 for the best; equal values share a place */
} ojh_placing;

/* The games that have the statistic, best first (for a statistic that is not ranked,
   largest first). Returns how many. */
int ojh_stat_rank(const ojh_stat *s, const ojh_game_results *games, int game_count, ojh_placing *out);

#endif
