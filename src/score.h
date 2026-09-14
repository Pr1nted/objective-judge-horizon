#ifndef OJH_SCORE_H
#define OJH_SCORE_H

#include "json.h"
#include "jsonread.h"

/* The OJH score: one number for one game, built from that game's own result file and
   nothing else. Every part is measured against a fixed reference level written into this
   file, never against the other games in a run, so adding, removing or re-running another
   game cannot move a score.

   Each part gives 1000 x log2(1 + value / reference) points: the reference level is worth
   1000, three times it 2000, seven times it 3000, and nothing is ever negative. The score
   is the weighted mean of the parts the result has; coverage says how much of the weight
   that was. Speeds are first put on OJH's reference CPU using the machine's reference
   score, so a faster computer does not make a faster game.

   A score version changes whenever a part, weight or reference level does; scores of
   different versions are not compared. */

#define OJH_SCORE_VERSION 1
#define OJH_SCORE_MAX_PARTS 8
#define OJH_SCORE_MIN_TURNS 100
#define OJH_SCORE_MAX_REASONS 6

typedef struct {
    const char *key;      /* "turn_throughput" */
    const char *name;     /* "Turn throughput" */
    const char *measures; /* what it is, in words */
    const char *unit;
    double weight;
    int present;
    int hardware_adjusted;
    int lower_is_better;
    double measured;  /* as the result file has it */
    double value;     /* on the reference CPU where hardware_adjusted, capped where the part says */
    double reference; /* the level worth 1000 points */
    double points;
} ojh_score_part;

typedef struct {
    char id[64];
    char name[128];
    int turns;
    ojh_score_part parts[OJH_SCORE_MAX_PARTS];
    int part_count;
    double total;           /* weighted mean of the present parts' points */
    double coverage;        /* weight of the present parts, 0 to 1 */
    double hardware_factor; /* reference CPU speed / this CPU's single-core speed; 1 when unknown */
    int hardware_known;
    int provisional;        /* see reasons */
    char reasons[OJH_SCORE_MAX_REASONS][200];
    int reason_count;
} ojh_score;

/* 1000 x log2(1 + value / reference); 0 for no value. */
double ojh_score_points(double value, double reference);

/* Scores one result file. Returns 0 when it is a result OJH can score. */
int ojh_score_result(const ojh_jvalue *root, ojh_score *out);

void ojh_score_json(ojh_json *w, const ojh_score *s);

#endif
