#include "stats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct {
    const char *id;
    const char *name;
} METRICS[OJH_METRIC_COUNT] = {
    {"tpm", "Turn speed"},
    {"fps", "Frame rate"},
    {"net", "Network"},
    {"footprint", "Footprint"},
};

const char *ojh_metric_id(ojh_metric m) { return m < OJH_METRIC_COUNT ? METRICS[m].id : "?"; }
const char *ojh_metric_name(ojh_metric m) { return m < OJH_METRIC_COUNT ? METRICS[m].name : "?"; }

int ojh_metric_parse(const char *id, ojh_metric *out) {
    for (int m = 0; m < OJH_METRIC_COUNT; m++) {
        if (strcmp(id, METRICS[m].id) == 0) {
            *out = (ojh_metric)m;
            return 0;
        }
    }
    return -1;
}

#define TURNS "Turn speed"
#define RESOURCES "CPU and memory"
#define FRAMES "Frame rate"
#define NETWORK "Network"
#define FOOTPRINT "Footprint"

static const ojh_stat STATS[] = {
    {"tpm", OJH_METRIC_TPM, TURNS, "Turns per minute", "result.tpm", OJH_UNIT_NUMBER, "turns/min", OJH_MORE_IS_BETTER,
     "complete turns played in a minute with every player run by the game's AI"},
    {"tpm_x_players", OJH_METRIC_TPM, TURNS, "Player-turns per minute", "result.tpm_x_players", OJH_UNIT_NUMBER,
     "player-turns/min", OJH_MORE_IS_BETTER, "turns per minute times players, so a game with more players is not punished"},
    {"tpm_x_regions", OJH_METRIC_TPM, TURNS, "Region-turns per minute", "result.tpm_x_regions", OJH_UNIT_NUMBER,
     "region-turns/min", OJH_MORE_IS_BETTER, "turns per minute times map regions, so a bigger map is not punished"},
    {"median_turn", OJH_METRIC_TPM, TURNS, "Median turn", "result.per_turn.median_seconds", OJH_UNIT_SECONDS, NULL,
     OJH_LESS_IS_BETTER, "the typical time one turn takes"},
    {"p95_turn", OJH_METRIC_TPM, TURNS, "Slow turn (95th percentile)", "result.per_turn.p95_seconds", OJH_UNIT_SECONDS, NULL,
     OJH_LESS_IS_BETTER, "only one turn in twenty takes longer than this"},
    {"slowest_turn", OJH_METRIC_TPM, TURNS, "Slowest turn", "result.per_turn.slowest_seconds", OJH_UNIT_SECONDS, NULL,
     OJH_LESS_IS_BETTER, "the longest a player waited for one turn in the run"},
    {"late_slowdown", OJH_METRIC_TPM, TURNS, "Late-game slowdown", NULL, OJH_UNIT_RATIO, "x", OJH_LESS_IS_BETTER,
     "median late turn time divided by median early turn time: 1 means turns never slowed down"},
    {"start_up", OJH_METRIC_TPM, TURNS, "Start-up", "result.boot_seconds", OJH_UNIT_SECONDS, NULL, OJH_LESS_IS_BETTER,
     "from launching the game to its first turn starting, world generation or loading included"},
    {"players", OJH_METRIC_TPM, TURNS, "Players", "result.players", OJH_UNIT_NUMBER, NULL, OJH_NOT_RANKED,
     "how many players the measured game had"},
    {"regions", OJH_METRIC_TPM, TURNS, "Map regions", "result.regions", OJH_UNIT_NUMBER, NULL, OJH_NOT_RANKED,
     "provinces or tiles on the measured map"},

    {"peak_memory", OJH_METRIC_TPM, RESOURCES, "Peak memory", "result.resources.peak_memory_bytes", OJH_UNIT_BYTES, NULL,
     OJH_LESS_IS_BETTER, "the most memory the game and every process it started held at once"},
    {"memory_per_player", OJH_METRIC_TPM, RESOURCES, "Memory per player", NULL, OJH_UNIT_BYTES, NULL, OJH_LESS_IS_BETTER,
     "peak memory divided by players"},
    {"cpu_per_turn", OJH_METRIC_TPM, RESOURCES, "CPU time per turn", "result.resources.cpu_seconds_per_turn",
     OJH_UNIT_SECONDS, NULL, OJH_LESS_IS_BETTER, "processor time spent per turn, summed over every core"},
    {"cpu_per_player_turn", OJH_METRIC_TPM, RESOURCES, "CPU time per player-turn", NULL, OJH_UNIT_SECONDS, NULL,
     OJH_LESS_IS_BETTER, "CPU time per turn divided by players: what one AI player's turn costs"},
    {"cores_used", OJH_METRIC_TPM, RESOURCES, "Cores in use", "result.resources.median_cores", OJH_UNIT_NUMBER, "cores",
     OJH_NOT_RANKED, "median number of cores busy while turns ran; more is neither better nor worse on its own"},

    {"fps", OJH_METRIC_FPS, FRAMES, "Frame rate on the map", "result.map_average_fps", OJH_UNIT_NUMBER, "fps",
     OJH_MORE_IS_BETTER, "median of the average frame rate over the map scenes"},
    {"fps_low", OJH_METRIC_FPS, FRAMES, "1% low frame rate", "result.one_percent_low_fps", OJH_UNIT_NUMBER, "fps",
     OJH_MORE_IS_BETTER, "the frame rate of the slowest 1% of frames, across every scene: what stutter feels like"},
    {"frame_p99", OJH_METRIC_FPS, FRAMES, "Slow frame (99th percentile)", "result.p99_frame_seconds", OJH_UNIT_SECONDS,
     NULL, OJH_LESS_IS_BETTER, "only one frame in a hundred takes longer than this"},
    {"fps_menu", OJH_METRIC_FPS, FRAMES, "Main menu", "result.scenes.menu.average_fps", OJH_UNIT_NUMBER, "fps",
     OJH_MORE_IS_BETTER, "average frame rate on the main menu, idle"},
    {"fps_map_start", OJH_METRIC_FPS, FRAMES, "Map, start of game", "result.scenes.map-start.average_fps",
     OJH_UNIT_NUMBER, "fps", OJH_MORE_IS_BETTER, "the world map at the start of a game, default zoom, idle"},
    {"fps_map_out", OJH_METRIC_FPS, FRAMES, "Map, zoomed out", "result.scenes.map-out.average_fps", OJH_UNIT_NUMBER,
     "fps", OJH_MORE_IS_BETTER, "the world map zoomed all the way out"},
    {"fps_map_in", OJH_METRIC_FPS, FRAMES, "Map, zoomed in", "result.scenes.map-in.average_fps", OJH_UNIT_NUMBER, "fps",
     OJH_MORE_IS_BETTER, "the world map zoomed all the way in"},
    {"fps_map_pan", OJH_METRIC_FPS, FRAMES, "Map, scrolling", "result.scenes.map-pan.average_fps", OJH_UNIT_NUMBER,
     "fps", OJH_MORE_IS_BETTER, "the world map scrolling continuously"},
    {"fps_panel", OJH_METRIC_FPS, FRAMES, "Heaviest panel", "result.scenes.panel.average_fps", OJH_UNIT_NUMBER, "fps",
     OJH_MORE_IS_BETTER, "the heaviest information screen: economy, diplomacy or research"},
    {"fps_map_late", OJH_METRIC_FPS, FRAMES, "Map, late game", "result.scenes.map-late.average_fps", OJH_UNIT_NUMBER,
     "fps", OJH_MORE_IS_BETTER, "the world map after many turns have been played"},
    {"fps_end_turn", OJH_METRIC_FPS, FRAMES, "While a turn resolves", "result.scenes.end-turn.average_fps",
     OJH_UNIT_NUMBER, "fps", OJH_MORE_IS_BETTER, "the map while the game is processing a turn"},

    {"dpt_highest", OJH_METRIC_NET, NETWORK, "Data per turn, highest", "result.dpt.highest_bytes", OJH_UNIT_BYTES, NULL,
     OJH_LESS_IS_BETTER, "the most bytes one turn took, both ways: what a connection must survive"},
    {"dpt_median", OJH_METRIC_NET, NETWORK, "Data per turn, median", "result.dpt.median_bytes", OJH_UNIT_BYTES, NULL,
     OJH_LESS_IS_BETTER, "the bytes a typical turn takes, both ways"},
    {"dpt_lowest", OJH_METRIC_NET, NETWORK, "Data per turn, lowest", "result.dpt.lowest_bytes", OJH_UNIT_BYTES, NULL,
     OJH_LESS_IS_BETTER, "the fewest bytes one turn took, both ways: the floor a connection always carries"},
    {"dpt_per_player", OJH_METRIC_NET, NETWORK, "Highest data per turn per player", NULL, OJH_UNIT_BYTES, NULL,
     OJH_LESS_IS_BETTER, "the highest data per turn divided by players in the game"},
    {"delivery", OJH_METRIC_NET, NETWORK, "Turn delivery time", "result.delivery.median_seconds", OJH_UNIT_SECONDS, NULL,
     OJH_LESS_IS_BETTER, "median time from a turn ending to its last byte reaching the clients, with no internet in the way"},
    {"nipm", OJH_METRIC_NET, NETWORK, "Network information per minute", "result.nipm_bytes_per_minute", OJH_UNIT_BYTES,
     "/min", OJH_NOT_RANKED, "bytes moved per minute of play, both ways; more is not better or worse on its own"},
    {"nipm_per_client", OJH_METRIC_NET, NETWORK, "Information per minute per client", "result.nipm_per_client_bytes",
     OJH_UNIT_BYTES, "/min", OJH_NOT_RANKED, "network information per minute divided by clients"},
    {"busiest_second", OJH_METRIC_NET, NETWORK, "Busiest second", "result.busiest_second_bytes", OJH_UNIT_BYTES, NULL,
     OJH_LESS_IS_BETTER, "the most bytes moved in any one second: the burst a connection must absorb"},

    {"install_size", OJH_METRIC_FOOTPRINT, FOOTPRINT, "Install size", "result.install_bytes", OJH_UNIT_BYTES, NULL,
     OJH_LESS_IS_BETTER, "everything a player downloads to play"},
    {"save_size", OJH_METRIC_FOOTPRINT, FOOTPRINT, "Save file size", "result.save_bytes", OJH_UNIT_BYTES, NULL,
     OJH_LESS_IS_BETTER, "one save of the measured game"},
    {"save_time", OJH_METRIC_FOOTPRINT, FOOTPRINT, "Save time", "result.save_seconds", OJH_UNIT_SECONDS, NULL,
     OJH_LESS_IS_BETTER, "how long writing that save took"},
    {"load_time", OJH_METRIC_FOOTPRINT, FOOTPRINT, "Load time", "result.load_seconds", OJH_UNIT_SECONDS, NULL,
     OJH_LESS_IS_BETTER, "how long loading that save took"},
};

#define STAT_COUNT ((int)(sizeof STATS / sizeof STATS[0]))

int ojh_stat_count(void) { return STAT_COUNT; }
const ojh_stat *ojh_stat_at(int i) { return i >= 0 && i < STAT_COUNT ? &STATS[i] : NULL; }

const ojh_stat *ojh_stat_find(const char *id) {
    for (int i = 0; i < STAT_COUNT; i++) {
        if (strcmp(STATS[i].id, id) == 0) return &STATS[i];
    }
    return NULL;
}

static int number_at(const ojh_jvalue *root, const char *path, double *out) {
    const ojh_jvalue *v = ojh_jpath(root, path);
    if (!v || v->type != OJH_JNUMBER) return 0;
    *out = v->number;
    return 1;
}

int ojh_stat_value(const ojh_stat *s, const ojh_game_results *g, double *out) {
    const ojh_jvalue *root = g->result[s->metric];
    if (!root) return 0;
    double a, b;
    if (s->path) {
        if (!number_at(root, s->path, &a)) return 0;
        if (s->better == OJH_MORE_IS_BETTER && a <= 0) return 0;
        *out = a;
        return 1;
    }
    if (strcmp(s->id, "late_slowdown") == 0) {
        if (!number_at(root, "result.per_turn.late_median_seconds", &a) ||
            !number_at(root, "result.per_turn.early_median_seconds", &b) || b <= 0) {
            return 0;
        }
        *out = a / b;
        return 1;
    }
    if (strcmp(s->id, "memory_per_player") == 0) {
        if (!number_at(root, "result.resources.peak_memory_bytes", &a) || !number_at(root, "result.players", &b) || b <= 0) {
            return 0;
        }
        *out = a / b;
        return 1;
    }
    if (strcmp(s->id, "cpu_per_player_turn") == 0) {
        if (!number_at(root, "result.resources.cpu_seconds_per_turn", &a) || !number_at(root, "result.players", &b) ||
            b <= 0) {
            return 0;
        }
        *out = a / b;
        return 1;
    }
    if (strcmp(s->id, "dpt_per_player") == 0) {
        if (!number_at(root, "result.dpt.highest_bytes", &a) || !number_at(root, "result.players", &b) || b <= 0) return 0;
        *out = a / b;
        return 1;
    }
    return 0;
}

static ojh_better sort_better;

static int by_placing(const void *pa, const void *pb) {
    const ojh_placing *x = pa, *y = pb;
    if (x->value == y->value) return x->game - y->game;
    int x_first = sort_better == OJH_LESS_IS_BETTER ? x->value < y->value : x->value > y->value;
    return x_first ? -1 : 1;
}

int ojh_stat_rank(const ojh_stat *s, const ojh_game_results *games, int game_count, ojh_placing *out) {
    int n = 0;
    for (int i = 0; i < game_count; i++) {
        double v;
        if (ojh_stat_value(s, &games[i], &v)) {
            out[n].game = i;
            out[n].value = v;
            n++;
        }
    }
    sort_better = s->better;
    qsort(out, (size_t)n, sizeof *out, by_placing);
    for (int i = 0; i < n; i++) out[i].place = i > 0 && out[i].value == out[i - 1].value ? out[i - 1].place : i + 1;
    return n;
}

int ojh_group_results(const ojh_jvalue *const *roots, const char *const *files, int count, ojh_game_results *games,
                      int max_games) {
    int n = 0;
    for (int i = 0; i < count; i++) {
        ojh_metric metric;
        if (ojh_metric_parse(ojh_jstring(ojh_jget(roots[i], "metric"), ""), &metric) != 0) continue;
        const ojh_jvalue *r = ojh_jget(roots[i], "result");
        const char *id = ojh_jstring(ojh_jget(r, "game"), "");
        if (!*id) continue;
        int g = 0;
        while (g < n && strcmp(games[g].id, id) != 0) g++;
        if (g == n) {
            if (n >= max_games) continue;
            memset(&games[n], 0, sizeof games[n]);
            snprintf(games[n].id, sizeof games[n].id, "%s", id);
            n++;
        }
        const char *name = ojh_jstring(ojh_jget(r, "name"), "");
        const char *version = ojh_jstring(ojh_jget(r, "version"), "");
        if (*name) snprintf(games[g].name, sizeof games[g].name, "%s", name);
        if (*version) snprintf(games[g].version, sizeof games[g].version, "%s", version);
        if (games[g].result[metric]) games[g].replaced++;
        games[g].result[metric] = roots[i];
        games[g].file[metric] = files ? files[i] : NULL;
    }
    for (int g = 0; g < n; g++) {
        if (!*games[g].name) snprintf(games[g].name, sizeof games[g].name, "%s", games[g].id);
    }
    return n;
}
