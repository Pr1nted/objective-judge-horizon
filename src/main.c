#ifndef _WIN32
#  define _DEFAULT_SOURCE
#  define _DARWIN_C_SOURCE
#endif
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gamespec.h"
#include "json.h"
#include "jsonread.h"
#include "machine.h"
#include "netmeter.h"
#include "procmeter.h"
#include "report.h"
#include "runner.h"
#include "score.h"
#include "sha256.h"
#include "tpm.h"

#define OJH_VERSION "0.1.0"

static int usage(void) {
    fputs("Objective Judge Horizon (OJH) " OJH_VERSION "\n"
          "turn-based strategy games measured the same way on the same machine\n"
          "\n"
          "  ojh machine [seconds]                  this machine's profile and CPU reference score\n"
          "  ojh relay <listen> <host> <port> <seconds> [clients]\n"
          "                                         count a netcode's traffic on loopback\n"
          "  ojh tpm <opendoctrines|gd5|freeciv|unciv|your-game.json> [--turns N] [--seed S] [--players P] [--timeout S]\n"
          "          [--od-server PATH --od-data DIR] [--gd5-python PATH --gd5-dir DIR]\n"
          "          [--freeciv-server PATH] [--unciv-jar PATH --java PATH --javac PATH --jar PATH]\n"
          "          [--drivers DIR] [--work DIR] [--out FILE]\n"
          "                                         turns per minute, every player AI\n"
          "  ojh report <folder>                    report.md and report.txt, and every game's scorecard, from the\n"
          "                                         result files in a folder\n"
          "  ojh score <result.json>... [--out DIR] each game's own OJH score and scorecard\n"
          "  ojh spec new <your-game.json>          a game spec to fill in, for putting your own game through OJH\n"
          "  ojh spec check <your-game.json>        what OJH will run for that spec\n"
          "  ojh selftest sha256|json|jsonread|relay|procmeter|runner|tpm|report|spec|score\n",
          stderr);
    return 2;
}

static int cmd_machine(int argc, char **argv) {
    double seconds = argc > 2 ? atof(argv[2]) : 3.0;
    if (seconds <= 0) seconds = 3.0;
    ojh_machine m;
    ojh_reference r;
    ojh_machine_read(&m);
    ojh_reference_measure(&r, seconds);
    ojh_json w;
    ojh_json_init(&w, stdout);
    ojh_machine_json(&w, &m, &r);
    return 0;
}

static int cmd_relay(int argc, char **argv) {
    if (argc < 6) return usage();
    uint16_t listen_port = (uint16_t)atoi(argv[2]);
    uint16_t target_port = (uint16_t)atoi(argv[4]);
    double seconds = atof(argv[5]);
    int clients = argc > 6 ? atoi(argv[6]) : 0;
    ojh_relay *r = ojh_relay_start(listen_port, argv[3], target_port);
    if (!r) {
        fprintf(stderr, "relay: cannot listen on 127.0.0.1:%u\n", (unsigned)listen_port);
        return 1;
    }
    fprintf(stderr, "relay: 127.0.0.1:%u -> %s:%u for %.0f s\n", (unsigned)ojh_relay_port(r), argv[3],
            (unsigned)target_port, seconds);
    ojh_sleep(seconds);
    ojh_relay_stop(r);
    ojh_json w;
    ojh_json_init(&w, stdout);
    ojh_relay_json(&w, r, clients);
    ojh_relay_free(r);
    return 0;
}

static int ends_with(const char *s, const char *suffix) {
    size_t n = strlen(s), k = strlen(suffix);
    return n >= k && strcmp(s + n - k, suffix) == 0;
}

/* The folder a file is in: "." when the path has none. */
static void folder_of(char *out, size_t n, const char *path) {
    snprintf(out, n, "%s", path);
    char *slash = strrchr(out, '/'), *backslash = strrchr(out, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    if (slash) *slash = '\0';
    else snprintf(out, n, ".");
    if (!*out) snprintf(out, n, "/");
}

static int cmd_tpm(int argc, char **argv) {
    if (argc < 3) return usage();
    ojh_game game = OJH_GAME_CUSTOM;
    ojh_gamespec spec;
    memset(&spec, 0, sizeof spec);
    int from_spec = ends_with(argv[2], ".json");
    if (from_spec) {
        char why[1024];
        if (ojh_gamespec_load(argv[2], &spec, why, sizeof why) != 0) {
            fprintf(stderr, "tpm: %s\n", why);
            return 2;
        }
    } else if (ojh_game_parse(argv[2], &game) != 0) {
        fprintf(stderr, "tpm: unknown game '%s' (give opendoctrines, gd5, freeciv or unciv, or your own game's spec file "
                        "ending in .json: see docs/adding-your-game.md)\n", argv[2]);
        return 2;
    }
    const char *name = from_spec ? spec.name : ojh_game_name(game);
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = getenv("TEMP");
    if (!tmp) tmp = ".";
    static char work[1024];
    snprintf(work, sizeof work, "%s/ojh-work", tmp);

    ojh_tpm_options o;
    memset(&o, 0, sizeof o);
    o.turns = from_spec ? spec.default_turns : game == OJH_GAME_OPENDOCTRINES ? 500 : 100;
    o.seed = 20260914u;
    o.players = 8;
    o.timeout_seconds = from_spec ? 0 : 3600; /* 0: the spec's own */
    o.freeciv_server = "freeciv-server";
    o.java = "java";
    o.javac = "javac";
    o.jar_tool = "jar";
    o.drivers_dir = "drivers";
    o.work_dir = work;
    static char self[1024];
    if (ojh_self_path(self, sizeof self) == 0) o.ojh_path = self;
    const char *out_path = NULL;
    for (int i = 3; i < argc; i += 2) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!v) {
            fprintf(stderr, "tpm: %s needs a value\n", a);
            return 2;
        }
        if (strcmp(a, "--turns") == 0) o.turns = atoi(v);
        else if (strcmp(a, "--seed") == 0) o.seed = (unsigned)strtoul(v, NULL, 10);
        else if (strcmp(a, "--players") == 0) o.players = atoi(v);
        else if (strcmp(a, "--timeout") == 0) o.timeout_seconds = atof(v);
        else if (strcmp(a, "--od-server") == 0) o.od_server = v;
        else if (strcmp(a, "--od-data") == 0) o.od_data = v;
        else if (strcmp(a, "--gd5-python") == 0) o.gd5_python = v;
        else if (strcmp(a, "--gd5-dir") == 0) o.gd5_dir = v;
        else if (strcmp(a, "--freeciv-server") == 0) o.freeciv_server = v;
        else if (strcmp(a, "--unciv-jar") == 0) o.unciv_jar = v;
        else if (strcmp(a, "--java") == 0) o.java = v;
        else if (strcmp(a, "--javac") == 0) o.javac = v;
        else if (strcmp(a, "--jar") == 0) o.jar_tool = v;
        else if (strcmp(a, "--drivers") == 0) o.drivers_dir = v;
        else if (strcmp(a, "--work") == 0) o.work_dir = v;
        else if (strcmp(a, "--out") == 0) out_path = v;
        else {
            fprintf(stderr, "tpm: unknown option %s\n", a);
            return 2;
        }
    }
    if (o.turns < 1) o.turns = 1;

    ojh_machine m;
    ojh_reference ref;
    ojh_machine_read(&m);
    fprintf(stderr, "tpm: measuring this machine's reference score\n");
    ojh_reference_measure(&ref, 3.0);
    fprintf(stderr, "tpm: %s for %d turns\n", name, o.turns);

    ojh_tpm result;
    char error[1024] = "";
    int status = from_spec ? ojh_tpm_run_spec(&spec, &o, &result, error, sizeof error)
                           : ojh_tpm_run(game, &o, &result, error, sizeof error);

    FILE *out = out_path ? fopen(out_path, "wb") : stdout;
    if (!out) {
        fprintf(stderr, "tpm: cannot write %s\n", out_path);
        ojh_tpm_free(&result);
        return 1;
    }
    ojh_json w;
    ojh_json_init(&w, out);
    ojh_json_object(&w);
    ojh_json_key(&w, "ojh_version"); ojh_json_string(&w, OJH_VERSION);
    ojh_json_key(&w, "metric"); ojh_json_string(&w, "tpm");
    ojh_json_key(&w, "machine"); ojh_machine_json(&w, &m, &ref);
    ojh_json_key(&w, "settings");
    ojh_json_object(&w);
    ojh_json_key(&w, "turns_requested"); ojh_json_int(&w, o.turns);
    ojh_json_key(&w, "seed"); ojh_json_uint(&w, o.seed);
    ojh_json_key(&w, "players_requested"); ojh_json_int(&w, o.players);
    /* OJH sets the player count for Freeciv (aifill) and Unciv (civilizations); GD5's scenario
       and Open Doctrines' generated world set their own. */
    ojh_json_key(&w, "players_chosen");
    ojh_json_bool(&w, from_spec ? spec.players_chosen : game == OJH_GAME_FREECIV || game == OJH_GAME_UNCIV);
    if (from_spec) {
        ojh_json_key(&w, "spec"); ojh_json_string(&w, argv[2]);
    }
    ojh_json_end_object(&w);
    ojh_json_key(&w, "result"); ojh_tpm_json(&w, &result);
    ojh_json_key(&w, "error");
    if (status == 0) ojh_json_null(&w);
    else ojh_json_string(&w, error);
    ojh_json_end_object(&w);
    if (out != stdout) fclose(out);
    if (status == 0) {
        fprintf(stderr, "tpm: %s played %d turns in %.2f s: %.1f turns per minute\n", name, result.turns,
                result.play_seconds, ojh_tpm_value(&result));
    } else {
        fprintf(stderr, "tpm: %s failed: %s\n", name, error);
    }
    if (out_path) {
        char why[256];
        ojh_jvalue *root = ojh_jparse_file(out_path, why, sizeof why);
        ojh_score s;
        if (root && ojh_score_result(root, &s) == 0) {
            fprintf(stderr, "tpm: OJH score %.0f (%s, %.0f%% coverage); ojh score %s writes its scorecard\n", s.total,
                    s.provisional ? "provisional" : "final", s.coverage * 100, out_path);
        }
        if (root) ojh_jfree(root);
    }
    ojh_tpm_free(&result);
    ojh_gamespec_free(&spec);
    return status == 0 ? 0 : 1;
}

static int cmd_report(int argc, char **argv) {
    if (argc < 3) return usage();
    char error[1024] = "";
    if (ojh_report_write(argv[2], error, sizeof error) != 0) {
        fprintf(stderr, "report: %s\n", error);
        return 1;
    }
    fprintf(stderr, "report: wrote %s/report.md and %s/report.txt\n", argv[2], argv[2]);
    return 0;
}

static int cmd_score(int argc, char **argv) {
    const char *out_dir = NULL;
    int files = 0, failures = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--out") == 0) {
            if (i + 1 >= argc) return usage();
            out_dir = argv[++i];
        } else {
            files++;
        }
    }
    if (files == 0) return usage();
    if (out_dir) ojh_make_dir(out_dir);
    ojh_json w;
    ojh_json_init(&w, stdout);
    ojh_json_array(&w);
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--out") == 0) {
            i++;
            continue;
        }
        char error[1024] = "", folder[4096], written[128];
        ojh_jvalue *root = ojh_jparse_file(argv[i], error, sizeof error);
        ojh_score s;
        if (!root || ojh_score_result(root, &s) != 0) {
            fprintf(stderr, "score: %s is not a result OJH can score%s%s\n", argv[i], *error ? ": " : "", error);
            if (root) ojh_jfree(root);
            failures++;
            continue;
        }
        ojh_jfree(root);
        ojh_score_json(&w, &s);
        folder_of(folder, sizeof folder, argv[i]);
        const char *dir = out_dir ? out_dir : folder;
        if (ojh_scorecard_write(argv[i], dir, written, sizeof written, error, sizeof error) != 0) {
            fprintf(stderr, "score: %s\n", error);
            failures++;
            continue;
        }
        fprintf(stderr, "score: %s %.0f (%s, %.0f%% coverage) -> %s/%s.md, .txt and .svg\n", s.name, s.total,
                s.provisional ? "provisional" : "final", s.coverage * 100, dir, written);
    }
    ojh_json_end_array(&w);
    return failures ? 1 : 0;
}

static void print_argument(const char *a) {
    if (strchr(a, ' ') || !*a) printf(" \"%s\"", a);
    else printf(" %s", a);
}

static int cmd_spec(int argc, char **argv) {
    if (argc < 4) return usage();
    char error[1024] = "";
    if (strcmp(argv[2], "new") == 0) {
        if (ojh_gamespec_write_template(argv[3], error, sizeof error) != 0) {
            fprintf(stderr, "spec: %s\n", error);
            return 1;
        }
        fprintf(stderr, "spec: wrote %s. Fill it in, then run: ojh spec check %s\n", argv[3], argv[3]);
        return 0;
    }
    if (strcmp(argv[2], "check") != 0) return usage();
    ojh_gamespec s;
    if (ojh_gamespec_load(argv[3], &s, error, sizeof error) != 0) {
        fprintf(stderr, "spec: %s\n", error);
        return 1;
    }
    char self[1024] = "ojh";
    ojh_self_path(self, sizeof self);
    ojh_spec_values v = {s.default_turns, 20260914u, 8, "<OJH's scratch folder>", self};
    printf("%s (id %s)%s%s\n", s.name, s.id, *s.version ? ", version " : "", s.version);
    if (s.turns_from == OJH_TURNS_PROTOCOL) {
        printf("  turns:      the game prints OJH ready, then OJH turn N as each turn ends\n");
    } else {
        printf("  turns:      the gaps between lines containing \"%s\" on %s\n", s.turn_ends,
               s.stream == OJH_STDOUT ? "stdout" : s.stream == OJH_STDERR ? "stderr" : "stdout or stderr");
        if (s.game_starts) printf("  start-up:   ends at the first line containing \"%s\"\n", s.game_starts);
        else printf("  start-up:   ends at the first turn line, which is not timed as a turn\n");
    }
    if (s.players_chosen) printf("  players:    chosen by OJH (--players, default 8)\n");
    else if (s.players) printf("  players:    %d, as the spec says\n", s.players);
    else printf("  players:    as the game prints them, or n/a\n");
    printf("  command:   ");
    int missing = 0;
    for (int i = 0; i < s.command_count; i++) {
        char *filled = ojh_gamespec_expand(s.command[i], &s, &v);
        char *shown = filled && i == 0 ? ojh_gamespec_path(&s, filled, 1) : NULL;
        const char *text = shown ? shown : filled ? filled : "?";
        if (i == 0 && (strchr(text, '/') || strchr(text, '\\'))) {
            FILE *f = fopen(text, "rb");
            if (f) fclose(f);
            else missing = 1;
        }
        print_argument(text);
        free(shown);
        free(filled);
    }
    char *cwd_template = s.working_directory ? ojh_gamespec_expand(s.working_directory, &s, &v) : NULL;
    char *cwd = ojh_gamespec_path(&s, cwd_template ? cwd_template : ".", 0);
    printf("\n  in folder:  %s\n", cwd ? cwd : "?");
    free(cwd_template);
    free(cwd);
    for (int i = 0; i < s.environment_count; i++) {
        char *filled = ojh_gamespec_expand(s.environment[i], &s, &v);
        printf("  environment: %s\n", filled ? filled : "?");
        free(filled);
    }
    printf("  default:    %d turns, stopped after %.0f s\n", s.default_turns, s.timeout_seconds);
    ojh_gamespec_free(&s);
    if (missing) {
        fprintf(stderr, "spec: the program does not exist at that path yet; the spec itself is valid\n");
        return 1;
    }
    fprintf(stderr, "spec: ok\n");
    return 0;
}

/* ---- self-tests: each proves one piece against a known answer */

static int close_to(double a, double b) { return a - b < 1e-6 && b - a < 1e-6; }

/* One result file the way `ojh tpm` writes it. */
static int write_tpm_result(const char *path, const ojh_machine *m, const ojh_reference *ref, const ojh_tpm *t,
                            int turns_requested) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    ojh_json w;
    ojh_json_init(&w, f);
    ojh_json_object(&w);
    ojh_json_key(&w, "ojh_version"); ojh_json_string(&w, OJH_VERSION);
    ojh_json_key(&w, "metric"); ojh_json_string(&w, "tpm");
    ojh_json_key(&w, "machine"); ojh_machine_json(&w, m, ref);
    ojh_json_key(&w, "settings");
    ojh_json_object(&w);
    ojh_json_key(&w, "turns_requested"); ojh_json_int(&w, turns_requested);
    ojh_json_key(&w, "seed"); ojh_json_uint(&w, 20260914u);
    ojh_json_key(&w, "players_requested"); ojh_json_int(&w, 8);
    ojh_json_key(&w, "players_chosen"); ojh_json_bool(&w, t->game == OJH_GAME_FREECIV || t->game == OJH_GAME_UNCIV);
    ojh_json_end_object(&w);
    ojh_json_key(&w, "result"); ojh_tpm_json(&w, t);
    ojh_json_key(&w, "error"); ojh_json_null(&w);
    ojh_json_end_object(&w);
    fclose(f);
    return 0;
}

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *buf = malloc(1 << 20);
    size_t len = buf ? fread(buf, 1, (1 << 20) - 1, f) : 0;
    fclose(f);
    if (buf) buf[len] = '\0';
    return buf;
}

/* "1,338" as the report writes a score. */
static void points_text(char *out, size_t n, double total) {
    long v = (long)(total + 0.5);
    if (v >= 1000) snprintf(out, n, "%ld,%03ld", v / 1000, v % 1000);
    else snprintf(out, n, "%ld", v);
}

/* Freeciv's scorecard shows the score of its own file, and stays byte-for-byte the same
   when Open Doctrines leaves the folder. */
static int check_scorecards(const char *dir, const char *freeciv_path, const char *od_path) {
    const char *suffixes[] = {"md", "txt", "svg"};
    char freeciv_card[3][1200], od_card[3][1200];
    for (int i = 0; i < 3; i++) {
        snprintf(freeciv_card[i], sizeof freeciv_card[i], "%s/score-freeciv.%s", dir, suffixes[i]);
        snprintf(od_card[i], sizeof od_card[i], "%s/score-opendoctrines.%s", dir, suffixes[i]);
    }
    int failures = 0;
    char error[256], expected[48] = "?", bold[64];
    char *card = read_all(freeciv_card[0]), *card_txt = read_all(freeciv_card[1]), *badge = read_all(freeciv_card[2]);
    char *od = read_all(od_card[0]);
    ojh_jvalue *alone = ojh_jparse_file(freeciv_path, error, sizeof error);
    ojh_score s;
    if (alone && ojh_score_result(alone, &s) == 0) points_text(expected, sizeof expected, s.total);
    if (alone) ojh_jfree(alone);
    snprintf(bold, sizeof bold, "**%s points**", expected);
    if (!card || !strstr(card, bold) || !strstr(card, "| Turn throughput | 40% |") || !strstr(card, "## Why it is provisional")) {
        fprintf(stderr, "report: score-freeciv.md is missing, or does not show %s from Freeciv's own file\n", bold);
        failures++;
    }
    if (!card_txt || strstr(card_txt, "**") || !strstr(card_txt, expected)) {
        fputs("report: score-freeciv.txt is missing, has Markdown, or lacks the score\n", stderr);
        failures++;
    }
    if (!badge || !strstr(badge, "<svg") || !strstr(badge, expected)) {
        fputs("report: score-freeciv.svg is missing or lacks the score\n", stderr);
        failures++;
    }
    if (!od || !strstr(od, "not reported") || !strstr(od, "50% of the weight")) {
        fputs("report: Open Doctrines' scorecard does not say which parts it lacks\n", stderr);
        failures++;
    }

    remove(od_path);
    for (int i = 0; i < 3; i++) {
        remove(od_card[i]);
        remove(freeciv_card[i]);
    }
    char again_error[1024] = "";
    ojh_report_write(dir, again_error, sizeof again_error);
    char *again = read_all(freeciv_card[0]);
    if (!card || !again || strcmp(card, again) != 0) {
        fputs("report: Freeciv's scorecard changed when Open Doctrines left the folder\n", stderr);
        failures++;
    }
    for (int i = 0; i < 3; i++) remove(freeciv_card[i]);
    free(card);
    free(card_txt);
    free(badge);
    free(od);
    free(again);
    return failures;
}

/* A complete result and one with gaps, through the report, in both formats. */
static int test_report(void) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = getenv("TEMP");
    if (!tmp) tmp = ".";
    char dir[1024], freeciv_path[1200], od_path[1200], md_path[1200], txt_path[1200];
    snprintf(dir, sizeof dir, "%s/ojh-report-selftest", tmp);
    ojh_make_dir(dir);
    snprintf(freeciv_path, sizeof freeciv_path, "%s/freeciv.json", dir);
    snprintf(od_path, sizeof od_path, "%s/opendoctrines.json", dir);
    snprintf(md_path, sizeof md_path, "%s/report.md", dir);
    snprintf(txt_path, sizeof txt_path, "%s/report.txt", dir);

    ojh_machine m;
    memset(&m, 0, sizeof m);
    snprintf(m.os, sizeof m.os, "macOS 26.3 (25D125)");
    snprintf(m.model, sizeof m.model, "MacBookPro18,1");
    snprintf(m.cpu, sizeof m.cpu, "Apple M1 Pro");
    snprintf(m.gpu, sizeof m.gpu, "Apple M1 Pro");
    snprintf(m.gpu_cores, sizeof m.gpu_cores, "16");
    snprintf(m.display, sizeof m.display, "3456 x 2234 Retina");
    m.logical_cpus = 10;
    m.performance_cpus = 8;
    m.efficiency_cpus = 2;
    m.memory_bytes = 17179869184ull;
    m.on_battery = 0;
    ojh_reference ref = {3.0, 10, 1252.0, 8399.0};

    static double freeciv_turns[] = {0.06, 0.06, 0.2, 0.2, 0.42, 0.42};
    ojh_tpm freeciv;
    memset(&freeciv, 0, sizeof freeciv);
    freeciv.game = OJH_GAME_FREECIV;
    freeciv.turns = 99;
    freeciv.timed_turns = 6;
    freeciv.turn_seconds = freeciv_turns;
    freeciv.play_seconds = 24.985;
    freeciv.boot_seconds = 0.485;
    freeciv.wall_seconds = 25.962;
    freeciv.players = 8;
    freeciv.regions = 2592;
    snprintf(freeciv.region_kind, sizeof freeciv.region_kind, "tiles");
    snprintf(freeciv.how, sizeof freeciv.how, "gaps between Freeciv's per-turn log lines");

    ojh_tpm od;
    memset(&od, 0, sizeof od);
    od.game = OJH_GAME_OPENDOCTRINES;
    od.turns = 500;
    od.play_seconds = 31.0;
    od.boot_seconds = 12.662;
    od.wall_seconds = 43.899;
    od.players = 37;
    snprintf(od.how, sizeof od.how, "Open Doctrines' own [EVAL] progress line");

    if (write_tpm_result(freeciv_path, &m, &ref, &freeciv, 100) != 0 || write_tpm_result(od_path, &m, &ref, &od, 500) != 0) {
        fputs("report: cannot write the result files\n", stderr);
        return 1;
    }
    char error[1024] = "";
    int status = ojh_report_write(dir, error, sizeof error);
    char *md = read_all(md_path), *txt = read_all(txt_path);
    int card_failures = check_scorecards(dir, freeciv_path, od_path);
    remove(freeciv_path);
    remove(od_path);
    remove(md_path);
    remove(txt_path);
    if (status != 0 || !md || !txt) {
        fprintf(stderr, "report: not written (%s)\n", error);
        free(md);
        free(txt);
        return 1;
    }
    const char *md_needs[] = {"# Objective Judge Horizon (OJH) report", "| Apple M1 Pro |", "| Freeciv | 99 | 237.7 |",
                              "| Open Doctrines | 500 | 967.7 |", "| 2,592 tiles |", "0.060 → 0.420",
                              "The runs timed different numbers of turns (99 to 500)", "Player counts differ (8 to 37)",
                              "n/a for TPM × regions", "**Freeciv**: gaps between", "8 players, chosen by OJH",
                              "players as the game's own scenario or world sets them",
                              "## OJH scores", "| score-freeciv.md |", "| provisional |"};
    const char *txt_needs[] = {"Objective Judge Horizon (OJH) report\n====", "Freeciv", "237.7", "967.7", "2,592 tiles",
                               "  - Freeciv: gaps between"};
    int failures = 0;
    for (size_t i = 0; i < sizeof md_needs / sizeof md_needs[0]; i++) {
        if (!strstr(md, md_needs[i])) {
            fprintf(stderr, "report: report.md is missing \"%s\"\n", md_needs[i]);
            failures++;
        }
    }
    for (size_t i = 0; i < sizeof txt_needs / sizeof txt_needs[0]; i++) {
        if (!strstr(txt, txt_needs[i])) {
            fprintf(stderr, "report: report.txt is missing \"%s\"\n", txt_needs[i]);
            failures++;
        }
    }
    const char *od_line = strstr(md, "**Open Doctrines**");
    const char *od_end = od_line ? strchr(od_line, '\n') : NULL;
    if (od_line && od_end && strstr(od_line, "chosen by OJH") && strstr(od_line, "chosen by OJH") < od_end) {
        fputs("report: Open Doctrines' line claims OJH chose its players\n", stderr);
        failures++;
    }
    if (strstr(txt, "| Freeciv |") || strstr(txt, "**")) {
        fputs("report: report.txt has Markdown in it\n", stderr);
        failures++;
    }
    free(md);
    free(txt);
    if (failures || card_failures) return 1;
    puts("report: ok (machine, scores, TPM table with n/a gaps, how each game was timed and the comparison warnings, in Markdown and text; scorecards match each game's own file and do not change when another game leaves)");
    return 0;
}

/* Reading JSON back: values, escapes and UTF-8, what must be refused, and OJH's own output. */
static int test_jsonread(void) {
    int failures = 0;
    char error[256];
    const char *doc =
        "\xEF\xBB\xBF{\"metric\": \"tpm\", \"result\": {\"tpm\": 237.74, \"players\": 8, \"regions\": null, "
        "\"per_turn\": {\"median_seconds\": 0.2073}, \"list\": [1, -2.5e1, true, false], "
        "\"name\": \"Gods \\u0026 Kings \\ud83c\\udfae \\\"quoted\\\"\"}}";
    ojh_jvalue *root = ojh_jparse(doc, strlen(doc), error, sizeof error);
    if (!root) {
        fprintf(stderr, "jsonread: a valid document was refused: %s\n", error);
        return 1;
    }
    const ojh_jvalue *list = ojh_jpath(root, "result.list");
    if (strcmp(ojh_jstring(ojh_jpath(root, "metric"), ""), "tpm") != 0) failures++;
    if (!close_to(ojh_jnumber(ojh_jpath(root, "result.tpm"), 0), 237.74)) failures++;
    if (!close_to(ojh_jnumber(ojh_jpath(root, "result.players"), 0), 8)) failures++;
    if (ojh_jpresent(ojh_jpath(root, "result.regions")) || ojh_jpresent(ojh_jpath(root, "result.missing"))) failures++;
    if (!close_to(ojh_jnumber(ojh_jpath(root, "result.per_turn.median_seconds"), 0), 0.2073)) failures++;
    if (!list || list->type != OJH_JARRAY || list->count != 4 || !close_to(list->items[1].number, -25.0) ||
        list->items[2].type != OJH_JBOOL || !close_to(list->items[2].number, 1)) {
        failures++;
    }
    if (strcmp(ojh_jstring(ojh_jpath(root, "result.name"), ""), "Gods & Kings \xF0\x9F\x8E\xAE \"quoted\"") != 0) {
        fprintf(stderr, "jsonread: escapes decoded as \"%s\"\n", ojh_jstring(ojh_jpath(root, "result.name"), ""));
        failures++;
    }
    ojh_jfree(root);

    const char *bad[] = {"{\"a\": [1, 2,}", "{} x", "{\"a\" 1}", "[\"\\ud800\"]", "01x", "\"unterminated"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ojh_jvalue *v = ojh_jparse(bad[i], strlen(bad[i]), error, sizeof error);
        if (v || !strstr(error, "at byte")) {
            fprintf(stderr, "jsonread: accepted or did not explain %s\n", bad[i]);
            ojh_jfree(v);
            failures++;
        }
    }
    char deep[201];
    memset(deep, '[', 200);
    deep[200] = '\0';
    ojh_jvalue *d = ojh_jparse(deep, 200, error, sizeof error);
    if (d || !strstr(error, "nested too deeply")) {
        ojh_jfree(d);
        failures++;
    }

    /* What ojh_json writes, ojh_jparse_file must read back. */
    char path[512];
    const char *dir = getenv("TMPDIR");
    if (!dir) dir = getenv("TEMP");
    if (!dir) dir = ".";
    snprintf(path, sizeof path, "%s/ojh-jsonread-selftest.json", dir);
    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    ojh_json w;
    ojh_json_init(&w, f);
    ojh_json_object(&w);
    ojh_json_key(&w, "machine"); ojh_json_object(&w); ojh_json_key(&w, "cpu"); ojh_json_string(&w, "Apple M1 Pro"); ojh_json_end_object(&w);
    ojh_json_key(&w, "tpm"); ojh_json_double(&w, 967.74, 2);
    ojh_json_end_object(&w);
    fclose(f);
    ojh_jvalue *back = ojh_jparse_file(path, error, sizeof error);
    remove(path);
    if (!back || strcmp(ojh_jstring(ojh_jpath(back, "machine.cpu"), ""), "Apple M1 Pro") != 0 ||
        !close_to(ojh_jnumber(ojh_jget(back, "tpm"), 0), 967.74)) {
        fprintf(stderr, "jsonread: OJH's own output did not read back (%s)\n", error);
        failures++;
    }
    ojh_jfree(back);

    if (failures) {
        fprintf(stderr, "jsonread: %d check(s) failed\n", failures);
        return 1;
    }
    puts("jsonread: ok (values, escapes and UTF-8 surrogate pairs; malformed, trailing and too-deep input refused; OJH's output reads back)");
    return 0;
}

/* Each game's output format, as its program prints it, through its parser. */
static int test_tpm(void) {
    int failures = 0;
    ojh_tpm t;

    ojh_line gd5[] = {
        {0.1, OJH_STDOUT, "pygame-ce 2.5.7"},
        {4.0, OJH_STDOUT, "{\"turn\": 1, \"logic_seconds\": 0.4, \"total_seconds\": 0.5, \"living_nations\": 35}"},
        {4.5, OJH_STDOUT, "{\"turn\": 2, \"logic_seconds\": 0.4, \"total_seconds\": 0.5, \"living_nations\": 35}"},
        {5.0, OJH_STDOUT, "{\"turn\": 3, \"logic_seconds\": 0.4, \"total_seconds\": 0.5, \"living_nations\": 35}"},
        {5.1, OJH_STDOUT, "{\"summary\": {\"game\": \"Greater Diplomacy 5\", \"turns\": 3, \"regions\": 906, "
                          "\"nations_at_start\": 35, \"boot_seconds\": 3.998}}"},
    };
    if (ojh_tpm_parse(OJH_GAME_GD5, gd5, 5, &t) != 0 || t.turns != 3 || !close_to(t.play_seconds, 1.5) ||
        !close_to(ojh_tpm_value(&t), 120.0) || t.regions != 906 || t.players != 35 || !close_to(t.boot_seconds, 3.998)) {
        fprintf(stderr, "tpm: GD5 parse gave turns %d play %.3f regions %ld players %d\n", t.turns, t.play_seconds,
                t.regions, t.players);
        failures++;
    }
    ojh_tpm_free(&t);

    ojh_line unciv[] = {
        {1.0, OJH_STDOUT, "{\"turn\": 1, \"seconds\": 0.250000, \"game_turn\": 1, \"major_civs_alive\": 8}"},
        {1.3, OJH_STDOUT, "{\"turn\": 2, \"seconds\": 0.250000, \"game_turn\": 2, \"major_civs_alive\": 8}"},
        {1.6, OJH_STDOUT, "{\"turn\": 3, \"seconds\": 0.250000, \"game_turn\": 3, \"major_civs_alive\": 8}"},
        {1.9, OJH_STDOUT, "{\"turn\": 4, \"seconds\": 0.250000, \"game_turn\": 4, \"major_civs_alive\": 8}"},
        {2.0, OJH_STDOUT, "{\"summary\": {\"game\": \"Unciv\", \"civs\": 8, \"map_size\": \"small\", \"tiles\": 1234, "
                          "\"turns\": 4, \"boot_seconds\": 0.800, \"turn_seconds\": 1.000, \"tpm\": 240.00}}"},
    };
    if (ojh_tpm_parse(OJH_GAME_UNCIV, unciv, 5, &t) != 0 || t.turns != 4 || !close_to(t.play_seconds, 1.0) ||
        !close_to(ojh_tpm_value(&t), 240.0) || t.regions != 1234 || t.players != 8) {
        fprintf(stderr, "tpm: Unciv parse gave turns %d play %.3f regions %ld players %d\n", t.turns, t.play_seconds,
                t.regions, t.players);
        failures++;
    }
    ojh_tpm_free(&t);

    ojh_line freeciv[] = {
        {0.5, OJH_STDOUT, "3: Konrad Adenauer rules the Germans."},
        {0.5, OJH_STDOUT, "3: Giuseppe Mazzini rules the Italians."},
        {0.5, OJH_STDOUT, "3: Bench rules the Indonesians."},
        {0.6, OJH_STDOUT, "3: Creating a map of size 36 x 72 = 2592 tiles (2666 requested)."},
        {1.9, OJH_STDERR, "4: in srv_running() [../server/srv_main.c::2861]: srv_running() mostly redundant send_server_settings()"},
        {2.0, OJH_STDERR, "4: in srv_running() [../server/srv_main.c::2925]: End/start-turn server/ai activities: 0.005 seconds"},
        {2.5, OJH_STDERR, "4: in srv_running() [../server/srv_main.c::2925]: End/start-turn server/ai activities: 0.039 seconds"},
        {3.0, OJH_STDERR, "4: in srv_running() [../server/srv_main.c::2925]: End/start-turn server/ai activities: 0.027 seconds"},
        {3.5, OJH_STDERR, "4: in srv_running() [../server/srv_main.c::2925]: End/start-turn server/ai activities: 0.044 seconds"},
    };
    if (ojh_tpm_parse(OJH_GAME_FREECIV, freeciv, 9, &t) != 0 || t.turns != 3 || !close_to(t.play_seconds, 1.5) ||
        !close_to(ojh_tpm_value(&t), 120.0) || t.regions != 2592 || t.players != 3 || !close_to(t.boot_seconds, 2.0)) {
        fprintf(stderr, "tpm: Freeciv parse gave turns %d play %.3f regions %ld players %d boot %.3f\n", t.turns,
                t.play_seconds, t.regions, t.players, t.boot_seconds);
        failures++;
    }
    ojh_tpm_free(&t);

    ojh_line od[] = {
        {0.2, OJH_STDOUT, "[EVAL] 1 map(s) x 500 turn(s), seed 20260914, difficulty hard, worlds generated"},
        {1.0, OJH_STDOUT, "[EVAL] map 1/1 [pangaea] seed=1395647406 countries=22"},
        {31.0, OJH_STDOUT, "[EVAL]   turn 250/500  22 alive  largest 12.6%  (0.120 s/turn)"},
        {51.0, OJH_STDOUT, "[EVAL]   turn 500/500  21 alive  largest 13.0%  (0.100 s/turn)"},
        {51.1, OJH_STDOUT, "[EVAL]   cap after 500 turns | alive 21/22 | largest 13.0% | concentration 0.070"},
    };
    if (ojh_tpm_parse(OJH_GAME_OPENDOCTRINES, od, 5, &t) != 0 || t.turns != 500 || !close_to(t.play_seconds, 50.0) ||
        !close_to(ojh_tpm_value(&t), 600.0) || t.players != 22 || !close_to(t.boot_seconds, 1.0)) {
        fprintf(stderr, "tpm: Open Doctrines parse gave turns %d play %.3f players %d boot %.3f\n", t.turns,
                t.play_seconds, t.players, t.boot_seconds);
        failures++;
    }
    ojh_tpm_free(&t);

    ojh_line nothing[] = {{0.1, OJH_STDOUT, "Traceback (most recent call last):"}};
    if (ojh_tpm_parse(OJH_GAME_GD5, nothing, 1, &t) == 0) {
        fputs("tpm: a run with no turns was reported as a success\n", stderr);
        failures++;
    }
    ojh_tpm_free(&t);

    if (failures) return 1;
    puts("tpm: ok (GD5, Unciv, Freeciv and Open Doctrines output formats parse to the expected turns, times, players and regions)");
    return 0;
}

/* A stand-in game for the spec tests and examples/games: it loads, then plays turns that
   take a little longer with more players, printing OJH's protocol lines or, with "plain",
   ordinary log lines. */
static int sample_game(int argc, char **argv) {
    int turns = argc > 3 ? atoi(argv[3]) : 10;
    int players = argc > 4 ? atoi(argv[4]) : 4;
    int plain = argc > 5 && strcmp(argv[5], "plain") == 0;
    if (turns < 1) turns = 1;
    if (players < 1) players = 1;
    ojh_sleep(0.05);
    if (plain) printf("Loading world\nWorld ready with %d nations and 480 provinces\n", players);
    else printf("OJH players %d\nOJH regions 480 provinces\nOJH ready\n", players);
    fflush(stdout);
    for (int t = 1; t <= turns; t++) {
        ojh_sleep(0.004 + 0.001 * players);
        if (plain) printf("Turn %d finished\n", t);
        else printf("OJH turn %d\n", t);
        fflush(stdout);
    }
    return 0;
}

static const char SAMPLE_PROTOCOL_SPEC[] =
    "{\"ojh_game_spec\": 1, \"id\": \"sample-protocol\", \"name\": \"Sample\", \"version\": \"1\", "
    "\"command\": [\"{ojh}\", \"selftest\", \"sample-game\", \"{turns}\", \"{players}\"], "
    "\"turns\": {\"from\": \"protocol\"}, \"region_kind\": \"provinces\"}";
static const char SAMPLE_MARKER_SPEC[] =
    "{\"ojh_game_spec\": 1, \"id\": \"sample-marker\", \"name\": \"Sample (marker)\", "
    "\"command\": [\"{ojh}\", \"selftest\", \"sample-game\", \"{turns}\", \"{players}\", \"plain\"], "
    "\"environment\": {\"LC_ALL\": \"C\"}, "
    "\"turns\": {\"from\": \"marker\", \"game_starts\": \"World ready\", \"turn_ends\": \"finished\", \"stream\": \"stdout\", "
    "\"players_after\": \"ready with \", \"regions_after\": \"nations and \"}, \"region_kind\": \"provinces\"}";

/* Specs: what must be refused and why, the protocol and marker readings, placeholders,
   and whole runs of the sample game through both kinds of spec. */
static int test_spec(void) {
    int failures = 0;
    char error[1024];
    ojh_gamespec s;
    static const struct {
        const char *text;
        const char *says;
    } bad[] = {
        {"{\"ojh_game_spec\": 1, \"id\": \"g\", \"name\": \"G\", \"turns\": {\"from\": \"protocol\"}}", "\"command\" must be a list"},
        {"{\"ojh_game_spec\": 1, \"id\": \"My Game\", \"name\": \"G\", \"command\": [\"g\"], \"turns\": {\"from\": \"protocol\"}}", "lower-case"},
        {"{\"ojh_game_spec\": 1, \"id\": \"freeciv\", \"name\": \"G\", \"command\": [\"g\"], \"turns\": {\"from\": \"protocol\"}}", "belongs to a game OJH measures"},
        {"{\"ojh_game_spec\": 1, \"id\": \"g\", \"name\": \"G\", \"comand\": [\"g\"], \"turns\": {\"from\": \"protocol\"}}", "unknown key \"comand\""},
        {"{\"ojh_game_spec\": 1, \"id\": \"g\", \"name\": \"G\", \"command\": [\"g\", \"{turn}\"], \"turns\": {\"from\": \"protocol\"}}", "{turn}"},
        {"{\"ojh_game_spec\": 1, \"id\": \"g\", \"name\": \"G\", \"command\": [\"g\"], \"turns\": {\"from\": \"marker\"}}", "no \"turn_ends\""},
        {"{\"ojh_game_spec\": 2, \"id\": \"g\", \"name\": \"G\", \"command\": [\"g\"], \"turns\": {\"from\": \"protocol\"}}", "must be 1"},
        {"{\"ojh_game_spec\": 1, \"id\": \"g\", \"name\": \"G\", \"command\": [\"g\"]}", "\"turns\" is missing"},
        {"{\"ojh_game_spec\": 1, \"id\": \"g\", \"name\": \"G\", \"command\": [\"g\"], \"turns\": {\"from\": \"protocol\"}, \"players\": 0}", "\"players\""},
        {"{\"ojh_game_spec\": 1, \"id\": \"g\", \"name\": \"G\", \"command\": [\"g\"], \"turns\": {\"from\": \"marker\", \"turn_ends\": \"x\", \"stream\": \"out\"}}", "\"stream\""},
        {"[1, 2]", "JSON object"},
        {"{\"id\": ", "not valid JSON"},
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        *error = '\0';
        if (ojh_gamespec_parse(bad[i].text, strlen(bad[i].text), ".", &s, error, sizeof error) == 0) {
            fprintf(stderr, "spec: accepted %s\n", bad[i].text);
            ojh_gamespec_free(&s);
            failures++;
        } else if (!strstr(error, bad[i].says)) {
            fprintf(stderr, "spec: refused %s but said \"%s\" (expected it to mention %s)\n", bad[i].text, error, bad[i].says);
            failures++;
        }
    }

    if (ojh_gamespec_parse(SAMPLE_PROTOCOL_SPEC, strlen(SAMPLE_PROTOCOL_SPEC), "/games/sample", &s, error, sizeof error) != 0) {
        fprintf(stderr, "spec: the protocol sample was refused: %s\n", error);
        return 1;
    }
    if (!s.players_chosen || s.default_turns != 100 || s.turns_from != OJH_TURNS_PROTOCOL) {
        fputs("spec: the protocol sample read back wrong\n", stderr);
        failures++;
    }
    ojh_spec_values values = {7, 42, 3, "/w", "/bin/ojh"};
    char *filled = ojh_gamespec_expand("{ojh} --turns={turns} {seed}/{players} {work} {spec_dir} {brace", &s, &values);
    if (!filled || strcmp(filled, "/bin/ojh --turns=7 42/3 /w /games/sample {brace") != 0) {
        fprintf(stderr, "spec: placeholders filled in as \"%s\"\n", filled ? filled : "(null)");
        failures++;
    }
    free(filled);
    char *near_spec = ojh_gamespec_path(&s, "./MyGame", 1), *on_path = ojh_gamespec_path(&s, "python3", 1);
    if (!near_spec || !on_path || strcmp(near_spec, "/games/sample/MyGame") != 0 || strcmp(on_path, "python3") != 0) {
        fprintf(stderr, "spec: paths resolved to %s and %s\n", near_spec ? near_spec : "?", on_path ? on_path : "?");
        failures++;
    }
    free(near_spec);
    free(on_path);

    ojh_line lines[] = {
        {0.1, OJH_STDOUT, "loading"},
        {0.9, OJH_STDERR, "[main] OJH players 6"},
        {1.0, OJH_STDOUT, "OJH ready"},
        {1.5, OJH_STDOUT, "OJH turn 1"},
        {2.5, OJH_STDOUT, "12:00:02 INFO: OJH turn 2"},
        {2.6, OJH_STDOUT, "OJH turn 3 0.25"},
        {2.7, OJH_STDOUT, "NOJH turn 4"},
        {2.8, OJH_STDOUT, "OJH regions 480 provinces"},
    };
    ojh_tpm t;
    if (ojh_tpm_parse_spec(&s, lines, sizeof lines / sizeof lines[0], &t) != 0 || t.turns != 3 ||
        !close_to(t.play_seconds, 1.75) || !close_to(t.boot_seconds, 1.0) || t.players != 6 || t.regions != 480 ||
        strcmp(t.region_kind, "provinces") != 0 || strcmp(t.id, "sample-protocol") != 0 || t.game != OJH_GAME_CUSTOM) {
        fprintf(stderr, "spec: protocol lines gave turns %d play %.3f boot %.3f players %d regions %ld %s id %s\n",
                t.turns, t.play_seconds, t.boot_seconds, t.players, t.regions, t.region_kind, t.id);
        failures++;
    }
    ojh_tpm_free(&t);

    /* Whole runs. */
    char self[1024], work[1200];
    if (ojh_self_path(self, sizeof self) != 0) return 1;
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = getenv("TEMP");
    if (!tmp) tmp = ".";
    snprintf(work, sizeof work, "%s/ojh-spec-selftest", tmp);
    ojh_tpm_options o;
    memset(&o, 0, sizeof o);
    o.turns = 12;
    o.players = 5;
    o.seed = 1;
    o.timeout_seconds = 60;
    o.work_dir = work;
    o.ojh_path = self;
    snprintf(s.spec_dir, sizeof s.spec_dir, "%s", tmp); /* the path checks above used a folder that does not exist */
    if (ojh_tpm_run_spec(&s, &o, &t, error, sizeof error) != 0 || t.turns != 12 || t.players != 5 || t.regions != 480 ||
        t.boot_seconds < 0 || t.exit_code != 0) {
        fprintf(stderr, "spec: the protocol sample ran to turns %d players %d regions %ld exit %d (%s)\n", t.turns,
                t.players, t.regions, t.exit_code, error);
        failures++;
    }
    ojh_tpm_free(&t);
    ojh_gamespec_free(&s);

    if (ojh_gamespec_parse(SAMPLE_MARKER_SPEC, strlen(SAMPLE_MARKER_SPEC), tmp, &s, error, sizeof error) != 0) {
        fprintf(stderr, "spec: the marker sample was refused: %s\n", error);
        return 1;
    }
    if (ojh_tpm_run_spec(&s, &o, &t, error, sizeof error) != 0 || t.turns != 12 || t.players != 5 || t.regions != 480 ||
        t.boot_seconds < 0 || !strstr(t.how, "\"finished\"")) {
        fprintf(stderr, "spec: the marker sample ran to turns %d players %d regions %ld boot %.3f (%s)\n", t.turns,
                t.players, t.regions, t.boot_seconds, error);
        failures++;
    }
    ojh_tpm_free(&t);
    ojh_gamespec_free(&s);

    if (failures) return 1;
    puts("spec: ok (bad specs refused with the reason; protocol lines, markers, placeholders and paths read right; "
         "the sample game timed through both kinds of spec)");
    return 0;
}

static void score_doc(char *out, size_t n, double tpm, const char *regions, int turns, const char *reference,
                      const char *error) {
    snprintf(out, n,
             "{\"metric\": \"tpm\", \"machine\": {\"reference\": %s}, \"result\": {\"game\": \"g\", \"name\": \"G\", "
             "\"turns\": %d, \"tpm\": %.2f, \"players\": 40, \"regions\": %s, \"boot_seconds\": 5, \"per_turn\": "
             "{\"median_seconds\": 0.2, \"p95_seconds\": 0.4, \"early_median_seconds\": 0.1, \"late_median_seconds\": 0.2}}, "
             "\"error\": %s}", reference, turns, tpm, regions, error);
}

static int score_of(const char *doc, ojh_score *s) {
    char error[256];
    ojh_jvalue *root = ojh_jparse(doc, strlen(doc), error, sizeof error);
    int status = root ? ojh_score_result(root, s) : -1;
    if (root) ojh_jfree(root);
    return status;
}

/* The score against hand-worked answers. */
static int test_score(void) {
    int failures = 0;
    char doc[1024];
    ojh_score s;
    if (!close_to(ojh_score_points(1, 1), 1000) || !close_to(ojh_score_points(7, 1), 3000) || ojh_score_points(0, 1) != 0) {
        fputs("score: points are not 1000 x log2(1 + value / reference)\n", stderr);
        failures++;
    }

    /* Every part exactly at its reference level, measured on a CPU twice the reference's speed. */
    score_doc(doc, sizeof doc, 100, "4000", 100, "{\"single_core_rounds_per_second\": 2000}", "null");
    if (score_of(doc, &s) != 0 || !close_to(s.total, 1000) || !close_to(s.coverage, 1) || s.provisional || s.reason_count) {
        fprintf(stderr, "score: parts at their reference levels gave %.3f, coverage %.2f, provisional %d:", s.total,
                s.coverage, s.provisional);
        for (int i = 0; i < s.part_count; i++) fprintf(stderr, " %s %.3f", s.parts[i].key, s.parts[i].points);
        fputc('\n', stderr);
        failures++;
    }

    /* Three times the turn throughput and no map size: 2000 points for throughput, world
       throughput left out rather than counted as zero. */
    score_doc(doc, sizeof doc, 300, "null", 100, "{\"single_core_rounds_per_second\": 2000}", "null");
    if (score_of(doc, &s) != 0 || !close_to(s.total, 1150.0 / 0.75) || !close_to(s.coverage, 0.75) || s.provisional ||
        s.reason_count != 1 || !strstr(s.reasons[0], "World throughput")) {
        fprintf(stderr, "score: a partial result gave %.3f, coverage %.2f, provisional %d, notes %d\n", s.total,
                s.coverage, s.provisional, s.reason_count);
        failures++;
    }

    /* Short, crashed and with no CPU reference: provisional, with all three reasons. */
    score_doc(doc, sizeof doc, 100, "4000", 20, "null", "\"crashed\"");
    if (score_of(doc, &s) != 0 || !s.provisional || s.reason_count != 3 || s.hardware_known) {
        fprintf(stderr, "score: a short failed run gave provisional %d with %d reasons\n", s.provisional, s.reason_count);
        failures++;
    }

    if (score_of("{\"metric\": \"fps\"}", &s) == 0) {
        fputs("score: scored something that is not a TPM result\n", stderr);
        failures++;
    }
    if (failures) return 1;
    puts("score: ok (points, hardware adjustment, weights, partial coverage and provisional runs match hand-worked answers)");
    return 0;
}

static int test_sha256(void) {
    static const struct {
        const char *input;
        const char *hex;
    } cases[] = {
        {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
    };
    uint8_t digest[32];
    char hex[65];
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        ojh_sha256((const uint8_t *)cases[i].input, strlen(cases[i].input), digest);
        for (int k = 0; k < 32; k++) snprintf(hex + k * 2, 3, "%02x", digest[k]);
        if (strcmp(hex, cases[i].hex) != 0) {
            fprintf(stderr, "sha256(\"%s\") = %s, expected %s\n", cases[i].input, hex, cases[i].hex);
            return 1;
        }
    }
    /* 1,000,000 x 'a' crosses the multi-block and length-padding paths */
    size_t n = 1000000;
    uint8_t *a = malloc(n);
    if (!a) return 1;
    memset(a, 'a', n);
    ojh_sha256(a, n, digest);
    free(a);
    for (int k = 0; k < 32; k++) snprintf(hex + k * 2, 3, "%02x", digest[k]);
    if (strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") != 0) {
        fprintf(stderr, "sha256(1e6 x 'a') = %s\n", hex);
        return 1;
    }
    puts("sha256: ok (FIPS 180-2 vectors)");
    return 0;
}

static int test_json(void) {
    char path[512];
    const char *dir = getenv("TMPDIR");
    if (!dir) dir = getenv("TEMP");
    if (!dir) dir = ".";
    snprintf(path, sizeof path, "%s/ojh-json-selftest-%d.json", dir, (int)(ojh_now() * 1000) % 100000);
    FILE *f = fopen(path, "w+b");
    if (!f) {
        fprintf(stderr, "json: cannot write %s\n", path);
        return 1;
    }
    ojh_json w;
    ojh_json_init(&w, f);
    ojh_json_object(&w);
    ojh_json_key(&w, "name"); ojh_json_string(&w, "quote \" backslash \\ newline \n tab \t");
    ojh_json_key(&w, "list"); ojh_json_array(&w);
    double zero = 0.0;
    ojh_json_int(&w, -3); ojh_json_double(&w, 1.5, 2); ojh_json_double(&w, zero / zero, 1); ojh_json_bool(&w, 1);
    ojh_json_end_array(&w);
    ojh_json_key(&w, "empty"); ojh_json_object(&w); ojh_json_end_object(&w);
    ojh_json_end_object(&w);
    fflush(f);
    rewind(f);
    char got[512];
    size_t len = fread(got, 1, sizeof got - 1, f);
    got[len] = '\0';
    fclose(f);
    remove(path);
    const char *expected =
        "{\n"
        "  \"name\": \"quote \\\" backslash \\\\ newline \\n tab \\t\",\n"
        "  \"list\": [\n"
        "    -3,\n"
        "    1.50,\n"
        "    null,\n"
        "    true\n"
        "  ],\n"
        "  \"empty\": {}\n"
        "}\n";
    if (strcmp(got, expected) != 0) {
        fprintf(stderr, "json: got\n%s\nexpected\n%s", got, expected);
        return 1;
    }
    puts("json: ok");
    return 0;
}

typedef struct {
    ojh_socket listener;
} echo_arg;

static void *echo_server(void *p) {
    echo_arg *a = p;
    int readable;
    if (ojh_wait_readable(&a->listener, 1, 10000, &readable) <= 0) return NULL;
    ojh_socket conn = ojh_accept(a->listener);
    if (conn == OJH_INVALID_SOCKET) return NULL;
    uint8_t buf[65536];
    long n;
    while ((n = ojh_recv(conn, buf, sizeof buf)) > 0) {
        if (ojh_send_all(conn, buf, (size_t)n) != 0) break;
    }
    ojh_sock_close(conn);
    return NULL;
}

/* Ten turns of growing size through the relay: turn k sends k*100 bytes and gets them
   echoed back, so both ways it takes k*200. The relay must count every byte, and DPT
   must find turn 1 lowest (200) and turn 10 highest (2000). */
static int test_relay(void) {
    if (ojh_net_init() != 0) return 1;
    uint16_t echo_port = 0;
    echo_arg ea;
    ea.listener = ojh_listen_loopback(0, &echo_port);
    if (ea.listener == OJH_INVALID_SOCKET) return 1;
    ojh_thread echo;
    if (ojh_thread_start(&echo, echo_server, &ea) != 0) return 1;

    ojh_relay *r = ojh_relay_start(0, "127.0.0.1", echo_port);
    if (!r) return 1;
    ojh_socket c = ojh_connect_tcp("127.0.0.1", ojh_relay_port(r));
    if (c == OJH_INVALID_SOCKET) return 1;
    uint8_t block[1000], in[4096];
    memset(block, 'x', sizeof block);
    uint64_t sent_total = 0;
    for (int turn = 1; turn <= 10; turn++) {
        size_t size = (size_t)turn * 100;
        if (ojh_send_all(c, block, size) != 0) return 1;
        sent_total += size;
        size_t got = 0;
        while (got < size) {
            long n = ojh_recv(c, in, sizeof in);
            if (n <= 0) return 1;
            got += (size_t)n;
        }
        char label[16];
        snprintf(label, sizeof label, "turn %d", turn);
        ojh_relay_mark(r, label);
    }
    ojh_sock_close(c);
    ojh_sleep(0.4);
    ojh_relay_stop(r);
    ojh_thread_join(&echo);
    ojh_sock_close(ea.listener);

    uint64_t up = ojh_relay_bytes(r, OJH_UP), down = ojh_relay_bytes(r, OJH_DOWN);
    ojh_dpt dpt;
    int turns = ojh_relay_dpt(r, &dpt);
    ojh_json w;
    ojh_json_init(&w, stdout);
    ojh_relay_json(&w, r, 1);
    ojh_relay_free(r);
    int ok = up == sent_total && down == sent_total && turns == 10 && dpt.lowest[2] == 200 &&
             dpt.highest[2] == 2000 && dpt.lowest_turn == 0 && dpt.highest_turn == 9 && dpt.unmarked_tail == 0;
    if (!ok) {
        fprintf(stderr,
                "relay: up=%llu down=%llu (expected %llu each), turns=%d, dpt lowest=%llu (turn %d) highest=%llu (turn %d) tail=%llu\n",
                (unsigned long long)up, (unsigned long long)down, (unsigned long long)sent_total, turns,
                (unsigned long long)dpt.lowest[2], dpt.lowest_turn + 1, (unsigned long long)dpt.highest[2],
                dpt.highest_turn + 1, (unsigned long long)dpt.unmarked_tail);
        return 1;
    }
    puts("relay: ok (every byte counted; DPT lowest 200 bytes on turn 1, highest 2000 on turn 10)");
    return 0;
}

/* Run as a child by the procmeter test: touches 64 MiB and burns CPU for 1.5 s. */
static int busy_child(void) {
    size_t size = (size_t)64 << 20;
    unsigned char *mem = malloc(size);
    if (!mem) return 1;
    memset(mem, 1, size);
    double end = ojh_now() + 1.5;
    uint64_t x = 0;
    while (ojh_now() < end) {
        for (int i = 0; i < 100000; i++) x += mem[(x * 2654435761u + (uint64_t)i) % size];
    }
    free(mem);
    return x > 0 ? 0 : 1;
}

static int test_procmeter(void) {
    char self[1024];
    if (ojh_self_path(self, sizeof self) != 0) {
        fputs("procmeter: cannot find this executable\n", stderr);
        return 1;
    }
    const char *argv[] = {self, "selftest", "busy-child", NULL};
    ojh_process child;
    if (ojh_spawn(argv, &child) != 0) {
        fputs("procmeter: cannot start the busy child\n", stderr);
        return 1;
    }
    ojh_procmeter *m = ojh_procmeter_start(child.pid, 0.1);
    int code = ojh_wait(&child);
    ojh_procmeter_stop(m);
    ojh_json w;
    ojh_json_init(&w, stdout);
    ojh_procmeter_json(&w, m);
    int samples = ojh_procmeter_samples(m);
    ojh_procmeter_free(m);
    if (code != 0 || samples < 5) {
        fprintf(stderr, "procmeter: child exited %d, %d samples of a 1.5 s process\n", code, samples);
        return 1;
    }
    puts("procmeter: ok (sampled a 64 MiB, CPU-busy child)");
    return 0;
}

/* Run as a child by the runner test: five paced lines, an environment value and a file
   found relative to the working directory on stdout, one line on stderr, exit code 7. */
static int print_lines(void) {
    for (int i = 1; i <= 5; i++) {
        printf("line %d\n", i);
        fflush(stdout);
        ojh_sleep(0.06);
    }
    const char *value = getenv("OJH_RUNNER_TEST");
    printf("env=%s\n", value ? value : "(unset)");
    FILE *marker = fopen("ojh-runner-cwd-marker.txt", "rb");
    printf("cwd=%s\n", marker ? "found" : "missing");
    if (marker) fclose(marker);
    fflush(stdout);
    fputs("to stderr\n", stderr);
    fflush(stderr);
    return 7;
}

static int test_runner(void) {
    char self[1024];
    if (ojh_self_path(self, sizeof self) != 0) return 1;

    const char *dir = getenv("TMPDIR");
    if (!dir) dir = getenv("TEMP");
    if (!dir) dir = ".";
    char marker[1024];
    snprintf(marker, sizeof marker, "%s/ojh-runner-cwd-marker.txt", dir);
    FILE *f = fopen(marker, "wb");
    if (!f) return 1;
    fputs("x", f);
    fclose(f);

    const char *argv[] = {self, "selftest", "print-lines", NULL};
    const char *env[] = {"OJH_RUNNER_TEST=hello from the runner", NULL};
    ojh_run *r = ojh_run_start(argv, env, dir);
    if (!r) {
        fputs("runner: the child did not start\n", stderr);
        remove(marker);
        return 1;
    }
    int code = ojh_run_wait(r, 30);
    remove(marker);
    int out = 0, err = 0, env_ok = 0, cwd_ok = 0, ordered = 1;
    double first = -1, last = -1, previous = -1;
    for (size_t i = 0; i < ojh_run_line_count(r); i++) {
        const ojh_line *l = ojh_run_line(r, i);
        if (l->stream == OJH_STDERR) {
            err += strcmp(l->text, "to stderr") == 0;
            continue;
        }
        out++;
        if (strncmp(l->text, "line ", 5) == 0) {
            if (first < 0) first = l->t;
            if (l->t < previous) ordered = 0;
            previous = last = l->t;
        }
        env_ok |= strcmp(l->text, "env=hello from the runner") == 0;
        cwd_ok |= strcmp(l->text, "cwd=found") == 0;
    }
    ojh_run_free(r);
    int ok = code == 7 && out == 7 && err == 1 && env_ok && cwd_ok && ordered && last - first >= 0.15;
    if (!ok) {
        fprintf(stderr, "runner: exit %d, %d stdout and %d stderr lines, env %d, cwd %d, ordered %d, span %.3f s\n",
                code, out, err, env_ok, cwd_ok, ordered, last - first);
        return 1;
    }

    /* A program that would run for a minute must end at the timeout, quickly. */
    const char *sleeper[] = {self, "selftest", "sleep-long", NULL};
    double t0 = ojh_now();
    ojh_run *s = ojh_run_start(sleeper, NULL, NULL);
    if (!s) return 1;
    int timed = ojh_run_wait(s, 0.5);
    double took = ojh_now() - t0;
    ojh_run_free(s);
    if (timed != -2 || took > 8.0) {
        fprintf(stderr, "runner: timeout returned %d after %.2f s\n", timed, took);
        return 1;
    }
    printf("runner: ok (stdout and stderr lines apart and in order, environment and working directory set, "
           "timeout ended the child in %.2f s)\n", took);
    return 0;
}

int main(int argc, char **argv) {
    ojh_ignore_sigpipe();
    if (ojh_net_init() != 0) {
        fputs("cannot start networking\n", stderr);
        return 1;
    }
    if (argc < 2) return usage();
    if (strcmp(argv[1], "machine") == 0) return cmd_machine(argc, argv);
    if (strcmp(argv[1], "relay") == 0) return cmd_relay(argc, argv);
    if (strcmp(argv[1], "tpm") == 0) return cmd_tpm(argc, argv);
    if (strcmp(argv[1], "report") == 0) return cmd_report(argc, argv);
    if (strcmp(argv[1], "score") == 0) return cmd_score(argc, argv);
    if (strcmp(argv[1], "spec") == 0) return cmd_spec(argc, argv);
    if (strcmp(argv[1], "selftest") == 0 && argc > 2) {
        if (strcmp(argv[2], "sha256") == 0) return test_sha256();
        if (strcmp(argv[2], "json") == 0) return test_json();
        if (strcmp(argv[2], "relay") == 0) return test_relay();
        if (strcmp(argv[2], "procmeter") == 0) return test_procmeter();
        if (strcmp(argv[2], "busy-child") == 0) return busy_child();
        if (strcmp(argv[2], "runner") == 0) return test_runner();
        if (strcmp(argv[2], "tpm") == 0) return test_tpm();
        if (strcmp(argv[2], "jsonread") == 0) return test_jsonread();
        if (strcmp(argv[2], "report") == 0) return test_report();
        if (strcmp(argv[2], "spec") == 0) return test_spec();
        if (strcmp(argv[2], "score") == 0) return test_score();
        if (strcmp(argv[2], "sample-game") == 0) return sample_game(argc, argv);
        if (strcmp(argv[2], "print-lines") == 0) return print_lines();
        if (strcmp(argv[2], "sleep-long") == 0) {
            ojh_sleep(60);
            return 0;
        }
    }
    return usage();
}
