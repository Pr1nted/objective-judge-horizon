#include "score.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* src/machine.c's workload, in rounds per second on one core, on OJH's reference CPU.
   A machine at 2000 is taken to be twice as fast as the reference. */
#define REFERENCE_SINGLE_CORE 1000.0

/* Version 1 parts. Weights add up to 1. */
#define TURN_THROUGHPUT_REFERENCE 2000.0    /* player-turns per minute */
#define WORLD_THROUGHPUT_REFERENCE 200000.0 /* region-turns per minute */
#define LATE_PACE_REFERENCE 0.5             /* late turns take twice as long as early ones */
#define STEADINESS_REFERENCE 0.5            /* the p95 turn takes twice the median */
#define START_UP_REFERENCE 10.0             /* seconds */

double ojh_score_points(double value, double reference) {
    if (!(value > 0) || !(reference > 0)) return 0;
    return 1000.0 * log2(1.0 + value / reference);
}

static const ojh_jvalue *number(const ojh_jvalue *v) { return v && v->type == OJH_JNUMBER ? v : NULL; }

static ojh_score_part *add_part(ojh_score *s, const char *key, const char *name, const char *measures, const char *unit,
                               double weight, double reference) {
    ojh_score_part *p = &s->parts[s->part_count++];
    p->key = key;
    p->name = name;
    p->measures = measures;
    p->unit = unit;
    p->weight = weight;
    p->reference = reference;
    return p;
}

static void reason(ojh_score *s, const char *text) {
    if (s->reason_count < OJH_SCORE_MAX_REASONS) {
        snprintf(s->reasons[s->reason_count++], sizeof s->reasons[0], "%s", text);
    }
}

int ojh_score_result(const ojh_jvalue *root, ojh_score *s) {
    memset(s, 0, sizeof *s);
    if (strcmp(ojh_jstring(ojh_jget(root, "metric"), ""), "tpm") != 0) return -1;
    const ojh_jvalue *r = ojh_jget(root, "result");
    if (!r || r->type != OJH_JOBJECT) return -1;
    snprintf(s->id, sizeof s->id, "%s", ojh_jstring(ojh_jget(r, "game"), "game"));
    snprintf(s->name, sizeof s->name, "%s", ojh_jstring(ojh_jget(r, "name"), s->id));
    s->turns = (int)ojh_jnumber(ojh_jget(r, "turns"), 0);

    double single = ojh_jnumber(ojh_jpath(root, "machine.reference.single_core_rounds_per_second"), 0);
    s->hardware_known = single > 0;
    s->hardware_factor = single > 0 ? REFERENCE_SINGLE_CORE / single : 1.0;

    double tpm = ojh_jnumber(ojh_jget(r, "tpm"), 0);
    const ojh_jvalue *players = number(ojh_jget(r, "players"));
    const ojh_jvalue *regions = number(ojh_jget(r, "regions"));
    const ojh_jvalue *per_turn = ojh_jget(r, "per_turn");
    const ojh_jvalue *early = number(ojh_jget(per_turn, "early_median_seconds"));
    const ojh_jvalue *late = number(ojh_jget(per_turn, "late_median_seconds"));
    const ojh_jvalue *median = number(ojh_jget(per_turn, "median_seconds"));
    const ojh_jvalue *p95 = number(ojh_jget(per_turn, "p95_seconds"));
    const ojh_jvalue *boot = number(ojh_jget(r, "boot_seconds"));

    ojh_score_part *p;
    p = add_part(s, "turn_throughput", "Turn throughput",
                 "turns per minute times players: how many player-turns the game resolves in a minute",
                 "player-turns/min", 0.40, TURN_THROUGHPUT_REFERENCE);
    p->hardware_adjusted = 1;
    if (tpm > 0 && players && players->number > 0) {
        p->present = 1;
        p->measured = tpm * players->number;
        p->value = p->measured * s->hardware_factor;
    }

    p = add_part(s, "world_throughput", "World throughput",
                 "turns per minute times map regions: how much map the game resolves in a minute",
                 "region-turns/min", 0.25, WORLD_THROUGHPUT_REFERENCE);
    p->hardware_adjusted = 1;
    if (tpm > 0 && regions && regions->number > 0) {
        p->present = 1;
        p->measured = tpm * regions->number;
        p->value = p->measured * s->hardware_factor;
    }

    p = add_part(s, "late_game_pace", "Late-game pace",
                 "median early turn time divided by median late turn time: 1 means turns never slow down",
                 "early / late", 0.15, LATE_PACE_REFERENCE);
    if (early && late && early->number > 0 && late->number > 0) {
        p->present = 1;
        p->measured = early->number / late->number;
        p->value = p->measured > 1 ? 1 : p->measured;
    }

    p = add_part(s, "steadiness", "Steadiness",
                 "median turn time divided by the 95th percentile: 1 means no turn is slower than usual",
                 "median / p95", 0.10, STEADINESS_REFERENCE);
    if (median && p95 && median->number > 0 && p95->number > 0) {
        p->present = 1;
        p->measured = median->number / p95->number;
        p->value = p->measured > 1 ? 1 : p->measured;
    }

    p = add_part(s, "start_up", "Start-up", "seconds from launching the game to its first turn starting", "s", 0.10,
                 START_UP_REFERENCE);
    p->hardware_adjusted = 1;
    p->lower_is_better = 1;
    if (boot && boot->number >= 0) {
        p->present = 1;
        p->measured = boot->number;
        p->value = boot->number / s->hardware_factor; /* seconds it would take on the reference CPU */
    }

    double weighted = 0;
    for (int i = 0; i < s->part_count; i++) {
        ojh_score_part *q = &s->parts[i];
        if (!q->present) continue;
        /* Below 10 ms a start-up is instant for a player; the floor also keeps 0 from dividing. */
        q->points = q->lower_is_better ? ojh_score_points(q->reference, q->value < 0.01 ? 0.01 : q->value)
                                       : ojh_score_points(q->value, q->reference);
        weighted += q->weight * q->points;
        s->coverage += q->weight;
    }
    s->total = s->coverage > 0 ? weighted / s->coverage : 0;

    char text[200];
    if (ojh_jpresent(ojh_jget(root, "error"))) reason(s, "the run did not finish cleanly, so its figures may be partial");
    if (s->turns < OJH_SCORE_MIN_TURNS) {
        snprintf(text, sizeof text, "%d turns were timed; a score needs at least %d because turns slow down as a game goes on",
                 s->turns, OJH_SCORE_MIN_TURNS);
        reason(s, text);
    }
    if (!s->hardware_known) reason(s, "the result has no CPU reference score, so speeds are not put on the reference CPU");
    s->provisional = s->reason_count > 0;
    if (s->coverage < 0.999) {
        size_t at = (size_t)snprintf(text, sizeof text, "scored on %.0f%% of the weight; not reported:", s->coverage * 100);
        int first = 1;
        for (int i = 0; i < s->part_count && at < sizeof text; i++) {
            if (s->parts[i].present) continue;
            at += (size_t)snprintf(text + at, sizeof text - at, "%s %s", first ? "" : ",", s->parts[i].name);
            first = 0;
        }
        reason(s, text); /* a partial score is still final; it says what it lacks */
    }
    return 0;
}

void ojh_score_json(ojh_json *w, const ojh_score *s) {
    ojh_json_object(w);
    ojh_json_key(w, "score_version"); ojh_json_int(w, OJH_SCORE_VERSION);
    ojh_json_key(w, "game"); ojh_json_string(w, s->id);
    ojh_json_key(w, "name"); ojh_json_string(w, s->name);
    ojh_json_key(w, "score"); ojh_json_int(w, (int64_t)(s->total + 0.5));
    ojh_json_key(w, "coverage"); ojh_json_double(w, s->coverage, 2);
    ojh_json_key(w, "provisional"); ojh_json_bool(w, s->provisional);
    ojh_json_key(w, "turns"); ojh_json_int(w, s->turns);
    ojh_json_key(w, "hardware_factor");
    if (s->hardware_known) ojh_json_double(w, s->hardware_factor, 4);
    else ojh_json_null(w);
    ojh_json_key(w, "notes");
    ojh_json_array(w);
    for (int i = 0; i < s->reason_count; i++) ojh_json_string(w, s->reasons[i]);
    ojh_json_end_array(w);
    ojh_json_key(w, "parts");
    ojh_json_array(w);
    for (int i = 0; i < s->part_count; i++) {
        const ojh_score_part *p = &s->parts[i];
        ojh_json_object(w);
        ojh_json_key(w, "part"); ojh_json_string(w, p->key);
        ojh_json_key(w, "name"); ojh_json_string(w, p->name);
        ojh_json_key(w, "weight"); ojh_json_double(w, p->weight, 2);
        ojh_json_key(w, "unit"); ojh_json_string(w, p->unit);
        ojh_json_key(w, "reference"); ojh_json_double(w, p->reference, 2);
        ojh_json_key(w, "measured");
        if (p->present) ojh_json_double(w, p->measured, 4);
        else ojh_json_null(w);
        ojh_json_key(w, "on_reference_cpu");
        if (p->present) ojh_json_double(w, p->value, 4);
        else ojh_json_null(w);
        ojh_json_key(w, "points");
        if (p->present) ojh_json_int(w, (int64_t)(p->points + 0.5));
        else ojh_json_null(w);
        ojh_json_end_object(w);
    }
    ojh_json_end_array(w);
    ojh_json_end_object(w);
}
