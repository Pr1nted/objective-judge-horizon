#include "platform.h"
#include "tpm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct {
    const char *id;
    const char *name;
} GAMES[OJH_GAME_COUNT] = {
    {"opendoctrines", "Open Doctrines"},
    {"gd5", "Greater Diplomacy 5"},
    {"freeciv", "Freeciv"},
    {"unciv", "Unciv"},
};

const char *ojh_game_id(ojh_game g) { return g < OJH_GAME_COUNT ? GAMES[g].id : g == OJH_GAME_CUSTOM ? "custom" : "?"; }
const char *ojh_game_name(ojh_game g) { return g < OJH_GAME_COUNT ? GAMES[g].name : g == OJH_GAME_CUSTOM ? "Custom game" : "?"; }

int ojh_game_parse(const char *id, ojh_game *out) {
    for (int g = 0; g < OJH_GAME_COUNT; g++) {
        if (strcmp(id, GAMES[g].id) == 0) {
            *out = (ojh_game)g;
            return 0;
        }
    }
    return -1;
}

/* ---------------------------------------------------------------- parsing helpers */

/* The number after key in text, skipping quotes, colons, equals signs and spaces. */
static int number_after(const char *text, const char *key, double *out) {
    const char *at = strstr(text, key);
    if (!at) return 0;
    at += strlen(key);
    while (*at == ' ' || *at == '"' || *at == ':' || *at == '=') at++;
    char *end;
    double v = strtod(at, &end);
    if (end == at) return 0;
    *out = v;
    return 1;
}

static void push_turn(ojh_tpm *t, double seconds) {
    if (t->timed_turns % 256 == 0) {
        double *grown = realloc(t->turn_seconds, (size_t)(t->timed_turns + 256) * sizeof *grown);
        if (!grown) return;
        t->turn_seconds = grown;
    }
    t->turn_seconds[t->timed_turns++] = seconds;
    t->play_seconds += seconds;
}

static void reset(ojh_game game, ojh_tpm *t) {
    memset(t, 0, sizeof *t);
    t->game = game;
    snprintf(t->id, sizeof t->id, "%s", ojh_game_id(game));
    snprintf(t->name, sizeof t->name, "%s", ojh_game_name(game));
    t->boot_seconds = -1;
    t->exit_code = -1;
}

/* GD5's and Unciv's drivers print {"turn": ..., "<key>": seconds, ...} per turn and a
   {"summary": {...}} line at the end. */
static int parse_json_driver(const ojh_line *lines, size_t count, const char *turn_key, const char *regions_key,
                             const char *players_key, const char *kind, ojh_tpm *t) {
    for (size_t i = 0; i < count; i++) {
        if (lines[i].stream != OJH_STDOUT) continue;
        const char *s = lines[i].text;
        double v;
        if (strncmp(s, "{\"turn\"", 7) == 0 && number_after(s, turn_key, &v)) {
            push_turn(t, v);
        } else if (strncmp(s, "{\"summary\"", 10) == 0) {
            if (number_after(s, "\"boot_seconds\"", &v)) t->boot_seconds = v;
            if (number_after(s, regions_key, &v)) t->regions = (long)v;
            if (number_after(s, players_key, &v)) t->players = (int)v;
        }
    }
    t->turns = t->timed_turns;
    snprintf(t->region_kind, sizeof t->region_kind, "%s", kind);
    return t->turns > 0 ? 0 : -1;
}

/* Freeciv logs one "End/start-turn server/ai activities" line per turn (verbose log on
   stderr). The gap between two of them is one whole turn; the first one ends start-up. */
static int parse_freeciv(const ojh_line *lines, size_t count, ojh_tpm *t) {
    double first = -1, previous = -1;
    int markers = 0, players = 0;
    for (size_t i = 0; i < count; i++) {
        const char *s = lines[i].text;
        if (strstr(s, "End/start-turn server/ai activities:")) {
            if (first < 0) first = lines[i].t;
            else push_turn(t, lines[i].t - previous);
            previous = lines[i].t;
            markers++;
        } else if (strstr(s, "Creating a map of size")) {
            const char *eq = strstr(s, "= ");
            if (eq) t->regions = strtol(eq + 2, NULL, 10);
        } else if (strstr(s, " rules the ")) {
            players++;
        }
    }
    t->turns = t->timed_turns;
    t->players = players;
    t->boot_seconds = first;
    snprintf(t->region_kind, sizeof t->region_kind, "tiles");
    snprintf(t->how, sizeof t->how,
             "gaps between Freeciv's per-turn \"End/start-turn server/ai activities\" log lines "
             "(%d turns timed of %d markers)", t->timed_turns, markers);
    return t->turns > 0 ? 0 : -1;
}

/* Open Doctrines' headless eval prints "[EVAL]   turn T/N  A alive ... (S s/turn)" every
   250 turns, S being its own average since the map was ready. The last such line gives
   T turns in S*T seconds. */
static int parse_opendoctrines(const ojh_line *lines, size_t count, ojh_tpm *t) {
    int last_turns = 0;
    double last_average = 0;
    for (size_t i = 0; i < count; i++) {
        const char *s = lines[i].text;
        if (strncmp(s, "[EVAL]", 6) != 0) continue;
        const char *turn = strstr(s, " turn ");
        const char *open = strrchr(s, '(');
        if (turn && open && strstr(s, "s/turn)")) {
            int done = 0, planned = 0;
            double average = 0;
            if (sscanf(turn + 6, "%d/%d", &done, &planned) == 2 && sscanf(open + 1, "%lf", &average) == 1) {
                last_turns = done;
                last_average = average;
            }
            continue;
        }
        double v;
        if (strstr(s, "[EVAL] map ") && number_after(s, "countries=", &v)) {
            t->players = (int)v;
            t->boot_seconds = lines[i].t;
        }
    }
    t->turns = last_turns;
    t->play_seconds = last_average * last_turns;
    snprintf(t->how, sizeof t->how,
             "Open Doctrines' own [EVAL] progress line: its average seconds per turn over the first %d turns "
             "(printed every 250 turns)", last_turns);
    return t->turns > 0 ? 0 : -1;
}

/* Where "OJH " starts a protocol word in a line: at the start, or after a space, tab, ']'
   or ':' so a logger's prefix ("[12:00:01] OJH turn 3") does not hide it. */
static const char *protocol_word(const char *s) {
    for (const char *p = strstr(s, "OJH "); p; p = strstr(p + 1, "OJH ")) {
        if (p == s || p[-1] == ' ' || p[-1] == '\t' || p[-1] == ']' || p[-1] == ':') return p + 4;
    }
    return NULL;
}

static int word_is(const char *w, const char *word) {
    size_t n = strlen(word);
    return strncmp(w, word, n) == 0 && (w[n] == '\0' || w[n] == ' ' || w[n] == '\r');
}

static void copy_identity(const ojh_gamespec *spec, ojh_tpm *t) {
    snprintf(t->id, sizeof t->id, "%s", spec->id);
    snprintf(t->name, sizeof t->name, "%s", spec->name);
    snprintf(t->version, sizeof t->version, "%s", spec->version);
    snprintf(t->license, sizeof t->license, "%s", spec->license);
    snprintf(t->homepage, sizeof t->homepage, "%s", spec->homepage);
}

int ojh_tpm_parse_spec(const ojh_gamespec *spec, const ojh_line *lines, size_t count, ojh_tpm *t) {
    reset(OJH_GAME_CUSTOM, t);
    copy_identity(spec, t);
    int markers = 0, reported_times = 0;
    double previous = -1;
    for (size_t i = 0; i < count; i++) {
        const char *s = lines[i].text;
        double v;
        if (spec->turns_from == OJH_TURNS_PROTOCOL) {
            const char *w = protocol_word(s);
            if (!w) continue;
            if (word_is(w, "ready")) {
                if (t->boot_seconds < 0) t->boot_seconds = lines[i].t;
                previous = lines[i].t;
            } else if (word_is(w, "turn")) {
                int n = 0;
                double seconds = 0;
                int got = sscanf(w + 4, "%d %lf", &n, &seconds);
                markers++;
                if (got == 2 && seconds >= 0) {
                    push_turn(t, seconds); /* the game timed its own turn */
                    reported_times++;
                } else if (got >= 1 && previous >= 0) {
                    push_turn(t, lines[i].t - previous);
                } else if (got >= 1 && t->boot_seconds < 0) {
                    t->boot_seconds = lines[i].t; /* no ready line: the first turn line starts the clock */
                }
                previous = lines[i].t;
            } else if (word_is(w, "players")) {
                int n = atoi(w + 7);
                if (n > 0) t->players = n;
            } else if (word_is(w, "regions")) {
                char kind[16] = "";
                long n = 0;
                if (sscanf(w + 7, "%ld %15s", &n, kind) >= 1 && n > 0) {
                    t->regions = n;
                    if (*kind) snprintf(t->region_kind, sizeof t->region_kind, "%s", kind);
                }
            }
            continue;
        }
        if (spec->players_after && number_after(s, spec->players_after, &v) && v > 0) t->players = (int)v;
        if (spec->regions_after && number_after(s, spec->regions_after, &v) && v > 0) t->regions = (long)v;
        if (spec->stream != OJH_STREAM_EITHER && lines[i].stream != spec->stream) continue;
        if (spec->game_starts && t->boot_seconds < 0 && strstr(s, spec->game_starts)) {
            t->boot_seconds = lines[i].t;
            previous = lines[i].t;
        }
        if (strstr(s, spec->turn_ends)) {
            markers++;
            if (previous >= 0) push_turn(t, lines[i].t - previous);
            else t->boot_seconds = lines[i].t; /* no start line: the first marker starts the clock */
            previous = lines[i].t;
        }
    }
    if (spec->turns_from == OJH_TURNS_PROTOCOL && (spec->players_after || spec->regions_after)) {
        for (size_t i = 0; i < count; i++) {
            double v;
            if (spec->players_after && !t->players && number_after(lines[i].text, spec->players_after, &v) && v > 0) t->players = (int)v;
            if (spec->regions_after && !t->regions && number_after(lines[i].text, spec->regions_after, &v) && v > 0) t->regions = (long)v;
        }
    }
    if (!t->players) t->players = spec->players;
    if (!t->regions) t->regions = spec->regions;
    if (!*t->region_kind) snprintf(t->region_kind, sizeof t->region_kind, "%s", *spec->region_kind ? spec->region_kind : "regions");
    t->turns = t->timed_turns;
    if (spec->turns_from == OJH_TURNS_PROTOCOL) {
        snprintf(t->how, sizeof t->how, "%s's own \"OJH turn\" lines, %s (%d turns timed of %d turn lines)", t->name,
                 reported_times == t->timed_turns && reported_times > 0 ? "each carrying the game's own time for its turn"
                 : reported_times > 0 ? "partly with the game's own turn times and partly the gaps between lines"
                                      : "the gaps between them", t->timed_turns, markers);
    } else {
        snprintf(t->how, sizeof t->how, "gaps between %s's \"%.80s\" lines (%d turns timed of %d markers)", t->name,
                 spec->turn_ends, t->timed_turns, markers);
    }
    return t->turns > 0 ? 0 : -1;
}

int ojh_tpm_parse(ojh_game game, const ojh_line *lines, size_t count, ojh_tpm *out) {
    reset(game, out);
    switch (game) {
        case OJH_GAME_GD5: {
            int r = parse_json_driver(lines, count, "\"total_seconds\"", "\"regions\"", "\"nations_at_start\"",
                                      "provinces", out);
            snprintf(out->how, sizeof out->how,
                     "OJH's GD5 driver times each turn through GD5's own turn manager, redraw included "
                     "(%d turns, LLM diplomacy off)", out->turns);
            return r;
        }
        case OJH_GAME_UNCIV: {
            int r = parse_json_driver(lines, count, "\"seconds\"", "\"tiles\"", "\"civs\"", "tiles", out);
            snprintf(out->how, sizeof out->how,
                     "OJH's Unciv driver times each GameInfo.nextTurn call (%d turns)", out->turns);
            return r;
        }
        case OJH_GAME_FREECIV:
            return parse_freeciv(lines, count, out);
        case OJH_GAME_OPENDOCTRINES:
            return parse_opendoctrines(lines, count, out);
        default:
            return -1;
    }
}

/* ---------------------------------------------------------------- running */

static void join_path(char *out, size_t n, const char *dir, const char *name) {
    size_t len = strlen(dir);
    snprintf(out, n, "%s%s%s", dir, (len > 0 && (dir[len - 1] == '/' || dir[len - 1] == '\\')) ? "" : "/", name);
}

static int fail(char *error, size_t len, const char *message) {
    if (error && len) snprintf(error, len, "%s", message);
    return -1;
}

/* Runs argv to completion and parses what it printed. */
static int run_and_parse(ojh_game game, const ojh_gamespec *spec, const char *const *argv, const char *const *env,
                         const char *cwd, double timeout, ojh_tpm *out, char *error, size_t error_len) {
    double t0 = ojh_now();
    ojh_run *r = ojh_run_start(argv, env, cwd);
    if (!r) return fail(error, error_len, "the program did not start (is the path right?)");
    int code = ojh_run_wait(r, timeout);
    double wall = ojh_now() - t0;
    size_t count = ojh_run_line_count(r);
    ojh_line *lines = calloc(count ? count : 1, sizeof *lines);
    if (!lines) {
        ojh_run_free(r);
        return fail(error, error_len, "out of memory");
    }
    for (size_t i = 0; i < count; i++) lines[i] = *ojh_run_line(r, i);
    int parsed = spec ? ojh_tpm_parse_spec(spec, lines, count, out) : ojh_tpm_parse(game, lines, count, out);
    out->exit_code = code;
    out->wall_seconds = wall;
    if (parsed != 0 && error && error_len) {
        /* The last few lines usually say why. */
        size_t at = 0;
        at += (size_t)snprintf(error, error_len, "no turns found (exit %d); last lines:", code);
        for (size_t i = count > 5 ? count - 5 : 0; i < count && at < error_len; i++) {
            at += (size_t)snprintf(error + at, error_len - at, " | %s", lines[i].text);
        }
    }
    free(lines);
    ojh_run_free(r);
    if (code == -2) return fail(error, error_len, "timed out");
    return parsed;
}

int ojh_tpm_run(ojh_game game, const ojh_tpm_options *o, ojh_tpm *out, char *error, size_t error_len) {
    reset(game, out);
    char turns[16], seed[16], players[16];
    snprintf(turns, sizeof turns, "%d", o->turns);
    snprintf(seed, sizeof seed, "%u", o->seed);
    snprintf(players, sizeof players, "%d", o->players);
    double timeout = o->timeout_seconds > 0 ? o->timeout_seconds : 3600;

    switch (game) {
        case OJH_GAME_OPENDOCTRINES: {
            if (!o->od_server || !o->od_data) return fail(error, error_len, "needs --od-server and --od-data");
            const char *argv[] = {o->od_server, "--eval-ai", "1", turns, seed, "2", "--data", o->od_data, NULL};
            return run_and_parse(game, NULL, argv, NULL, NULL, timeout, out, error, error_len);
        }
        case OJH_GAME_GD5: {
            if (!o->gd5_python || !o->gd5_dir || !o->drivers_dir) {
                return fail(error, error_len, "needs --gd5-python, --gd5-dir and --drivers");
            }
            char driver[4096];
            join_path(driver, sizeof driver, o->drivers_dir, "gd5_tpm.py");
            const char *argv[] = {o->gd5_python, driver, "--gd5", o->gd5_dir, "--turns", turns, NULL};
            return run_and_parse(game, NULL, argv, NULL, NULL, timeout, out, error, error_len);
        }
        case OJH_GAME_FREECIV: {
            if (!o->freeciv_server || !o->work_dir) return fail(error, error_len, "needs --freeciv-server and --work");
            char dir[4096], saves[4200], script[4200];
            join_path(dir, sizeof dir, o->work_dir, "freeciv");
            join_path(saves, sizeof saves, dir, "saves");
            join_path(script, sizeof script, dir, "autogame.serv");
            ojh_make_dir(o->work_dir);
            ojh_make_dir(dir);
            ojh_make_dir(saves);
            FILE *f = fopen(script, "wb");
            if (!f) return fail(error, error_len, "cannot write the Freeciv start-up script");
            /* Freeciv's own recipe for a server-only autogame (doc/HACKING). */
            fprintf(f, "set gameseed %u\nset mapseed %u\nset timeout -1\nset minplayers 0\nset ec_turns 0\n"
                       "set aifill %d\nset endturn %d\nset autosaves \"\"\nhard\ncreate Bench\nstart\n",
                    o->seed, o->seed, o->players, o->turns);
            fclose(f);
            const char *argv[] = {o->freeciv_server, "-e", "-p", "55600", "-s", saves, "-r", script, "-d", "v", NULL};
            const char *env[] = {"LC_ALL=C", "LANG=C", NULL};
            return run_and_parse(game, NULL, argv, env, dir, timeout, out, error, error_len);
        }
        case OJH_GAME_UNCIV: {
            if (!o->unciv_jar || !o->java || !o->javac || !o->jar_tool || !o->drivers_dir || !o->work_dir) {
                return fail(error, error_len, "needs --unciv-jar, --java, --javac, --jar, --drivers and --work");
            }
            char classes[4096], source[4096], assets[4096], classpath[8400];
            join_path(classes, sizeof classes, o->work_dir, "unciv-driver");
            join_path(source, sizeof source, o->drivers_dir, "unciv/UncivTpm.java");
            join_path(assets, sizeof assets, o->work_dir, "unciv-assets");
            ojh_make_dir(o->work_dir);
            ojh_make_dir(classes);
            ojh_make_dir(assets);
            /* Unciv reads its rulesets from jsons/ under the working directory; the jar has them. */
            const char *extract[] = {o->jar_tool, "xf", o->unciv_jar, "jsons", NULL};
            ojh_run *x = ojh_run_start(extract, NULL, assets);
            if (!x) return fail(error, error_len, "the jar tool did not start");
            int extracted = ojh_run_wait(x, 600);
            ojh_run_free(x);
            if (extracted != 0) return fail(error, error_len, "could not extract Unciv's rulesets from the jar");
            const char *compile[] = {o->javac, "-nowarn", "-cp", o->unciv_jar, "-d", classes, source, NULL};
            ojh_run *c = ojh_run_start(compile, NULL, NULL);
            if (!c) return fail(error, error_len, "javac did not start");
            int compiled = ojh_run_wait(c, 600);
            ojh_run_free(c);
            if (compiled != 0) return fail(error, error_len, "the Unciv driver did not compile");
#ifdef _WIN32
            snprintf(classpath, sizeof classpath, "%s;%s", classes, o->unciv_jar);
#else
            snprintf(classpath, sizeof classpath, "%s:%s", classes, o->unciv_jar);
#endif
            const char *argv[] = {o->java, "-Djava.awt.headless=true", "-cp", classpath, "UncivTpm", players, turns,
                                  "small", NULL};
            return run_and_parse(game, NULL, argv, NULL, assets, timeout, out, error, error_len);
        }
        default:
            return fail(error, error_len, "unknown game");
    }
}

static void ignore_name(const char *name, void *user) {
    (void)name;
    (void)user;
}

int ojh_tpm_run_spec(const ojh_gamespec *spec, const ojh_tpm_options *o, ojh_tpm *out, char *error,
                     size_t error_len) {
    reset(OJH_GAME_CUSTOM, out);
    copy_identity(spec, out);
    ojh_spec_values values = {o->turns, o->seed, o->players, o->work_dir, o->ojh_path};
    int status = -1;
    char **argv = calloc((size_t)spec->command_count + 1, sizeof *argv);
    char **env = calloc((size_t)spec->environment_count + 1, sizeof *env);
    char *cwd = NULL;
    if (!argv || !env) {
        fail(error, error_len, "out of memory");
        goto done;
    }
    for (int i = 0; i < spec->command_count; i++) {
        char *filled = ojh_gamespec_expand(spec->command[i], spec, &values);
        if (!filled) {
            fail(error, error_len, "out of memory");
            goto done;
        }
        if (i == 0) {
            /* "./MyGame" is next to the spec; "python3" is found on PATH. */
            argv[0] = ojh_gamespec_path(spec, filled, 1);
            free(filled);
            if (!argv[0]) {
                fail(error, error_len, "out of memory");
                goto done;
            }
        } else {
            argv[i] = filled;
        }
    }
    for (int i = 0; i < spec->environment_count; i++) {
        env[i] = ojh_gamespec_expand(spec->environment[i], spec, &values);
        if (!env[i]) {
            fail(error, error_len, "out of memory");
            goto done;
        }
    }
    if (spec->working_directory) {
        char *filled = ojh_gamespec_expand(spec->working_directory, spec, &values);
        cwd = filled ? ojh_gamespec_path(spec, filled, 0) : NULL;
        free(filled);
    } else {
        cwd = ojh_gamespec_path(spec, ".", 0);
    }
    if (!cwd) {
        fail(error, error_len, "out of memory");
        goto done;
    }
    if (o->work_dir) ojh_make_dir(o->work_dir);
    if (ojh_list_dir(cwd, ignore_name, NULL) != 0) {
        fail(error, error_len, "the game's working folder does not exist");
        if (error && error_len) snprintf(error, error_len, "the game's working folder %s does not exist", cwd);
        goto done;
    }
    double timeout = o->timeout_seconds > 0 ? o->timeout_seconds : spec->timeout_seconds;
    status = run_and_parse(OJH_GAME_CUSTOM, spec, (const char *const *)argv, (const char *const *)env, cwd, timeout,
                           out, error, error_len);
    if (status != 0 && out->exit_code == -1 && error && !*error) fail(error, error_len, "the game did not start");
done:
    if (argv) {
        for (int i = 0; i < spec->command_count; i++) free(argv[i]);
        free(argv);
    }
    if (env) {
        for (int i = 0; i < spec->environment_count; i++) free(env[i]);
        free(env);
    }
    free(cwd);
    return status;
}

/* ---------------------------------------------------------------- results */

double ojh_tpm_value(const ojh_tpm *t) {
    return t->turns > 0 && t->play_seconds > 0 ? t->turns / (t->play_seconds / 60.0) : 0.0;
}

static int compare_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* Median of turn_seconds[from, to). */
static double median_of(const double *values, int from, int to) {
    int n = to - from;
    if (n <= 0) return 0;
    double *copy = malloc((size_t)n * sizeof *copy);
    if (!copy) return 0;
    memcpy(copy, values + from, (size_t)n * sizeof *copy);
    qsort(copy, (size_t)n, sizeof *copy, compare_double);
    double m = copy[n / 2];
    free(copy);
    return m;
}

void ojh_tpm_json(ojh_json *w, const ojh_tpm *t) {
    double tpm = ojh_tpm_value(t);
    ojh_json_object(w);
    ojh_json_key(w, "game"); ojh_json_string(w, *t->id ? t->id : ojh_game_id(t->game));
    ojh_json_key(w, "name"); ojh_json_string(w, *t->name ? t->name : ojh_game_name(t->game));
    if (t->game == OJH_GAME_CUSTOM) {
        ojh_json_key(w, "from_spec"); ojh_json_bool(w, 1);
        ojh_json_key(w, "version"); if (*t->version) ojh_json_string(w, t->version); else ojh_json_null(w);
        ojh_json_key(w, "license"); if (*t->license) ojh_json_string(w, t->license); else ojh_json_null(w);
        ojh_json_key(w, "homepage"); if (*t->homepage) ojh_json_string(w, t->homepage); else ojh_json_null(w);
    }
    ojh_json_key(w, "exit_code"); ojh_json_int(w, t->exit_code);
    ojh_json_key(w, "turns"); ojh_json_int(w, t->turns);
    ojh_json_key(w, "play_seconds"); ojh_json_double(w, t->play_seconds, 3);
    ojh_json_key(w, "boot_seconds");
    if (t->boot_seconds >= 0) ojh_json_double(w, t->boot_seconds, 3);
    else ojh_json_null(w);
    ojh_json_key(w, "wall_seconds"); ojh_json_double(w, t->wall_seconds, 3);
    ojh_json_key(w, "tpm"); ojh_json_double(w, tpm, 2);
    ojh_json_key(w, "players");
    if (t->players > 0) ojh_json_int(w, t->players);
    else ojh_json_null(w);
    ojh_json_key(w, "tpm_x_players");
    if (t->players > 0) ojh_json_double(w, tpm * t->players, 1);
    else ojh_json_null(w);
    ojh_json_key(w, "regions");
    if (t->regions > 0) ojh_json_int(w, t->regions);
    else ojh_json_null(w);
    ojh_json_key(w, "region_kind");
    if (t->regions > 0) ojh_json_string(w, t->region_kind);
    else ojh_json_null(w);
    ojh_json_key(w, "tpm_x_regions");
    if (t->regions > 0) ojh_json_double(w, tpm * (double)t->regions, 0);
    else ojh_json_null(w);
    ojh_json_key(w, "per_turn");
    if (t->timed_turns > 0) {
        double *sorted = malloc((size_t)t->timed_turns * sizeof *sorted);
        ojh_json_object(w);
        if (sorted) {
            memcpy(sorted, t->turn_seconds, (size_t)t->timed_turns * sizeof *sorted);
            qsort(sorted, (size_t)t->timed_turns, sizeof *sorted, compare_double);
            int third = t->timed_turns / 3;
            ojh_json_key(w, "timed_turns"); ojh_json_int(w, t->timed_turns);
            ojh_json_key(w, "fastest_seconds"); ojh_json_double(w, sorted[0], 4);
            ojh_json_key(w, "median_seconds"); ojh_json_double(w, sorted[t->timed_turns / 2], 4);
            ojh_json_key(w, "p95_seconds"); ojh_json_double(w, sorted[(int)((t->timed_turns - 1) * 0.95)], 4);
            ojh_json_key(w, "slowest_seconds"); ojh_json_double(w, sorted[t->timed_turns - 1], 4);
            if (third > 0) {
                ojh_json_key(w, "early_median_seconds"); ojh_json_double(w, median_of(t->turn_seconds, 0, third), 4);
                ojh_json_key(w, "middle_median_seconds");
                ojh_json_double(w, median_of(t->turn_seconds, third, 2 * third), 4);
                ojh_json_key(w, "late_median_seconds");
                ojh_json_double(w, median_of(t->turn_seconds, 2 * third, t->timed_turns), 4);
            }
            free(sorted);
        }
        ojh_json_end_object(w);
    } else {
        ojh_json_null(w);
    }
    ojh_json_key(w, "how"); ojh_json_string(w, t->how);
    ojh_json_end_object(w);
}

void ojh_tpm_free(ojh_tpm *t) {
    if (!t) return;
    free(t->turn_seconds);
    t->turn_seconds = NULL;
    t->timed_turns = 0;
}
