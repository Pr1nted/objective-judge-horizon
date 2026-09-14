#ifndef OJH_SCORE_H
#define OJH_SCORE_H

#include "chart.h"
#include "json.h"
#include "jsonread.h"
#include "stats.h"

#define OJH_SCORE_VERSION 2
#define OJH_SCORE_MAX_PARTS 12
#define OJH_SCORE_MIN_TURNS 100
#define OJH_SCORE_MAX_REASONS 8

typedef struct {
    const char *key;
    const char *name;
    const char *measures;
    ojh_unit unit;
    const char *word;
    ojh_metric metric;
    double weight;
    int present;
    int hardware_adjusted;
    int lower_is_better;
    int capped;
    double measured;
    double value;
    double reference;
    double points;
} ojh_score_part;

typedef struct {
    char id[64];
    char name[128];
    int turns;
    ojh_score_part parts[OJH_SCORE_MAX_PARTS];
    int part_count;
    double total;
    double coverage;
    double hardware_factor;
    int hardware_known;
    int provisional;
    char reasons[OJH_SCORE_MAX_REASONS][240];
    int reason_count;
} ojh_score;

double ojh_score_points(double value, double reference);

int ojh_score_game(const ojh_game_results *game, ojh_score *out);

int ojh_score_result(const ojh_jvalue *root, ojh_score *out);

void ojh_score_json(ojh_json *w, const ojh_score *s);

#endif
