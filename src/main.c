#ifndef _WIN32
#  define _DEFAULT_SOURCE
#  define _DARWIN_C_SOURCE
#endif
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "machine.h"
#include "netmeter.h"
#include "procmeter.h"
#include "runner.h"
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
          "  ojh tpm <opendoctrines|gd5|freeciv|unciv> [--turns N] [--seed S] [--players P] [--timeout S]\n"
          "          [--od-server PATH --od-data DIR] [--gd5-python PATH --gd5-dir DIR]\n"
          "          [--freeciv-server PATH] [--unciv-jar PATH --java PATH --javac PATH]\n"
          "          [--drivers DIR] [--work DIR] [--out FILE]\n"
          "                                         turns per minute, every player AI\n"
          "  ojh selftest sha256|json|relay|procmeter|runner|tpm\n",
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

static int cmd_tpm(int argc, char **argv) {
    if (argc < 3) return usage();
    ojh_game game;
    if (ojh_game_parse(argv[2], &game) != 0) {
        fprintf(stderr, "tpm: unknown game '%s'\n", argv[2]);
        return 2;
    }
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = getenv("TEMP");
    if (!tmp) tmp = ".";
    static char work[1024];
    snprintf(work, sizeof work, "%s/ojh-work", tmp);

    ojh_tpm_options o;
    memset(&o, 0, sizeof o);
    o.turns = game == OJH_GAME_OPENDOCTRINES ? 500 : 100;
    o.seed = 20260914u;
    o.players = 8;
    o.timeout_seconds = 3600;
    o.freeciv_server = "freeciv-server";
    o.java = "java";
    o.javac = "javac";
    o.drivers_dir = "drivers";
    o.work_dir = work;
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
    fprintf(stderr, "tpm: %s for %d turns\n", ojh_game_name(game), o.turns);

    ojh_tpm result;
    char error[1024] = "";
    int status = ojh_tpm_run(game, &o, &result, error, sizeof error);

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
    ojh_json_end_object(&w);
    ojh_json_key(&w, "result"); ojh_tpm_json(&w, &result);
    ojh_json_key(&w, "error");
    if (status == 0) ojh_json_null(&w);
    else ojh_json_string(&w, error);
    ojh_json_end_object(&w);
    if (out != stdout) fclose(out);
    if (status == 0) {
        fprintf(stderr, "tpm: %s played %d turns in %.2f s: %.1f turns per minute\n", ojh_game_name(game),
                result.turns, result.play_seconds, ojh_tpm_value(&result));
    } else {
        fprintf(stderr, "tpm: %s failed: %s\n", ojh_game_name(game), error);
    }
    ojh_tpm_free(&result);
    return status == 0 ? 0 : 1;
}

/* ---- self-tests: each proves one piece against a known answer */

static int close_to(double a, double b) { return a - b < 1e-6 && b - a < 1e-6; }

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
    if (strcmp(argv[1], "selftest") == 0 && argc > 2) {
        if (strcmp(argv[2], "sha256") == 0) return test_sha256();
        if (strcmp(argv[2], "json") == 0) return test_json();
        if (strcmp(argv[2], "relay") == 0) return test_relay();
        if (strcmp(argv[2], "procmeter") == 0) return test_procmeter();
        if (strcmp(argv[2], "busy-child") == 0) return busy_child();
        if (strcmp(argv[2], "runner") == 0) return test_runner();
        if (strcmp(argv[2], "tpm") == 0) return test_tpm();
        if (strcmp(argv[2], "print-lines") == 0) return print_lines();
        if (strcmp(argv[2], "sleep-long") == 0) {
            ojh_sleep(60);
            return 0;
        }
    }
    return usage();
}
