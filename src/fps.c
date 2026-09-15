#include "platform.h"
#include "fps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "footprint.h"
#include "runner.h"

static int fail(char *error, size_t len, const char *message) {
    if (error && len) snprintf(error, len, "%s", message);
    return -1;
}

static const char *protocol_value(const char *s, const char *word) {
    size_t n = strlen(word);
    for (const char *p = strstr(s, "OJH "); p; p = strstr(p + 1, "OJH ")) {
        int starts = p == s || p[-1] == ' ' || p[-1] == '\t' || p[-1] == ']' || p[-1] == ':';
        if (starts && strncmp(p + 4, word, n) == 0 && (p[4 + n] == ' ' || p[4 + n] == '\0')) return p + 4 + n;
    }
    return NULL;
}

static void copy_rest(char *out, size_t n, const char *v) {
    while (*v == ' ') v++;
    snprintf(out, n, "%s", v);
    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == ' ' || out[len - 1] == '\r')) out[--len] = '\0';
}

void ojh_fps_parse_line(ojh_fps *f, const char *text) {
    const char *v;
    if ((v = protocol_value(text, "scene"))) {
        ojh_fps_scene s;
        memset(&s, 0, sizeof s);
        if (sscanf(v, " %31s %d %lf %lf %lf %lf %lf", s.name, &s.frames, &s.seconds, &s.p50_ms, &s.p95_ms, &s.p99_ms,
                   &s.low1_fps) != 7 || s.frames <= 0 || s.seconds <= 0) {
            return;
        }
        for (int i = 0; i < f->scene_count; i++) {
            if (strcmp(f->scenes[i].name, s.name) == 0) {
                f->scenes[i] = s;
                return;
            }
        }
        if (f->scene_count < OJH_FPS_MAX_SCENES) f->scenes[f->scene_count++] = s;
    } else if ((v = protocol_value(text, "noscene"))) {
        ojh_fps_missing m;
        memset(&m, 0, sizeof m);
        int used = 0;
        if (sscanf(v, " %31s%n", m.name, &used) != 1) return;
        copy_rest(m.reason, sizeof m.reason, v + used);
        if (f->missing_count < OJH_FPS_MAX_SCENES) f->missing[f->missing_count++] = m;
    } else if ((v = protocol_value(text, "renderer"))) {
        copy_rest(f->renderer, sizeof f->renderer, v);
    } else if ((v = protocol_value(text, "resolution"))) {
        copy_rest(f->resolution, sizeof f->resolution, v);
    } else if ((v = protocol_value(text, "vsync"))) {
        copy_rest(f->vsync, sizeof f->vsync, v);
    }
}

static int compare_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

double ojh_fps_map_average(const ojh_fps *f) {
    double values[OJH_FPS_MAX_SCENES];
    int n = 0;
    for (int i = 0; i < f->scene_count; i++) {
        if (strncmp(f->scenes[i].name, "map-", 4) == 0) values[n++] = f->scenes[i].frames / f->scenes[i].seconds;
    }
    if (n == 0) return 0;
    qsort(values, (size_t)n, sizeof values[0], compare_double);
    return n % 2 ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2;
}

double ojh_fps_worst_low(const ojh_fps *f) {
    double worst = 0;
    for (int i = 0; i < f->scene_count; i++) {
        if (i == 0 || f->scenes[i].low1_fps < worst) worst = f->scenes[i].low1_fps;
    }
    return worst;
}

double ojh_fps_worst_p99_seconds(const ojh_fps *f) {
    double worst = 0;
    for (int i = 0; i < f->scene_count; i++) {
        if (f->scenes[i].p99_ms > worst) worst = f->scenes[i].p99_ms;
    }
    return worst / 1000.0;
}

static void reset(ojh_fps *f, ojh_game game) {
    memset(f, 0, sizeof *f);
    snprintf(f->id, sizeof f->id, "%s", ojh_game_id(game));
    snprintf(f->name, sizeof f->name, "%s", ojh_game_name(game));
    f->exit_code = -1;
}

static int run_scenes(const char *const *argv, const char *const *env, const char *cwd, double timeout, ojh_fps *f,
                      char *error, size_t error_len) {
    ojh_run *r = ojh_run_start(argv, env, cwd);
    if (!r) return fail(error, error_len, "the program did not start (is the path right?)");
    int code = ojh_run_wait(r, timeout);
    size_t count = ojh_run_line_count(r);
    for (size_t i = 0; i < count; i++) ojh_fps_parse_line(f, ojh_run_line(r, i)->text);
    f->exit_code = code;
    if ((code != 0 || f->scene_count == 0) && error && error_len) {
        size_t at = (size_t)snprintf(error, error_len, "exit %d, %d scenes; last lines:", code, f->scene_count);
        for (size_t i = count > 4 ? count - 4 : 0; i < count && at < error_len; i++) {
            at += (size_t)snprintf(error + at, error_len - at, " | %s", ojh_run_line(r, i)->text);
        }
    }
    ojh_run_free(r);
    return code == 0 && f->scene_count > 0 ? 0 : -1;
}

int ojh_fps_run(ojh_game game, const ojh_tpm_options *o, double seconds, ojh_fps *out, char *error, size_t error_len) {
    reset(out, game);
    char seconds_text[32], turns_text[16];
    snprintf(seconds_text, sizeof seconds_text, "%.2f", seconds > 0 ? seconds : 5.0);
    snprintf(turns_text, sizeof turns_text, "%d", o->turns);
    double timeout = o->timeout_seconds > 0 ? o->timeout_seconds : 1800;
    switch (game) {
        case OJH_GAME_GD5: {
            if (!o->gd5_python || !o->gd5_dir || !o->drivers_dir) {
                return fail(error, error_len, "needs --gd5-python, --gd5-dir and --drivers");
            }
            char driver[4400], tool[4400], seed_text[16];
            const char *scenario = o->gd5_scenario ? o->gd5_scenario : "scenarios/historical/1939";
            snprintf(seed_text, sizeof seed_text, "%u", o->seed);
            if (ojh_gd5_tool(o->gd5_dir, tool, sizeof tool)) {
                const char *argv[] = {o->gd5_python, tool, "--scenario", scenario, "--seed", seed_text, "fps", "--seconds",
                                      seconds_text, "--late-turns", turns_text, NULL};
                int status = run_scenes(argv, NULL, NULL, timeout, out, error, error_len);
                snprintf(out->how, sizeof out->how,
                         "Greater Diplomacy 5's own map_tools/ojh_benchmark.py fps mode: the real game window brought to the "
                         "front, no frame cap, every frame of each scene timed for %s s after settling, on %.120s; pygame "
                         "draws in software; the late-game map comes after %d turns", seconds_text, scenario, o->turns);
                return status;
            }
            snprintf(driver, sizeof driver, "%s/gd5_fps.py", o->drivers_dir);
            const char *argv[] = {o->gd5_python, driver, "--gd5", o->gd5_dir, "--seconds", seconds_text, "--turns",
                                  turns_text, NULL};
            int status = run_scenes(argv, NULL, NULL, timeout, out, error, error_len);
            snprintf(out->how, sizeof out->how,
                     "OJH's GD5 driver boots the real game with SDL's dummy video driver and times every update and draw "
                     "of each scene for %s s after a warm-up, with no frame cap; pygame draws in software, so this is "
                     "GD5's own drawing cost with no display in the way. The late-game map comes after %d turns",
                     seconds_text, o->turns);
            return status;
        }
        case OJH_GAME_UNCIV: {
            char classpath[9000], assets[4400], players_text[16];
            snprintf(players_text, sizeof players_text, "%d", o->players > 0 ? o->players : 8);
            if (ojh_unciv_prepare(o, classpath, sizeof classpath, assets, sizeof assets, error, error_len) != 0) return -1;
            const char *argv[] = {o->java,
#ifdef __APPLE__
                                  "-XstartOnFirstThread",
#endif
                                  "-cp", classpath, "UncivFps", seconds_text, turns_text, players_text,
                                  o->map_size ? o->map_size : "small", NULL};
            int status = run_scenes(argv, NULL, assets, timeout, out, error, error_len);
            snprintf(out->how, sizeof out->how,
                     "OJH's Unciv driver opens Unciv's own desktop window (LWJGL3, vsync off, no frame cap), starts a game "
                     "and times every rendered frame of each scene for %s s after a warm-up; the late-game map comes after "
                     "%d turns", seconds_text, o->turns);
            return status;
        }
        case OJH_GAME_FREECIV:
            snprintf(out->how, sizeof out->how,
                     "not measured: Freeciv's client redraws only when something on the map changes, so it has no steady "
                     "frame rate to time");
            return fail(error, error_len, out->how);
        default: {
            if (!o->od_game || !o->od_data) return fail(error, error_len, "needs --od-game and --od-data");
            char save[4400];
            if (o->od_save) snprintf(save, sizeof save, "%s", o->od_save);
            else snprintf(save, sizeof save, "%s/saves/Modern Day.odsv", o->od_data);
            const char *argv[] = {o->od_game, "--ojh-fps", seconds_text, save, turns_text, NULL};
            int status = run_scenes(argv, NULL, NULL, timeout, out, error, error_len);
            snprintf(out->how, sizeof out->how,
                     "Open Doctrines' own --ojh-fps mode: its real window with vsync and the frame cap off, every frame "
                     "timed for %s s per scene after two seconds of settling, on the %.200s world; the late-game map comes "
                     "after %d turns", seconds_text, save, o->turns);
            return status;
        }
    }
}

int ojh_fps_run_spec(const ojh_gamespec *spec, const ojh_tpm_options *o, double seconds, ojh_fps *out, char *error,
                     size_t error_len) {
    reset(out, OJH_GAME_CUSTOM);
    snprintf(out->id, sizeof out->id, "%s", spec->id);
    snprintf(out->name, sizeof out->name, "%s", spec->name);
    snprintf(out->version, sizeof out->version, "%s", spec->version);
    if (spec->fps_count == 0) {
        snprintf(out->how, sizeof out->how, "not measured: the spec has no fps_command");
        return fail(error, error_len, "the spec has no \"fps_command\", so there is no frame rate to measure");
    }
    ojh_spec_values values = {o->turns, o->seed, o->players, o->work_dir, o->ojh_path};
    char **argv = calloc((size_t)spec->fps_count + 1, sizeof *argv);
    if (!argv) return fail(error, error_len, "out of memory");
    for (int i = 0; i < spec->fps_count; i++) {
        char *filled = ojh_gamespec_expand(spec->fps_command[i], spec, &values);
        argv[i] = i == 0 && filled ? ojh_gamespec_path(spec, filled, 1) : filled;
        if (i == 0) free(filled);
    }
    char *cwd = ojh_gamespec_path(spec, ".", 0);
    int status = run_scenes((const char *const *)argv, NULL, cwd,
                            o->timeout_seconds > 0 ? o->timeout_seconds : spec->timeout_seconds, out, error, error_len);
    snprintf(out->how, sizeof out->how, "the spec's fps command and its OJH scene lines, about %.1f s per scene",
             seconds > 0 ? seconds : 5.0);
    for (int i = 0; i < spec->fps_count; i++) free(argv[i]);
    free(argv);
    free(cwd);
    return status;
}

void ojh_fps_json(ojh_json *w, const ojh_fps *f) {
    ojh_json_object(w);
    ojh_json_key(w, "game"); ojh_json_string(w, f->id);
    ojh_json_key(w, "name"); ojh_json_string(w, f->name);
    if (*f->version) {
        ojh_json_key(w, "version"); ojh_json_string(w, f->version);
    }
    ojh_json_key(w, "exit_code"); ojh_json_int(w, f->exit_code);
    ojh_json_key(w, "renderer");
    if (*f->renderer) ojh_json_string(w, f->renderer);
    else ojh_json_null(w);
    ojh_json_key(w, "resolution");
    if (*f->resolution) ojh_json_string(w, f->resolution);
    else ojh_json_null(w);
    ojh_json_key(w, "vsync");
    if (*f->vsync) ojh_json_string(w, f->vsync);
    else ojh_json_null(w);
    ojh_json_key(w, "map_average_fps");
    if (ojh_fps_map_average(f) > 0) ojh_json_double(w, ojh_fps_map_average(f), 2);
    else ojh_json_null(w);
    ojh_json_key(w, "one_percent_low_fps");
    if (f->scene_count) ojh_json_double(w, ojh_fps_worst_low(f), 2);
    else ojh_json_null(w);
    ojh_json_key(w, "p99_frame_seconds");
    if (f->scene_count) ojh_json_double(w, ojh_fps_worst_p99_seconds(f), 5);
    else ojh_json_null(w);
    ojh_json_key(w, "scenes");
    ojh_json_object(w);
    for (int i = 0; i < f->scene_count; i++) {
        const ojh_fps_scene *s = &f->scenes[i];
        ojh_json_key(w, s->name);
        ojh_json_object(w);
        ojh_json_key(w, "frames"); ojh_json_int(w, s->frames);
        ojh_json_key(w, "seconds"); ojh_json_double(w, s->seconds, 3);
        ojh_json_key(w, "average_fps"); ojh_json_double(w, s->frames / s->seconds, 2);
        ojh_json_key(w, "one_percent_low_fps"); ojh_json_double(w, s->low1_fps, 2);
        ojh_json_key(w, "p50_ms"); ojh_json_double(w, s->p50_ms, 3);
        ojh_json_key(w, "p95_ms"); ojh_json_double(w, s->p95_ms, 3);
        ojh_json_key(w, "p99_ms"); ojh_json_double(w, s->p99_ms, 3);
        ojh_json_end_object(w);
    }
    ojh_json_end_object(w);
    ojh_json_key(w, "not_measured");
    ojh_json_object(w);
    for (int i = 0; i < f->missing_count; i++) {
        ojh_json_key(w, f->missing[i].name);
        ojh_json_string(w, f->missing[i].reason);
    }
    ojh_json_end_object(w);
    ojh_json_key(w, "how"); ojh_json_string(w, f->how);
    ojh_json_end_object(w);
}
