#include "score.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* src/machine.c's workload, in rounds per second on one core, on OJH's reference CPU.
   A machine at 2000 is taken to be twice as fast as the reference. */
#define REFERENCE_SINGLE_CORE 1000.0

double ojh_score_points(double value, double reference) {
    if (!(value > 0) || !(reference > 0)) return 0;
    return 1000.0 * log2(1.0 + value / reference);
}

static ojh_score_part *add(ojh_score *s, const char *key, const char *name, const char *measures, ojh_metric metric,
                           ojh_unit unit, const char *word, double weight, double reference, int lower_is_better,
                           int hardware_adjusted) {
    ojh_score_part *p = &s->parts[s->part_count++];
    p->key = key;
    p->name = name;
    p->measures = measures;
    p->metric = metric;
    p->unit = unit;
    p->word = word;
    p->weight = weight;
    p->reference = reference;
    p->lower_is_better = lower_is_better;
    p->hardware_adjusted = hardware_adjusted;
    return p;
}

static void reason(ojh_score *s, const char *text) {
    if (s->reason_count < OJH_SCORE_MAX_REASONS) {
        snprintf(s->reasons[s->reason_count++], sizeof s->reasons[0], "%s", text);
    }
}

/* Fills a part from a catalogue statistic. */
static void from_stat(const ojh_score *s, ojh_score_part *p, const ojh_game_results *g, const char *stat_id) {
    const ojh_stat *stat = ojh_stat_find(stat_id);
    double v;
    if (!stat || !ojh_stat_value(stat, g, &v) || v < 0 || (!p->lower_is_better && v <= 0)) return;
    p->present = 1;
    p->measured = v;
    if (!p->hardware_adjusted) p->value = v;
    else if (p->lower_is_better) p->value = v / s->hardware_factor; /* seconds it would take on the reference CPU */
    else p->value = v * s->hardware_factor;
}

static int number(const ojh_jvalue *root, const char *path, double *out) {
    const ojh_jvalue *v = ojh_jpath(root, path);
    if (!v || v->type != OJH_JNUMBER) return 0;
    *out = v->number;
    return 1;
}

int ojh_score_game(const ojh_game_results *g, ojh_score *s) {
    memset(s, 0, sizeof *s);
    int any = 0;
    for (int m = 0; m < OJH_METRIC_COUNT; m++) any |= g->result[m] != NULL;
    if (!any) return -1;
    snprintf(s->id, sizeof s->id, "%s", g->id);
    snprintf(s->name, sizeof s->name, "%s", g->name);

    double single = 0;
    for (int m = 0; m < OJH_METRIC_COUNT && single <= 0; m++) {
        if (g->result[m]) number(g->result[m], "machine.reference.single_core_rounds_per_second", &single);
    }
    s->hardware_known = single > 0;
    s->hardware_factor = single > 0 ? REFERENCE_SINGLE_CORE / single : 1.0;
    const ojh_jvalue *tpm = g->result[OJH_METRIC_TPM];
    if (tpm) s->turns = (int)ojh_jnumber(ojh_jpath(tpm, "result.turns"), 0);

    /* Version 2 parts. Weights add up to 1. */
    ojh_score_part *p;
    p = add(s, "turn_throughput", "Turn throughput",
            "turns per minute times players: how many player-turns the game resolves in a minute", OJH_METRIC_TPM,
            OJH_UNIT_NUMBER, "player-turns/min", 0.22, 2000.0, 0, 1);
    from_stat(s, p, g, "tpm_x_players");
    p = add(s, "world_throughput", "World throughput",
            "turns per minute times map regions: how much map the game resolves in a minute", OJH_METRIC_TPM,
            OJH_UNIT_NUMBER, "region-turns/min", 0.13, 200000.0, 0, 1);
    from_stat(s, p, g, "tpm_x_regions");

    p = add(s, "late_game_pace", "Late-game pace",
            "median early turn time divided by median late turn time: 1 means turns never slow down", OJH_METRIC_TPM,
            OJH_UNIT_RATIO, NULL, 0.08, 0.5, 0, 0);
    double v;
    const ojh_stat *slowdown = ojh_stat_find("late_slowdown");
    if (slowdown && ojh_stat_value(slowdown, g, &v) && v > 0) {
        p->present = 1;
        p->measured = 1.0 / v;
        p->capped = p->measured > 1;
        p->value = p->capped ? 1 : p->measured;
    }

    p = add(s, "steadiness", "Steadiness",
            "median turn time divided by the 95th percentile: 1 means no turn is slower than usual", OJH_METRIC_TPM,
            OJH_UNIT_RATIO, NULL, 0.05, 0.5, 0, 0);
    double median, p95;
    if (tpm && number(tpm, "result.per_turn.median_seconds", &median) && number(tpm, "result.per_turn.p95_seconds", &p95) &&
        median > 0 && p95 > 0) {
        p->present = 1;
        p->measured = median / p95;
        p->capped = p->measured > 1;
        p->value = p->capped ? 1 : p->measured;
    }

    p = add(s, "start_up", "Start-up", "seconds from launching the game to its first turn starting", OJH_METRIC_TPM,
            OJH_UNIT_SECONDS, NULL, 0.05, 10.0, 1, 1);
    from_stat(s, p, g, "start_up");

    p = add(s, "cpu_per_player_turn", "CPU per player-turn",
            "processor time one AI player's turn costs, summed over every core", OJH_METRIC_TPM, OJH_UNIT_SECONDS, NULL,
            0.06, 0.01, 1, 1);
    from_stat(s, p, g, "cpu_per_player_turn");

    p = add(s, "memory", "Memory", "the most memory the game held while its turns ran", OJH_METRIC_TPM, OJH_UNIT_BYTES,
            NULL, 0.08, 2147483648.0, 1, 0);
    from_stat(s, p, g, "peak_memory");

    p = add(s, "frame_rate", "Frame rate", "median of the average frame rate over the map scenes", OJH_METRIC_FPS,
            OJH_UNIT_NUMBER, "fps", 0.12, 60.0, 0, 0);
    from_stat(s, p, g, "fps");

    p = add(s, "smoothness", "Smoothness", "the frame rate of the slowest 1% of frames across every scene",
            OJH_METRIC_FPS, OJH_UNIT_NUMBER, "fps", 0.08, 30.0, 0, 0);
    from_stat(s, p, g, "fps_low");

    p = add(s, "data_per_turn", "Data per turn", "the most bytes one turn took over the network, both ways",
            OJH_METRIC_NET, OJH_UNIT_BYTES, NULL, 0.09, 262144.0, 1, 0);
    from_stat(s, p, g, "dpt_highest");

    p = add(s, "turn_delivery", "Turn delivery", "median time from a turn ending to its last byte reaching the clients",
            OJH_METRIC_NET, OJH_UNIT_SECONDS, NULL, 0.04, 0.1, 1, 1);
    from_stat(s, p, g, "delivery");

    double weighted = 0;
    int adjusted_present = 0;
    for (int i = 0; i < s->part_count; i++) {
        ojh_score_part *q = &s->parts[i];
        if (!q->present) continue;
        /* A floor keeps a zero from dividing: 10 ms, 1 byte. */
        double floor_value = q->unit == OJH_UNIT_BYTES ? 1.0 : 0.01;
        q->points = q->lower_is_better ? ojh_score_points(q->reference, q->value < floor_value ? floor_value : q->value)
                                       : ojh_score_points(q->value, q->reference);
        weighted += q->weight * q->points;
        s->coverage += q->weight;
        adjusted_present |= q->hardware_adjusted;
    }
    s->total = s->coverage > 0 ? weighted / s->coverage : 0;

    char text[240];
    for (int m = 0; m < OJH_METRIC_COUNT; m++) {
        if (g->result[m] && ojh_jpresent(ojh_jget(g->result[m], "error"))) {
            snprintf(text, sizeof text, "the %s run did not finish cleanly, so its figures may be partial",
                     ojh_metric_name((ojh_metric)m));
            reason(s, text);
        }
    }
    if (tpm && s->turns < OJH_SCORE_MIN_TURNS) {
        snprintf(text, sizeof text, "%d turns were timed; a score needs at least %d because turns slow down as a game goes on",
                 s->turns, OJH_SCORE_MIN_TURNS);
        reason(s, text);
    }
    if (!s->hardware_known && adjusted_present) {
        reason(s, "the results have no CPU reference score, so speeds are not put on the reference CPU");
    }
    s->provisional = s->reason_count > 0;
    if (s->coverage < 0.999) {
        size_t at = (size_t)snprintf(text, sizeof text, "scored on %.0f%% of the weight; not reported:", s->coverage * 100);
        int first = 1;
        for (int i = 0; i < s->part_count && at < sizeof text; i++) {
            if (s->parts[i].present) continue;
            at += (size_t)snprintf(text + at, sizeof text - at, "%s %s", first ? "" : ",", s->parts[i].name);
            first = 0;
        }
        reason(s, text); /* a partial score is still a score; it says what it lacks */
    }
    return 0;
}

int ojh_score_result(const ojh_jvalue *root, ojh_score *out) {
    ojh_game_results g;
    if (ojh_group_results(&root, NULL, 1, &g, 1) != 1) {
        memset(out, 0, sizeof *out);
        return -1;
    }
    return ojh_score_game(&g, out);
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
        ojh_json_key(w, "measurement"); ojh_json_string(w, ojh_metric_id(p->metric));
        ojh_json_key(w, "weight"); ojh_json_double(w, p->weight, 2);
        ojh_json_key(w, "less_is_better"); ojh_json_bool(w, p->lower_is_better);
        ojh_json_key(w, "reference"); ojh_json_double(w, p->reference, 4);
        ojh_json_key(w, "measured");
        if (p->present) ojh_json_double(w, p->measured, 5);
        else ojh_json_null(w);
        ojh_json_key(w, "on_reference_cpu");
        if (p->present) ojh_json_double(w, p->value, 5);
        else ojh_json_null(w);
        ojh_json_key(w, "points");
        if (p->present) ojh_json_int(w, (int64_t)(p->points + 0.5));
        else ojh_json_null(w);
        ojh_json_end_object(w);
    }
    ojh_json_end_array(w);
    ojh_json_end_object(w);
}
