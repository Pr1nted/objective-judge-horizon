#include "platform.h"
#include "footprint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runner.h"

static const char *const SKIP[] = {".git", "__pycache__", ".DS_Store", NULL};

static int fail(char *error, size_t len, const char *message) {
    if (error && len) snprintf(error, len, "%s", message);
    return -1;
}

static void reset(ojh_footprint *f, ojh_game game) {
    memset(f, 0, sizeof *f);
    f->game = game;
    snprintf(f->id, sizeof f->id, "%s", ojh_game_id(game));
    snprintf(f->name, sizeof f->name, "%s", ojh_game_name(game));
    f->save_seconds = -1;
    f->load_seconds = -1;
    f->exit_code = -1;
}

static void add_install(ojh_footprint *f, const char *path) {
    uint64_t bytes = 0, files = 0;
    if (ojh_path_size(path, SKIP, &bytes, &files) != 0) return;
    f->install_bytes += bytes;
    f->install_files += files;
    f->has_install = 1;
    size_t at = strlen(f->install_paths);
    if (at < sizeof f->install_paths) {
        snprintf(f->install_paths + at, sizeof f->install_paths - at, "%s%s", at ? "; " : "", path);
    }
}

static void join_path(char *out, size_t n, const char *dir, const char *name) {
    size_t len = strlen(dir);
    snprintf(out, n, "%s%s%s", dir, (len > 0 && (dir[len - 1] == '/' || dir[len - 1] == '\\')) ? "" : "/", name);
}

static const char *protocol_value(const char *s, const char *word) {
    size_t n = strlen(word);
    for (const char *p = strstr(s, "OJH "); p; p = strstr(p + 1, "OJH ")) {
        int starts = p == s || p[-1] == ' ' || p[-1] == '\t' || p[-1] == ']' || p[-1] == ':';
        if (starts && strncmp(p + 4, word, n) == 0 && (p[4 + n] == ' ' || p[4 + n] == '\0')) return p + 4 + n;
    }
    return NULL;
}

static int run_and_read(const char *const *argv, const char *const *env, const char *cwd, double timeout,
                        ojh_footprint *f, double *wall, char *error, size_t error_len) {
    double t0 = ojh_now();
    ojh_run *r = ojh_run_start(argv, env, cwd);
    if (!r) return fail(error, error_len, "the program did not start (is the path right?)");
    int code = ojh_run_wait(r, timeout);
    if (wall) *wall = ojh_now() - t0;
    size_t count = ojh_run_line_count(r);
    for (size_t i = 0; i < count; i++) {
        const char *text = ojh_run_line(r, i)->text;
        const char *v;
        if ((v = protocol_value(text, "save"))) {
            unsigned long long bytes = 0;
            double seconds = -1;
            int got = sscanf(v, "%llu %lf", &bytes, &seconds);
            if (got >= 1) {
                f->save_bytes = bytes;
                f->has_save = 1;
            }
            if (got == 2 && seconds >= 0) f->save_seconds = seconds;
        } else if ((v = protocol_value(text, "load"))) {
            double seconds;
            if (sscanf(v, "%lf", &seconds) == 1 && seconds >= 0) f->load_seconds = seconds;
        }
    }
    if (code != 0 && error && error_len) {
        size_t at = (size_t)snprintf(error, error_len, "exit %d; last lines:", code);
        for (size_t i = count > 4 ? count - 4 : 0; i < count && at < error_len; i++) {
            at += (size_t)snprintf(error + at, error_len - at, " | %s", ojh_run_line(r, i)->text);
        }
    }
    ojh_run_free(r);
    return code;
}

typedef struct {
    char newest[4400];
    const char *dir;
    uint64_t bytes;
} largest_file;

static void pick_largest(const char *name, void *user) {
    largest_file *l = user;
    char path[4400];
    join_path(path, sizeof path, l->dir, name);
    uint64_t bytes = 0, files = 0;
    if (ojh_path_size(path, NULL, &bytes, &files) == 0 && files == 1 && bytes > l->bytes) {
        l->bytes = bytes;
        snprintf(l->newest, sizeof l->newest, "%s", path);
    }
}

static int footprint_opendoctrines(const ojh_tpm_options *o, ojh_footprint *f, char *error, size_t error_len) {
    if (!o->od_server || !o->od_data) return fail(error, error_len, "needs --od-server and --od-data");
    add_install(f, o->od_server);
    add_install(f, o->od_data);
    char save[4400];
    if (o->od_save) snprintf(save, sizeof save, "%s", o->od_save);
    else join_path(save, sizeof save, o->od_data, "saves/Modern Day.odsv");
    uint64_t bytes = 0, files = 0;
    if (ojh_path_size(save, NULL, &bytes, &files) == 0 && files == 1) {
        f->has_save = 1;
        f->save_bytes = bytes;
        const char *argv[] = {o->od_server, "--load", save, "--check", "--data", o->od_data, NULL};
        double wall = 0;
        char why[1024] = "";
        if (o->work_dir) ojh_make_dir(o->work_dir);
        int code = run_and_read(argv, NULL, o->work_dir, o->timeout_seconds > 0 ? o->timeout_seconds : 600, f, &wall, why,
                                sizeof why);
        f->exit_code = code;
        if (code == 0) f->load_seconds = wall;
    }
    snprintf(f->how, sizeof f->how,
             "install is the server binary and the data folder; the save is %.400s; load time is OpenDoctrinesServer "
             "--load --check from launch to exit, process start included; Open Doctrines has no command to time a "
             "save, so save time is n/a", save);
    return f->has_install ? 0 : fail(error, error_len, "the Open Doctrines paths do not exist");
}

static int footprint_gd5(const ojh_tpm_options *o, ojh_footprint *f, char *error, size_t error_len) {
    if (!o->gd5_python || !o->gd5_dir || !o->drivers_dir) {
        return fail(error, error_len, "needs --gd5-python, --gd5-dir and --drivers");
    }
    add_install(f, o->gd5_dir);
    char driver[4400], turns[16];
    join_path(driver, sizeof driver, o->drivers_dir, "gd5_footprint.py");
    snprintf(turns, sizeof turns, "%d", o->turns);
    const char *argv[] = {o->gd5_python, driver, "--gd5", o->gd5_dir, "--turns", turns, NULL};
    f->exit_code = run_and_read(argv, NULL, NULL, o->timeout_seconds > 0 ? o->timeout_seconds : 1800, f, NULL, error,
                                error_len);
    snprintf(f->how, sizeof f->how,
             "install is the Greater Diplomacy 5 folder without .git and caches (Python and its packages not counted); "
             "OJH's GD5 driver plays %d turns, then times GD5's own save_map_data and loading that save into a new Map, "
             "and measures the save folder", o->turns);
    return f->exit_code == 0 ? 0 : -1;
}

static int footprint_freeciv(const ojh_tpm_options *o, ojh_footprint *f, char *error, size_t error_len) {
    if (!o->freeciv_server || !o->work_dir) return fail(error, error_len, "needs --freeciv-server and --work");
    char prefix[4400] = "";
    if (o->freeciv_prefix) {
        snprintf(prefix, sizeof prefix, "%s", o->freeciv_prefix);
    } else {
        if (ojh_resolve_program(o->freeciv_server, prefix, sizeof prefix) != 0) {
            snprintf(prefix, sizeof prefix, "%s", o->freeciv_server);
        }
        for (int up = 0; up < 2; up++) {
            char *slash = strrchr(prefix, '/');
            char *back = strrchr(prefix, '\\');
            if (back && (!slash || back > slash)) slash = back;
            if (!slash) {
                prefix[0] = '\0';
                break;
            }
            *slash = '\0';
        }
    }
    if (*prefix) add_install(f, prefix);

    char dir[4400], saves[4500], play[4500], quit[4500];
    join_path(dir, sizeof dir, o->work_dir, "freeciv-footprint");
    join_path(saves, sizeof saves, dir, "saves");
    join_path(play, sizeof play, dir, "play.serv");
    join_path(quit, sizeof quit, dir, "quit.serv");
    ojh_make_dir(o->work_dir);
    ojh_make_dir(dir);
    ojh_make_dir(saves);
    FILE *s = fopen(play, "wb");
    if (!s) return fail(error, error_len, "cannot write the Freeciv script");
    fprintf(s, "set gameseed %u\nset mapseed %u\nset timeout -1\nset minplayers 0\nset ec_turns 0\nset aifill %d\n"
               "set endturn %d\nset saveturns 1\nset autosaves \"TURN|GAMEOVER\"\nset savename \"ojh\"\nhard\n"
               "create Bench\nstart\n",
            o->seed, o->seed, o->players, o->turns);
    fclose(s);
    s = fopen(quit, "wb");
    if (!s) return fail(error, error_len, "cannot write the Freeciv script");
    fputs("quit\n", s);
    fclose(s);
    const char *env[] = {"LC_ALL=C", "LANG=C", NULL};
    const char *argv_play[] = {o->freeciv_server, "-e", "-p", "55620", "-s", saves, "-r", play, NULL};
    double timeout = o->timeout_seconds > 0 ? o->timeout_seconds : 1800;
    f->exit_code = run_and_read(argv_play, env, dir, timeout, f, NULL, error, error_len);
    largest_file biggest;
    memset(&biggest, 0, sizeof biggest);
    biggest.dir = saves;
    ojh_list_dir(saves, pick_largest, &biggest);
    if (biggest.bytes > 0) {
        f->has_save = 1;
        f->save_bytes = biggest.bytes;
        const char *argv_empty[] = {o->freeciv_server, "-p", "55621", "-r", quit, NULL};
        const char *argv_load[] = {o->freeciv_server, "-p", "55622", "-f", biggest.newest, "-r", quit, NULL};
        double empty = 0, loaded = 0;
        char why[1024] = "";
        int a = run_and_read(argv_empty, env, dir, 300, f, &empty, why, sizeof why);
        int b = run_and_read(argv_load, env, dir, 300, f, &loaded, why, sizeof why);
        if (a == 0 && b == 0 && loaded >= empty) f->load_seconds = loaded - empty;
    }
    snprintf(f->how, sizeof f->how,
             "install is the Freeciv folder the server runs from; the save is the largest of Freeciv's own per-turn "
             "saves over %d turns, in its default compression; load time is freeciv-server started on that save minus "
             "the same server started empty; Freeciv has no command to time a save, so save time is n/a", o->turns);
    return f->has_install || f->has_save ? 0 : fail(error, error_len, "neither the Freeciv install nor a save was found");
}

int ojh_unciv_prepare(const ojh_tpm_options *o, char *classpath, size_t classpath_len, char *assets, size_t assets_len,
                      char *error, size_t error_len) {
    if (!o->unciv_jar || !o->java || !o->javac || !o->jar_tool || !o->drivers_dir || !o->work_dir) {
        return fail(error, error_len, "needs --unciv-jar, --java, --javac, --jar, --drivers and --work");
    }
    ojh_footprint scratch;
    reset(&scratch, OJH_GAME_UNCIV);
    char classes[4400], source[4400];
    join_path(classes, sizeof classes, o->work_dir, "unciv-driver");
    join_path(source, sizeof source, o->drivers_dir, "unciv/UncivTpm.java");
    join_path(assets, assets_len, o->work_dir, "unciv-assets");
    ojh_make_dir(o->work_dir);
    ojh_make_dir(classes);
    ojh_make_dir(assets);
    const char *extract[] = {o->jar_tool, "xf", o->unciv_jar, "jsons", NULL};
    if (run_and_read(extract, NULL, assets, 600, &scratch, NULL, NULL, 0) != 0) {
        return fail(error, error_len, "could not extract Unciv's rulesets from the jar");
    }
    char fps_source[4400];
    join_path(fps_source, sizeof fps_source, o->drivers_dir, "unciv/UncivFps.java");
    uint64_t fps_bytes = 0, fps_files = 0;
    int has_fps = ojh_path_size(fps_source, NULL, &fps_bytes, &fps_files) == 0;
    const char *compile[] = {o->javac, "-nowarn", "-cp", o->unciv_jar, "-d", classes, source, has_fps ? fps_source : NULL, NULL};
    if (run_and_read(compile, NULL, NULL, 600, &scratch, NULL, NULL, 0) != 0) {
        return fail(error, error_len, "the Unciv driver did not compile");
    }
#ifdef _WIN32
    snprintf(classpath, classpath_len, "%s;%s", classes, o->unciv_jar);
#else
    snprintf(classpath, classpath_len, "%s:%s", classes, o->unciv_jar);
#endif
    return 0;
}

static int footprint_unciv(const ojh_tpm_options *o, ojh_footprint *f, char *error, size_t error_len) {
    char assets[4400], classpath[9000], turns[16], players[16];
    if (ojh_unciv_prepare(o, classpath, sizeof classpath, assets, sizeof assets, error, error_len) != 0) return -1;
    add_install(f, o->unciv_jar);
    snprintf(turns, sizeof turns, "%d", o->turns);
    snprintf(players, sizeof players, "%d", o->players);
    const char *argv[] = {o->java, "-Djava.awt.headless=true", "-cp", classpath, "UncivTpm", players, turns, "small",
                          "footprint", NULL};
    f->exit_code = run_and_read(argv, NULL, assets, o->timeout_seconds > 0 ? o->timeout_seconds : 1800, f, NULL, error,
                                error_len);
    snprintf(f->how, sizeof f->how,
             "install is Unciv.jar (the Java runtime not counted); OJH's Unciv driver plays %d turns, then times "
             "UncivFiles.gameInfoToString with compression, the form Unciv writes saves in, and gameInfoFromString on "
             "the result", o->turns);
    return f->exit_code == 0 ? 0 : -1;
}

int ojh_footprint_run(ojh_game game, const ojh_tpm_options *o, ojh_footprint *out, char *error, size_t error_len) {
    reset(out, game);
    switch (game) {
        case OJH_GAME_OPENDOCTRINES: return footprint_opendoctrines(o, out, error, error_len);
        case OJH_GAME_GD5: return footprint_gd5(o, out, error, error_len);
        case OJH_GAME_FREECIV: return footprint_freeciv(o, out, error, error_len);
        case OJH_GAME_UNCIV: return footprint_unciv(o, out, error, error_len);
        default: return fail(error, error_len, "unknown game");
    }
}

int ojh_footprint_run_spec(const ojh_gamespec *spec, const ojh_tpm_options *o, ojh_footprint *out, char *error,
                           size_t error_len) {
    reset(out, OJH_GAME_CUSTOM);
    snprintf(out->id, sizeof out->id, "%s", spec->id);
    snprintf(out->name, sizeof out->name, "%s", spec->name);
    snprintf(out->version, sizeof out->version, "%s", spec->version);
    for (int i = 0; i < spec->install_count; i++) {
        char *path = ojh_gamespec_path(spec, spec->install[i], 0);
        if (path) add_install(out, path);
        free(path);
    }
    if (spec->footprint_count > 0) {
        ojh_spec_values values = {o->turns, o->seed, o->players, o->work_dir, o->ojh_path};
        char **argv = calloc((size_t)spec->footprint_count + 1, sizeof *argv);
        if (!argv) return fail(error, error_len, "out of memory");
        for (int i = 0; i < spec->footprint_count; i++) {
            char *filled = ojh_gamespec_expand(spec->footprint_command[i], spec, &values);
            argv[i] = i == 0 && filled ? ojh_gamespec_path(spec, filled, 1) : filled;
            if (i == 0) free(filled);
        }
        char *cwd = ojh_gamespec_path(spec, ".", 0);
        if (o->work_dir) ojh_make_dir(o->work_dir);
        out->exit_code = run_and_read((const char *const *)argv, NULL, cwd,
                                      o->timeout_seconds > 0 ? o->timeout_seconds : spec->timeout_seconds, out, NULL,
                                      error, error_len);
        for (int i = 0; i < spec->footprint_count; i++) free(argv[i]);
        free(argv);
        free(cwd);
    }
    snprintf(out->how, sizeof out->how, "install is the paths the spec lists%s",
             spec->footprint_count > 0 ? "; save and load come from the spec's footprint command and its OJH save and "
                                         "OJH load lines" : "; the spec has no footprint command, so save and load are n/a");
    if (!out->has_install && !out->has_save) return fail(error, error_len, "the spec's install paths do not exist and it reported no save");
    return spec->footprint_count > 0 && out->exit_code != 0 ? -1 : 0;
}

void ojh_footprint_json(ojh_json *w, const ojh_footprint *f) {
    ojh_json_object(w);
    ojh_json_key(w, "game"); ojh_json_string(w, f->id);
    ojh_json_key(w, "name"); ojh_json_string(w, f->name);
    if (*f->version) {
        ojh_json_key(w, "version"); ojh_json_string(w, f->version);
    }
    ojh_json_key(w, "exit_code"); ojh_json_int(w, f->exit_code);
    ojh_json_key(w, "install_bytes");
    if (f->has_install) ojh_json_uint(w, f->install_bytes);
    else ojh_json_null(w);
    ojh_json_key(w, "install_files");
    if (f->has_install) ojh_json_uint(w, f->install_files);
    else ojh_json_null(w);
    ojh_json_key(w, "install_paths"); ojh_json_string(w, f->install_paths);
    ojh_json_key(w, "save_bytes");
    if (f->has_save) ojh_json_uint(w, f->save_bytes);
    else ojh_json_null(w);
    ojh_json_key(w, "save_seconds");
    if (f->save_seconds >= 0) ojh_json_double(w, f->save_seconds, 4);
    else ojh_json_null(w);
    ojh_json_key(w, "load_seconds");
    if (f->load_seconds >= 0) ojh_json_double(w, f->load_seconds, 4);
    else ojh_json_null(w);
    ojh_json_key(w, "how"); ojh_json_string(w, f->how);
    ojh_json_end_object(w);
}
