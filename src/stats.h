#ifndef OJH_STATS_H
#define OJH_STATS_H

#include "chart.h"
#include "jsonread.h"

typedef enum {
    OJH_METRIC_TPM,
    OJH_METRIC_FPS,
    OJH_METRIC_NET,
    OJH_METRIC_FOOTPRINT,
    OJH_METRIC_COUNT
} ojh_metric;

const char *ojh_metric_id(ojh_metric m);
const char *ojh_metric_name(ojh_metric m);
int ojh_metric_parse(const char *id, ojh_metric *out);

#define OJH_MAX_GAMES 64

typedef struct {
    char id[64];
    char name[128];
    char version[64];
    const ojh_jvalue *result[OJH_METRIC_COUNT];
    const char *file[OJH_METRIC_COUNT];
    int replaced;
} ojh_game_results;

int ojh_group_results(const ojh_jvalue *const *roots, const char *const *files, int count, ojh_game_results *games,
                      int max_games);

typedef enum { OJH_NOT_RANKED = 0, OJH_MORE_IS_BETTER = 1, OJH_LESS_IS_BETTER = -1 } ojh_better;

typedef struct {
    const char *id;
    ojh_metric metric;
    const char *group;
    const char *name;
    const char *path;
    ojh_unit unit;
    const char *word;
    ojh_better better;
    const char *meaning;
} ojh_stat;

int ojh_stat_count(void);
const ojh_stat *ojh_stat_at(int index);
const ojh_stat *ojh_stat_find(const char *id);

int ojh_stat_value(const ojh_stat *s, const ojh_game_results *game, double *out);

typedef struct {
    int game;
    double value;
    int place;
} ojh_placing;

int ojh_stat_rank(const ojh_stat *s, const ojh_game_results *games, int game_count, ojh_placing *out);

#endif
