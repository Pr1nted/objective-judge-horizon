#include "gamespec.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsonread.h"
#include "runner.h"
#include "tpm.h"

static const char *const TOP_KEYS[] = {"ojh_game_spec", "id", "name", "version", "license", "homepage", "notes",
                                       "command", "working_directory", "environment", "turns", "players",
                                       "regions", "region_kind", "default_turns", "timeout_seconds"};
static const char *const TURN_KEYS[] = {"from", "turn_ends", "game_starts", "stream", "players_after",
                                        "regions_after"};
static const char *const PLACEHOLDERS[] = {"turns", "seed", "players", "work", "spec_dir", "ojh"};

#define COUNT(a) (sizeof a / sizeof a[0])

static int fail(char *error, size_t len, const char *format, ...) {
    if (error && len) {
        va_list args;
        va_start(args, format);
        vsnprintf(error, len, format, args);
        va_end(args);
    }
    return -1;
}

static char *dup_string(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static int in_list(const char *key, const char *const *list, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (strcmp(key, list[i]) == 0) return 1;
    }
    return 0;
}

/* 0 when every {name} in text is one OJH fills in; otherwise the first unknown one is in bad. */
static int check_placeholders(const char *text, char *bad, size_t bad_len) {
    for (const char *p = strchr(text, '{'); p; p = strchr(p + 1, '{')) {
        const char *end = strchr(p, '}');
        if (!end) {
            snprintf(bad, bad_len, "%s", p);
            return -1;
        }
        size_t n = (size_t)(end - p - 1);
        int known = 0;
        for (size_t i = 0; i < COUNT(PLACEHOLDERS); i++) {
            if (strlen(PLACEHOLDERS[i]) == n && strncmp(p + 1, PLACEHOLDERS[i], n) == 0) known = 1;
        }
        if (!known) {
            snprintf(bad, bad_len, "%.*s", (int)(end - p + 1), p);
            return -1;
        }
    }
    return 0;
}

/* A template string: known placeholders only. Copies it into *out. */
static int template_string(const ojh_jvalue *v, const char *where, char **out, char *error, size_t error_len) {
    if (!v || v->type != OJH_JSTRING) return fail(error, error_len, "%s must be a string", where);
    char bad[64];
    if (check_placeholders(v->string, bad, sizeof bad) != 0) {
        return fail(error, error_len,
                    "%s has %s, which OJH does not fill in (it fills in {turns}, {seed}, {players}, {work}, "
                    "{spec_dir} and {ojh})", where, bad);
    }
    *out = dup_string(v->string);
    return *out ? 0 : fail(error, error_len, "out of memory");
}

/* An optional string copied into a fixed buffer. */
static int optional_text(const ojh_jvalue *object, const char *key, char *dst, size_t n, char *error,
                         size_t error_len) {
    const ojh_jvalue *v = ojh_jget(object, key);
    if (!ojh_jpresent(v)) return 0;
    if (v->type != OJH_JSTRING) return fail(error, error_len, "\"%s\" must be a string", key);
    if (strlen(v->string) >= n) return fail(error, error_len, "\"%s\" is longer than %lu characters", key, (unsigned long)(n - 1));
    snprintf(dst, n, "%s", v->string);
    return 0;
}

static int optional_copy(const ojh_jvalue *object, const char *key, char **dst, char *error, size_t error_len) {
    const ojh_jvalue *v = ojh_jget(object, key);
    if (!ojh_jpresent(v)) return 0;
    if (v->type != OJH_JSTRING || !*v->string) return fail(error, error_len, "\"turns\".\"%s\" must be a non-empty string", key);
    *dst = dup_string(v->string);
    return *dst ? 0 : fail(error, error_len, "out of memory");
}

static int whole_number(const ojh_jvalue *v, double at_least, double *out) {
    if (!v || v->type != OJH_JNUMBER || v->number < at_least || v->number != (double)(long long)v->number) return -1;
    *out = v->number;
    return 0;
}

static int read_turns(const ojh_jvalue *turns, ojh_gamespec *s, char *error, size_t error_len) {
    if (!turns || turns->type != OJH_JOBJECT) {
        return fail(error, error_len,
                    "\"turns\" is missing: say how OJH tells a turn has ended, {\"from\": \"protocol\"} when the "
                    "game prints OJH turn lines or {\"from\": \"marker\", \"turn_ends\": \"...\"}");
    }
    for (size_t i = 0; i < turns->count; i++) {
        if (!in_list(turns->items[i].key, TURN_KEYS, COUNT(TURN_KEYS))) {
            return fail(error, error_len,
                        "\"turns\" has an unknown key \"%s\" (it can have from, turn_ends, game_starts, stream, "
                        "players_after and regions_after)", turns->items[i].key);
        }
    }
    const char *from = ojh_jstring(ojh_jget(turns, "from"), "");
    if (strcmp(from, "protocol") == 0) s->turns_from = OJH_TURNS_PROTOCOL;
    else if (strcmp(from, "marker") == 0) s->turns_from = OJH_TURNS_MARKER;
    else return fail(error, error_len, "\"turns\".\"from\" must be \"protocol\" or \"marker\"");

    if (optional_copy(turns, "turn_ends", &s->turn_ends, error, error_len) != 0 ||
        optional_copy(turns, "game_starts", &s->game_starts, error, error_len) != 0 ||
        optional_copy(turns, "players_after", &s->players_after, error, error_len) != 0 ||
        optional_copy(turns, "regions_after", &s->regions_after, error, error_len) != 0) {
        return -1;
    }
    if (s->turns_from == OJH_TURNS_MARKER && !s->turn_ends) {
        return fail(error, error_len,
                    "\"turns\" reads a marker but has no \"turn_ends\": give text the game prints once at the "
                    "end of every turn");
    }
    if (s->turns_from == OJH_TURNS_PROTOCOL && (s->turn_ends || s->game_starts)) {
        return fail(error, error_len,
                    "\"turn_ends\" and \"game_starts\" are for \"from\": \"marker\"; with the protocol the game "
                    "prints OJH turn lines itself");
    }
    const ojh_jvalue *stream = ojh_jget(turns, "stream");
    if (ojh_jpresent(stream)) {
        const char *name = ojh_jstring(stream, "");
        if (strcmp(name, "stdout") == 0) s->stream = OJH_STDOUT;
        else if (strcmp(name, "stderr") == 0) s->stream = OJH_STDERR;
        else if (strcmp(name, "either") == 0) s->stream = OJH_STREAM_EITHER;
        else return fail(error, error_len, "\"turns\".\"stream\" must be \"stdout\", \"stderr\" or \"either\"");
    }
    return 0;
}

static int read_spec(const ojh_jvalue *root, ojh_gamespec *s, char *error, size_t error_len) {
    if (!root || root->type != OJH_JOBJECT) return fail(error, error_len, "a spec must be a JSON object");
    for (size_t i = 0; i < root->count; i++) {
        if (!in_list(root->items[i].key, TOP_KEYS, COUNT(TOP_KEYS))) {
            return fail(error, error_len,
                        "unknown key \"%s\" (docs/adding-your-game.md lists the keys a spec can have)",
                        root->items[i].key);
        }
    }
    if (ojh_jnumber(ojh_jget(root, "ojh_game_spec"), 0) != OJH_GAMESPEC_VERSION) {
        return fail(error, error_len, "\"ojh_game_spec\" must be %d, the spec version this OJH reads",
                    OJH_GAMESPEC_VERSION);
    }

    const ojh_jvalue *id = ojh_jget(root, "id");
    if (!id || id->type != OJH_JSTRING || !*id->string || strlen(id->string) >= sizeof s->id) {
        return fail(error, error_len, "\"id\" must be a short name for result files, like \"my-game\"");
    }
    for (const char *c = id->string; *c; c++) {
        if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '_')) {
            return fail(error, error_len, "\"id\" may only use lower-case letters, digits, - and _ (got \"%s\")",
                        id->string);
        }
    }
    ojh_game builtin;
    if (ojh_game_parse(id->string, &builtin) == 0) {
        return fail(error, error_len, "\"id\" \"%s\" belongs to a game OJH measures itself; choose another",
                    id->string);
    }
    snprintf(s->id, sizeof s->id, "%s", id->string);

    const ojh_jvalue *name = ojh_jget(root, "name");
    if (!name || name->type != OJH_JSTRING || !*name->string) {
        return fail(error, error_len, "\"name\" must be the game's name as reports should show it");
    }
    if (optional_text(root, "name", s->name, sizeof s->name, error, error_len) != 0 ||
        optional_text(root, "version", s->version, sizeof s->version, error, error_len) != 0 ||
        optional_text(root, "license", s->license, sizeof s->license, error, error_len) != 0 ||
        optional_text(root, "homepage", s->homepage, sizeof s->homepage, error, error_len) != 0 ||
        optional_text(root, "region_kind", s->region_kind, sizeof s->region_kind, error, error_len) != 0) {
        return -1;
    }

    const ojh_jvalue *command = ojh_jget(root, "command");
    if (!command || command->type != OJH_JARRAY || command->count == 0) {
        return fail(error, error_len,
                    "\"command\" must be a list: the program, then its arguments, e.g. [\"./MyGame\", "
                    "\"--autoplay\", \"{turns}\"]");
    }
    s->command = calloc(command->count, sizeof *s->command);
    if (!s->command) return fail(error, error_len, "out of memory");
    for (size_t i = 0; i < command->count; i++) {
        char where[48];
        snprintf(where, sizeof where, "\"command\" item %lu", (unsigned long)(i + 1));
        if (template_string(&command->items[i], where, &s->command[i], error, error_len) != 0) return -1;
        s->command_count++;
    }
    if (!*s->command[0]) return fail(error, error_len, "\"command\" starts with an empty program name");

    const ojh_jvalue *cwd = ojh_jget(root, "working_directory");
    if (ojh_jpresent(cwd) &&
        template_string(cwd, "\"working_directory\"", &s->working_directory, error, error_len) != 0) {
        return -1;
    }

    const ojh_jvalue *env = ojh_jget(root, "environment");
    if (ojh_jpresent(env)) {
        if (env->type != OJH_JOBJECT) {
            return fail(error, error_len, "\"environment\" must be an object of names and values, e.g. {\"LC_ALL\": \"C\"}");
        }
        s->environment = calloc(env->count ? env->count : 1, sizeof *s->environment);
        if (!s->environment) return fail(error, error_len, "out of memory");
        for (size_t i = 0; i < env->count; i++) {
            const ojh_jvalue *item = &env->items[i];
            char where[160];
            snprintf(where, sizeof where, "\"environment\".\"%.100s\"", item->key);
            if (!*item->key || strchr(item->key, '=')) {
                return fail(error, error_len, "%s is not a usable variable name", where);
            }
            char *value = NULL;
            if (template_string(item, where, &value, error, error_len) != 0) return -1;
            size_t n = strlen(item->key) + strlen(value) + 2;
            s->environment[i] = malloc(n);
            if (!s->environment[i]) {
                free(value);
                return fail(error, error_len, "out of memory");
            }
            snprintf(s->environment[i], n, "%s=%s", item->key, value);
            free(value);
            s->environment_count++;
        }
    }

    if (read_turns(ojh_jget(root, "turns"), s, error, error_len) != 0) return -1;

    double number;
    const ojh_jvalue *players = ojh_jget(root, "players");
    if (ojh_jpresent(players)) {
        if (whole_number(players, 1, &number) != 0) return fail(error, error_len, "\"players\" must be a whole number above 0");
        s->players = (int)number;
    }
    const ojh_jvalue *regions = ojh_jget(root, "regions");
    if (ojh_jpresent(regions)) {
        if (whole_number(regions, 1, &number) != 0) return fail(error, error_len, "\"regions\" must be a whole number above 0");
        s->regions = (long)number;
    }
    const ojh_jvalue *turns = ojh_jget(root, "default_turns");
    if (ojh_jpresent(turns)) {
        if (whole_number(turns, 1, &number) != 0) return fail(error, error_len, "\"default_turns\" must be a whole number above 0");
        s->default_turns = (int)number;
    }
    const ojh_jvalue *timeout = ojh_jget(root, "timeout_seconds");
    if (ojh_jpresent(timeout)) {
        if (timeout->type != OJH_JNUMBER || timeout->number <= 0) {
            return fail(error, error_len, "\"timeout_seconds\" must be a number above 0");
        }
        s->timeout_seconds = timeout->number;
    }

    for (int i = 0; i < s->command_count; i++) s->players_chosen |= strstr(s->command[i], "{players}") != NULL;
    for (int i = 0; i < s->environment_count; i++) s->players_chosen |= strstr(s->environment[i], "{players}") != NULL;
    return 0;
}

int ojh_gamespec_parse(const char *text, size_t len, const char *spec_dir, ojh_gamespec *s, char *error,
                       size_t error_len) {
    memset(s, 0, sizeof *s);
    s->default_turns = 100;
    s->timeout_seconds = 3600;
    s->stream = OJH_STREAM_EITHER;
    snprintf(s->spec_dir, sizeof s->spec_dir, "%s", spec_dir && *spec_dir ? spec_dir : ".");
    char why[256];
    ojh_jvalue *root = ojh_jparse(text, len, why, sizeof why);
    if (!root) return fail(error, error_len, "the spec is not valid JSON: %s", why);
    int status = read_spec(root, s, error, error_len);
    ojh_jfree(root);
    if (status != 0) ojh_gamespec_free(s);
    return status;
}

int ojh_gamespec_load(const char *path, ojh_gamespec *s, char *error, size_t error_len) {
    memset(s, 0, sizeof *s);
    FILE *f = fopen(path, "rb");
    if (!f) return fail(error, error_len, "cannot open the spec %s", path);
    size_t cap = 1 << 16, len = 0;
    char *text = malloc(cap);
    while (text) {
        size_t n = fread(text + len, 1, cap - len, f);
        len += n;
        if (len < cap) break;
        if (cap >= (size_t)1 << 24) {
            free(text);
            fclose(f);
            return fail(error, error_len, "the spec %s is larger than 16 MiB", path);
        }
        char *grown = realloc(text, cap * 2);
        if (!grown) {
            free(text);
            text = NULL;
            break;
        }
        text = grown;
        cap *= 2;
    }
    fclose(f);
    if (!text) return fail(error, error_len, "out of memory");

    char dir[1024];
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    char *backslash = strrchr(dir, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    if (slash) *slash = '\0';
    else snprintf(dir, sizeof dir, ".");
    if (!*dir) snprintf(dir, sizeof dir, "/");

    char why[768];
    int status = ojh_gamespec_parse(text, len, dir, s, why, sizeof why);
    free(text);
    if (status != 0) return fail(error, error_len, "%s: %s", path, why);
    return 0;
}

/* ---------------------------------------------------------------- filling in */

typedef struct {
    char *text;
    size_t len, cap;
    int broken;
} buffer;

static void put(buffer *b, const char *s, size_t n) {
    if (b->broken) return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (b->len + n + 1 > cap) cap *= 2;
        char *grown = realloc(b->text, cap);
        if (!grown) {
            b->broken = 1;
            return;
        }
        b->text = grown;
        b->cap = cap;
    }
    memcpy(b->text + b->len, s, n);
    b->len += n;
    b->text[b->len] = '\0';
}

char *ojh_gamespec_expand(const char *t, const ojh_gamespec *s, const ojh_spec_values *v) {
    buffer b = {0};
    put(&b, "", 0);
    while (*t) {
        const char *open = strchr(t, '{');
        const char *close = open ? strchr(open, '}') : NULL;
        if (!open || !close) {
            put(&b, t, strlen(t));
            break;
        }
        put(&b, t, (size_t)(open - t));
        size_t n = (size_t)(close - open - 1);
        char number[32];
        const char *value = NULL;
        if (n == 5 && strncmp(open + 1, "turns", 5) == 0) snprintf(number, sizeof number, "%d", v->turns), value = number;
        else if (n == 4 && strncmp(open + 1, "seed", 4) == 0) snprintf(number, sizeof number, "%u", v->seed), value = number;
        else if (n == 7 && strncmp(open + 1, "players", 7) == 0) snprintf(number, sizeof number, "%d", v->players), value = number;
        else if (n == 4 && strncmp(open + 1, "work", 4) == 0) value = v->work ? v->work : ".";
        else if (n == 8 && strncmp(open + 1, "spec_dir", 8) == 0) value = s->spec_dir;
        else if (n == 3 && strncmp(open + 1, "ojh", 3) == 0) value = v->ojh ? v->ojh : "ojh";
        if (value) put(&b, value, strlen(value));
        else put(&b, open, n + 2); /* not a placeholder: kept as written */
        t = close + 1;
    }
    if (b.broken) {
        free(b.text);
        return NULL;
    }
    return b.text;
}

static int is_absolute(const char *p) {
    return p[0] == '/' || p[0] == '\\' || (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':');
}

char *ojh_gamespec_path(const ojh_gamespec *s, const char *path, int bare_names_stay) {
    int has_folder = strchr(path, '/') || strchr(path, '\\');
    if (is_absolute(path) || (bare_names_stay && !has_folder)) return dup_string(path);
    const char *rest = path;
    while ((rest[0] == '.' && (rest[1] == '/' || rest[1] == '\\'))) rest += 2; /* "./game" is "game" in the spec's folder */
    if (!*rest || strcmp(rest, ".") == 0) return dup_string(s->spec_dir);
    if (strcmp(s->spec_dir, ".") == 0) {
        size_t n = strlen(rest) + 3;
        char *out = malloc(n);
        if (out) snprintf(out, n, "./%s", rest); /* keeps a folder in the name so PATH is not searched */
        return out;
    }
    size_t n = strlen(s->spec_dir) + strlen(rest) + 2;
    char *out = malloc(n);
    if (out) snprintf(out, n, "%s/%s", s->spec_dir, rest);
    return out;
}

/* ---------------------------------------------------------------- template */

static const char TEMPLATE[] =
    "{\n"
    "  \"ojh_game_spec\": 1,\n"
    "  \"id\": \"my-game\",\n"
    "  \"name\": \"My Game\",\n"
    "  \"version\": \"1.0.0\",\n"
    "  \"license\": \"MIT\",\n"
    "  \"homepage\": \"https://example.com/my-game\",\n"
    "  \"notes\": [\n"
    "    \"Fill this in, then check it with: ojh spec check my-game.json\",\n"
    "    \"command starts your game's AI-only mode. OJH fills in {turns}, {seed}, {players}, {work}, {spec_dir} and {ojh}.\",\n"
    "    \"turns.from protocol: your game prints 'OJH ready' when turn 1 starts and 'OJH turn N' when turn N ends.\",\n"
    "    \"turns.from marker: OJH times the gaps between lines containing turn_ends, text your game already prints.\",\n"
    "    \"Full reference: docs/adding-your-game.md\"\n"
    "  ],\n"
    "  \"command\": [\"./MyGameServer\", \"--ai-only\", \"--turns\", \"{turns}\", \"--seed\", \"{seed}\", \"--players\", \"{players}\"],\n"
    "  \"environment\": {\"LC_ALL\": \"C\"},\n"
    "  \"turns\": {\"from\": \"protocol\"},\n"
    "  \"region_kind\": \"provinces\",\n"
    "  \"default_turns\": 200,\n"
    "  \"timeout_seconds\": 3600\n"
    "}\n";

int ojh_gamespec_write_template(const char *path, char *error, size_t error_len) {
    FILE *existing = fopen(path, "rb");
    if (existing) {
        fclose(existing);
        return fail(error, error_len, "%s already exists; OJH will not overwrite it", path);
    }
    FILE *f = fopen(path, "wb");
    if (!f) return fail(error, error_len, "cannot write %s", path);
    size_t n = sizeof TEMPLATE - 1;
    int ok = fwrite(TEMPLATE, 1, n, f) == n;
    ok &= fclose(f) == 0;
    return ok ? 0 : fail(error, error_len, "could not finish writing %s", path);
}

void ojh_gamespec_free(ojh_gamespec *s) {
    if (!s) return;
    for (int i = 0; i < s->command_count; i++) free(s->command[i]);
    for (int i = 0; i < s->environment_count; i++) free(s->environment[i]);
    free(s->command);
    free(s->environment);
    free(s->working_directory);
    free(s->turn_ends);
    free(s->game_starts);
    free(s->players_after);
    free(s->regions_after);
    s->command = s->environment = NULL;
    s->working_directory = s->turn_ends = s->game_starts = s->players_after = s->regions_after = NULL;
    s->command_count = s->environment_count = 0;
}
