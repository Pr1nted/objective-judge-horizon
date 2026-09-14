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
#include "sha256.h"

#define OJH_VERSION "0.1.0"

static int usage(void) {
    fputs("Objective Judge Horizon (OJH) " OJH_VERSION "\n"
          "turn-based strategy games measured the same way on the same machine\n"
          "\n"
          "  ojh machine [seconds]                  this machine's profile and CPU reference score\n"
          "  ojh relay <listen> <host> <port> <seconds> [clients]\n"
          "                                         count a netcode's traffic on loopback\n"
          "  ojh selftest sha256|json|relay|procmeter\n",
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

/* ---- self-tests: each proves one piece against a known answer */

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

int main(int argc, char **argv) {
    ojh_ignore_sigpipe();
    if (ojh_net_init() != 0) {
        fputs("cannot start networking\n", stderr);
        return 1;
    }
    if (argc < 2) return usage();
    if (strcmp(argv[1], "machine") == 0) return cmd_machine(argc, argv);
    if (strcmp(argv[1], "relay") == 0) return cmd_relay(argc, argv);
    if (strcmp(argv[1], "selftest") == 0 && argc > 2) {
        if (strcmp(argv[2], "sha256") == 0) return test_sha256();
        if (strcmp(argv[2], "json") == 0) return test_json();
        if (strcmp(argv[2], "relay") == 0) return test_relay();
        if (strcmp(argv[2], "procmeter") == 0) return test_procmeter();
        if (strcmp(argv[2], "busy-child") == 0) return busy_child();
    }
    return usage();
}
