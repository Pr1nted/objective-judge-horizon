#include "platform.h"
#include "report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "jsonread.h"
#include "score.h"

#define OJH_REPORT_VERSION "0.1.0"
#define MAX_RESULTS 64
#define MAX_COLUMNS 12
#define MAX_ROWS (MAX_RESULTS + 1)
#define CELL 192 /* holds the longest line put in a table (the CPU reference score, 160 bytes) */
#define TEXT_WIDTH 88

/* ---------------------------------------------------------------- collecting results */

typedef struct {
    char name[256];
    ojh_jvalue *root;
} result_file;

typedef struct {
    const char *dir;
    result_file items[MAX_RESULTS];
    int count;
    int skipped;
    char unreadable[512];
} collection;

static void collect(const char *name, void *user) {
    collection *c = user;
    size_t n = strlen(name);
    if (n < 6 || strcmp(name + n - 5, ".json") != 0) return;
    if (c->count >= MAX_RESULTS) {
        c->skipped++;
        return;
    }
    char path[4400];
    snprintf(path, sizeof path, "%s/%s", c->dir, name);
    char error[256];
    ojh_jvalue *root = ojh_jparse_file(path, error, sizeof error);
    if (!root) {
        if (!c->unreadable[0]) snprintf(c->unreadable, sizeof c->unreadable, "%s (%s)", name, error);
        c->skipped++;
        return;
    }
    if (!ojh_jpresent(ojh_jget(root, "metric"))) { /* not an OJH result */
        ojh_jfree(root);
        return;
    }
    snprintf(c->items[c->count].name, sizeof c->items[0].name, "%s", name);
    c->items[c->count].root = root;
    c->count++;
}

static int by_game_name(const void *a, const void *b) {
    const result_file *x = a, *y = b;
    return strcmp(ojh_jstring(ojh_jpath(x->root, "result.name"), x->name),
                  ojh_jstring(ojh_jpath(y->root, "result.name"), y->name));
}

/* ---------------------------------------------------------------- formatting */

/* Characters on screen, not bytes: UTF-8 continuation bytes do not count. */
static size_t display_width(const char *s) {
    size_t w = 0;
    for (; *s; s++) {
        if (((unsigned char)*s & 0xC0) != 0x80) w++;
    }
    return w;
}

static int number_present(const ojh_jvalue *v) { return v && v->type == OJH_JNUMBER; }

static void fixed(char *out, size_t n, const ojh_jvalue *v, int decimals) {
    if (!number_present(v)) snprintf(out, n, "n/a");
    else snprintf(out, n, "%.*f", decimals, v->number);
}

/* A whole number with thousands separators: 2592 -> "2,592". */
static void grouped(char *out, size_t n, double value) {
    long long x = (long long)(value < 0 ? value - 0.5 : value + 0.5);
    char digits[32];
    snprintf(digits, sizeof digits, "%lld", x < 0 ? -x : x);
    size_t len = strlen(digits), at = 0;
    char buf[48];
    if (x < 0) buf[at++] = '-';
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) buf[at++] = ',';
        buf[at++] = digits[i];
    }
    buf[at] = '\0';
    snprintf(out, n, "%s", buf);
}

/* A decimal with thousands separators: 1111.84 -> "1,111.8". */
static void grouped_decimal(char *out, size_t n, double value, int decimals) {
    char whole[48];
    double scale = 1;
    for (int i = 0; i < decimals; i++) scale *= 10;
    double rounded = (double)(long long)(value * scale + (value < 0 ? -0.5 : 0.5)) / scale;
    long long integer = (long long)rounded;
    grouped(whole, sizeof whole, (double)integer);
    if (decimals <= 0) {
        snprintf(out, n, "%s", whole);
        return;
    }
    char fraction[32];
    snprintf(fraction, sizeof fraction, "%.*f", decimals, rounded - (double)integer);
    const char *dot = strchr(fraction, '.');
    snprintf(out, n, "%s%s", whole, dot ? dot : "");
}

static void grouped_or_na(char *out, size_t n, const ojh_jvalue *v) {
    if (!number_present(v)) snprintf(out, n, "n/a");
    else grouped(out, n, v->number);
}

/* ---------------------------------------------------------------- writing both files */

typedef struct {
    FILE *md;
    FILE *txt;
} out_pair;

static void heading(out_pair *o, int level, const char *text) {
    fprintf(o->md, "%s %s\n\n", level == 1 ? "#" : level == 2 ? "##" : "###", text);
    fprintf(o->txt, "%s\n", text);
    size_t w = display_width(text);
    for (size_t i = 0; i < w; i++) fputc(level == 1 ? '=' : level == 2 ? '-' : '.', o->txt);
    fputs("\n\n", o->txt);
}

/* Greedy word wrap for the text file. */
static void wrap(FILE *f, const char *indent_first, const char *indent_rest, const char *text) {
    size_t column = strlen(indent_first);
    fputs(indent_first, f);
    const char *p = text;
    int first_word = 1;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *end = p;
        while (*end && *end != ' ') end++;
        char word[512];
        size_t len = (size_t)(end - p) < sizeof word - 1 ? (size_t)(end - p) : sizeof word - 1;
        memcpy(word, p, len);
        word[len] = '\0';
        size_t w = display_width(word);
        if (!first_word && column + 1 + w > TEXT_WIDTH) {
            fputc('\n', f);
            fputs(indent_rest, f);
            column = strlen(indent_rest);
        } else if (!first_word) {
            fputc(' ', f);
            column++;
        }
        fputs(word, f);
        column += w;
        first_word = 0;
        p = end;
    }
    fputc('\n', f);
}

static void paragraph(out_pair *o, const char *text) {
    fprintf(o->md, "%s\n\n", text);
    wrap(o->txt, "", "", text);
    fputc('\n', o->txt);
}

/* "- **lead**: rest" in Markdown, "  - lead: rest" in text. */
static void bullet(out_pair *o, const char *lead, const char *rest) {
    if (lead && *lead) fprintf(o->md, "- **%s**: %s\n", lead, rest);
    else fprintf(o->md, "- %s\n", rest);
    char line[2048];
    if (lead && *lead) snprintf(line, sizeof line, "%s: %s", lead, rest);
    else snprintf(line, sizeof line, "%s", rest);
    wrap(o->txt, "  - ", "    ", line);
}

static void end_list(out_pair *o) {
    fputc('\n', o->md);
    fputc('\n', o->txt);
}

typedef struct {
    int columns, rows;
    char cell[MAX_ROWS][MAX_COLUMNS][CELL];
} table;

static void set_cell(table *t, int row, int column, const char *text) {
    char *c = t->cell[row][column];
    snprintf(c, CELL, "%s", text);
    for (; *c; c++) {
        if (*c == '|') *c = '/'; /* a pipe would split a Markdown cell */
    }
}

static void write_table(out_pair *o, const table *t) {
    for (int r = 0; r < t->rows; r++) {
        fputc('|', o->md);
        for (int c = 0; c < t->columns; c++) fprintf(o->md, " %s |", t->cell[r][c]);
        fputc('\n', o->md);
        if (r == 0) {
            fputc('|', o->md);
            for (int c = 0; c < t->columns; c++) fputs("---|", o->md);
            fputc('\n', o->md);
        }
    }
    fputc('\n', o->md);

    size_t width[MAX_COLUMNS] = {0};
    for (int r = 0; r < t->rows; r++) {
        for (int c = 0; c < t->columns; c++) {
            size_t w = display_width(t->cell[r][c]);
            if (w > width[c]) width[c] = w;
        }
    }
    for (int r = 0; r < t->rows; r++) {
        for (int c = 0; c < t->columns; c++) {
            fputs(t->cell[r][c], o->txt);
            if (c + 1 < t->columns) {
                for (size_t pad = display_width(t->cell[r][c]); pad < width[c] + 2; pad++) fputc(' ', o->txt);
            }
        }
        fputc('\n', o->txt);
        if (r == 0) {
            for (int c = 0; c < t->columns; c++) {
                for (size_t i = 0; i < width[c]; i++) fputc('-', o->txt);
                if (c + 1 < t->columns) fputs("  ", o->txt);
            }
            fputc('\n', o->txt);
        }
    }
    fputc('\n', o->txt);
}

/* ---------------------------------------------------------------- sections */

static void machine_section(out_pair *o, const ojh_jvalue *machine, int same_everywhere) {
    heading(o, 2, "Machine");
    table *t = calloc(1, sizeof *t);
    if (!t) return;
    t->columns = 2;
    int r = 0;
    set_cell(t, r, 0, "");
    set_cell(t, r, 1, "");
    set_cell(t, r, 0, "Part");
    set_cell(t, r++, 1, "This machine");
    char buf[CELL], a[48], b[48];

    set_cell(t, r, 0, "OS");
    set_cell(t, r++, 1, ojh_jstring(ojh_jget(machine, "os"), "n/a"));
    set_cell(t, r, 0, "Model");
    set_cell(t, r++, 1, ojh_jstring(ojh_jget(machine, "model"), "n/a"));
    set_cell(t, r, 0, "CPU");
    set_cell(t, r++, 1, ojh_jstring(ojh_jget(machine, "cpu"), "n/a"));

    double perf = ojh_jnumber(ojh_jget(machine, "performance_cpus"), 0);
    double eff = ojh_jnumber(ojh_jget(machine, "efficiency_cpus"), 0);
    if (perf > 0 || eff > 0) {
        snprintf(buf, sizeof buf, "%.0f (%.0f performance, %.0f efficiency)",
                 ojh_jnumber(ojh_jget(machine, "logical_cpus"), 0), perf, eff);
    } else {
        snprintf(buf, sizeof buf, "%.0f", ojh_jnumber(ojh_jget(machine, "logical_cpus"), 0));
    }
    set_cell(t, r, 0, "Logical CPUs");
    set_cell(t, r++, 1, buf);

    snprintf(buf, sizeof buf, "%.1f GiB", ojh_jnumber(ojh_jget(machine, "memory_bytes"), 0) / (1024.0 * 1024.0 * 1024.0));
    set_cell(t, r, 0, "Memory");
    set_cell(t, r++, 1, buf);

    const char *gpu_cores = ojh_jstring(ojh_jget(machine, "gpu_cores"), "");
    if (*gpu_cores) snprintf(buf, sizeof buf, "%s (%s cores)", ojh_jstring(ojh_jget(machine, "gpu"), "n/a"), gpu_cores);
    else snprintf(buf, sizeof buf, "%s", ojh_jstring(ojh_jget(machine, "gpu"), "n/a"));
    set_cell(t, r, 0, "GPU");
    set_cell(t, r++, 1, *buf ? buf : "n/a");

    const char *display = ojh_jstring(ojh_jget(machine, "display"), "");
    set_cell(t, r, 0, "Display");
    set_cell(t, r++, 1, *display ? display : "n/a");

    const ojh_jvalue *battery = ojh_jget(machine, "on_battery");
    set_cell(t, r, 0, "Power");
    set_cell(t, r++, 1, !ojh_jpresent(battery) ? "n/a" : battery->number != 0 ? "battery" : "mains");

    const ojh_jvalue *ref = ojh_jget(machine, "reference");
    grouped_or_na(a, sizeof a, ojh_jget(ref, "single_core_rounds_per_second"));
    grouped_or_na(b, sizeof b, ojh_jget(ref, "all_cores_rounds_per_second"));
    char score[160];
    snprintf(score, sizeof score, "%s rounds/s on one core, %s on all cores", a, b);
    set_cell(t, r, 0, "CPU reference score");
    set_cell(t, r++, 1, score);

    t->rows = r;
    write_table(o, t);
    free(t);
    paragraph(o, "The reference score is OJH's own fixed CPU workload (src/machine.c), timed on this machine. "
                 "Divide a result by it to compare across hardware.");
    if (!same_everywhere) {
        paragraph(o, "Warning: these result files were not all measured on this machine. Compare numbers only "
                     "between results from one machine.");
    }
}

static int same_machine(const ojh_jvalue *a, const ojh_jvalue *b) {
    return strcmp(ojh_jstring(ojh_jget(a, "cpu"), ""), ojh_jstring(ojh_jget(b, "cpu"), "")) == 0 &&
           ojh_jnumber(ojh_jget(a, "logical_cpus"), -1) == ojh_jnumber(ojh_jget(b, "logical_cpus"), -2) &&
           ojh_jnumber(ojh_jget(a, "memory_bytes"), -1) == ojh_jnumber(ojh_jget(b, "memory_bytes"), -2);
}

/* ---------------------------------------------------------------- scores */

/* The game's name with its version when the result has one. */
static void title_of(char *out, size_t n, const ojh_jvalue *root, const char *fallback) {
    const ojh_jvalue *r = ojh_jget(root, "result");
    const char *version = ojh_jstring(ojh_jget(r, "version"), "");
    if (*version) snprintf(out, n, "%s (version %s)", ojh_jstring(ojh_jget(r, "name"), fallback), version);
    else snprintf(out, n, "%s", ojh_jstring(ojh_jget(r, "name"), fallback));
}

static void safe_id(char *out, size_t n, const char *id) {
    size_t i = 0;
    for (; id[i] && i + 1 < n; i++) {
        char ch = id[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') out[i] = ch;
        else if (ch >= 'A' && ch <= 'Z') out[i] = (char)(ch - 'A' + 'a');
        else out[i] = '-';
    }
    out[i] = '\0';
    if (i == 0) snprintf(out, n, "game");
}

static void xml_text(FILE *f, const char *s) {
    for (; *s; s++) {
        if (*s == '&') fputs("&amp;", f);
        else if (*s == '<') fputs("&lt;", f);
        else if (*s == '>') fputs("&gt;", f);
        else if (*s == '"') fputs("&quot;", f);
        else fputc(*s, f);
    }
}

/* A badge for a README or a store page. */
static int write_badge(const char *path, const ojh_score *s, const char *title) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    char number[48], value[64];
    grouped(number, sizeof number, s->total);
    snprintf(value, sizeof value, "%s%s", number, s->provisional ? " provisional" : "");
    const char *color = s->provisional ? "#8a6a1c" : "#2d6a8a";
    int left = 72, right = 16 + 7 * (int)strlen(value), width = left + right;
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"20\" role=\"img\" aria-label=\"OJH score: ", width);
    xml_text(f, value);
    fputs("\">\n  <title>", f);
    xml_text(f, title);
    fprintf(f, ": OJH score %s (score version %d)</title>\n", value, OJH_SCORE_VERSION);
    fprintf(f, "  <rect width=\"%d\" height=\"20\" rx=\"3\" fill=\"#2b3440\"/>\n", width);
    fprintf(f, "  <rect x=\"%d\" width=\"%d\" height=\"20\" rx=\"3\" fill=\"%s\"/>\n", left, right, color);
    fprintf(f, "  <rect x=\"%d\" width=\"4\" height=\"20\" fill=\"%s\"/>\n", left, color);
    fprintf(f, "  <g fill=\"#ffffff\" font-family=\"Verdana,DejaVu Sans,sans-serif\" font-size=\"11\" text-anchor=\"middle\">\n");
    fprintf(f, "    <text x=\"%d\" y=\"14\">OJH score</text>\n    <text x=\"%d\" y=\"14\">", left / 2, left + right / 2);
    xml_text(f, value);
    fputs("</text>\n  </g>\n</svg>\n", f);
    return fclose(f) == 0 ? 0 : -1;
}

static void part_figure(char *out, size_t n, const ojh_score_part *p, double v) {
    if (strcmp(p->unit, "s") == 0) {
        snprintf(out, n, "%.2f s", v);
    } else if (v >= 100) {
        char whole[48];
        grouped(whole, sizeof whole, v);
        snprintf(out, n, "%s %s", whole, p->unit);
    } else {
        snprintf(out, n, "%.2f", v);
    }
}

static int write_scorecard(const char *dir, const ojh_jvalue *root, const ojh_score *s, char *base, size_t base_len) {
    char id[64];
    safe_id(id, sizeof id, s->id);
    snprintf(base, base_len, "score-%s", id);
    char md_path[4400], txt_path[4400], svg_path[4400];
    snprintf(md_path, sizeof md_path, "%s/%s.md", dir, base);
    snprintf(txt_path, sizeof txt_path, "%s/%s.txt", dir, base);
    snprintf(svg_path, sizeof svg_path, "%s/%s.svg", dir, base);
    out_pair o = {fopen(md_path, "wb"), fopen(txt_path, "wb")};
    if (!o.md || !o.txt) {
        if (o.md) fclose(o.md);
        if (o.txt) fclose(o.txt);
        return -1;
    }
    const ojh_jvalue *r = ojh_jget(root, "result");
    const ojh_jvalue *settings = ojh_jget(root, "settings");
    const ojh_jvalue *machine = ojh_jget(root, "machine");
    char title[200], text[1200], number[48], a[48], b[48];
    title_of(title, sizeof title, root, s->name);
    snprintf(text, sizeof text, "%s: OJH score", title);
    heading(&o, 1, text);

    grouped(number, sizeof number, s->total);
    fprintf(o.md, "**%s points**, OJH score version %d%s\n\n![OJH score %s](%s.svg)\n\n", number, OJH_SCORE_VERSION,
            s->provisional ? ", provisional" : "", number, base);
    snprintf(text, sizeof text, "%s points, OJH score version %d%s", number, OJH_SCORE_VERSION,
             s->provisional ? ", provisional" : "");
    wrap(o.txt, "", "", text);
    fputc('\n', o.txt);
    paragraph(&o, "This score is built from this game's own result file and nothing else. Every part is measured "
                  "against a fixed reference level, not against other games, so adding, removing or re-running "
                  "another game never changes it.");
    if (s->reason_count) {
        heading(&o, 2, s->provisional ? "Why it is provisional" : "Notes");
        for (int i = 0; i < s->reason_count; i++) bullet(&o, NULL, s->reasons[i]);
        end_list(&o);
    }

    heading(&o, 2, "Parts");
    table *t = calloc(1, sizeof *t);
    if (t) {
        const char *header[] = {"Part", "Weight", "Measured", "On the reference CPU", "Worth 1,000 points", "Points"};
        t->columns = 6;
        for (int i = 0; i < t->columns; i++) set_cell(t, 0, i, header[i]);
        for (int i = 0; i < s->part_count; i++) {
            const ojh_score_part *p = &s->parts[i];
            char cell[CELL];
            set_cell(t, i + 1, 0, p->name);
            snprintf(cell, sizeof cell, "%.0f%%", p->weight * 100);
            set_cell(t, i + 1, 1, cell);
            if (p->present) part_figure(cell, sizeof cell, p, p->measured);
            else snprintf(cell, sizeof cell, "not reported");
            set_cell(t, i + 1, 2, cell);
            if (!p->present) snprintf(cell, sizeof cell, "n/a");
            else if (p->hardware_adjusted) part_figure(cell, sizeof cell, p, p->value);
            else if (p->value != p->measured) snprintf(cell, sizeof cell, "%.2f (capped)", p->value);
            else snprintf(cell, sizeof cell, "same");
            set_cell(t, i + 1, 3, cell);
            part_figure(cell, sizeof cell, p, p->reference);
            set_cell(t, i + 1, 4, cell);
            if (p->present) grouped(cell, sizeof cell, p->points);
            else snprintf(cell, sizeof cell, "n/a");
            set_cell(t, i + 1, 5, cell);
        }
        t->rows = s->part_count + 1;
        write_table(&o, t);
        free(t);
    }
    heading(&o, 3, "What each part measures");
    for (int i = 0; i < s->part_count; i++) {
        const ojh_score_part *p = &s->parts[i];
        snprintf(text, sizeof text, "%s.%s", p->measures, p->lower_is_better ? " Less is better." : "");
        bullet(&o, p->name, text);
    }
    end_list(&o);

    heading(&o, 2, "The run");
    const char *license = ojh_jstring(ojh_jget(r, "license"), "");
    const char *homepage = ojh_jstring(ojh_jget(r, "homepage"), "");
    if (*license || *homepage) {
        snprintf(text, sizeof text, "%s%s%s%s%s", title, *license ? ", " : "", license, *homepage ? ", " : "", homepage);
        bullet(&o, "Game", text);
    }
    bullet(&o, "Measured", ojh_jstring(ojh_jget(r, "how"), "not recorded"));
    const ojh_jvalue *chosen = ojh_jget(settings, "players_chosen");
    if (ojh_jpresent(chosen) && chosen->number != 0) {
        snprintf(a, sizeof a, "%.0f players chosen by OJH", ojh_jnumber(ojh_jget(settings, "players_requested"), 0));
    } else {
        snprintf(a, sizeof a, "players set by the game");
    }
    snprintf(text, sizeof text, "%.0f turns asked for, %d timed, seed %.0f, %s", ojh_jnumber(ojh_jget(settings, "turns_requested"), 0),
             s->turns, ojh_jnumber(ojh_jget(settings, "seed"), 0), a);
    bullet(&o, "Settings", text);
    snprintf(text, sizeof text, "%s, %s", ojh_jstring(ojh_jget(machine, "cpu"), "unknown CPU"), ojh_jstring(ojh_jget(machine, "os"), "unknown OS"));
    bullet(&o, "Machine", text);
    if (s->hardware_known) {
        grouped_or_na(a, sizeof a, ojh_jpath(machine, "reference.single_core_rounds_per_second"));
        grouped(b, sizeof b, 1000);
        snprintf(text, sizeof text, "%s rounds/s on one core; speeds were multiplied by %.3f to put them on OJH's "
                                    "reference CPU (%s rounds/s)", a, s->hardware_factor, b);
        bullet(&o, "CPU reference score", text);
    }
    end_list(&o);

    heading(&o, 2, "How the score is built");
    bullet(&o, "Points", "each part scores 1000 × log2(1 + value ÷ reference level): the reference level is worth 1,000 "
                         "points, three times it 2,000 and seven times it 3,000, and nothing scores below zero. For "
                         "start-up, where less is better, the ratio is turned around.");
    bullet(&o, "Hardware", "speeds and start-up are put on OJH's reference CPU with the machine's single-core reference "
                           "score (src/machine.c), so a faster computer does not make a faster game. A game that uses "
                           "more cores keeps that advantage.");
    snprintf(text, sizeof text, "the weighted mean of the parts the result has. Coverage is how much of the weight that "
                                "was, here %.0f%%; a part a game does not report is left out, never counted as zero.",
             s->coverage * 100);
    bullet(&o, "Total", text);
    snprintf(text, sizeof text, "score version %d. Its parts, weights and reference levels are fixed in src/score.c; "
                                "any change makes a new version, and scores of different versions are not compared. "
                                "FPS, network and memory join as parts when OJH measures them.", OJH_SCORE_VERSION);
    bullet(&o, "Version", text);
    end_list(&o);

    int ok = fclose(o.md) == 0;
    ok &= fclose(o.txt) == 0;
    ok &= write_badge(svg_path, s, title) == 0;
    return ok ? 0 : -1;
}

static void scores_section(out_pair *o, const collection *c) {
    heading(o, 2, "OJH scores");
    paragraph(o, "Each game's score is its own: it is built from that game's result file alone, against fixed "
                 "reference levels, so no game's score depends on which other games are in this report. Each "
                 "scorecard next to this report shows every part, and has a badge (.svg) to go with it.");
    table *t = calloc(1, sizeof *t);
    if (!t) return;
    const char *header[] = {"Game", "OJH score", "Coverage", "Status", "Scorecard"};
    t->columns = 5;
    for (int i = 0; i < t->columns; i++) set_cell(t, 0, i, header[i]);
    int rows = 1, provisional = 0;
    for (int i = 0; i < c->count && rows < MAX_ROWS; i++) {
        ojh_score s;
        if (ojh_score_result(c->items[i].root, &s) != 0) continue;
        char base[128], cell[CELL];
        int written = write_scorecard(c->dir, c->items[i].root, &s, base, sizeof base) == 0;
        title_of(cell, sizeof cell, c->items[i].root, s.name);
        set_cell(t, rows, 0, cell);
        grouped(cell, sizeof cell, s.total);
        set_cell(t, rows, 1, cell);
        snprintf(cell, sizeof cell, "%.0f%%", s.coverage * 100);
        set_cell(t, rows, 2, cell);
        set_cell(t, rows, 3, s.provisional ? "provisional" : "final");
        snprintf(cell, sizeof cell, "%s.md", base);
        set_cell(t, rows, 4, written ? cell : "not written");
        provisional += s.provisional;
        rows++;
    }
    t->rows = rows;
    if (rows == 1) paragraph(o, "No results here can be scored.");
    else write_table(o, t);
    free(t);
    if (provisional) {
        paragraph(o, "A provisional score comes from a run too short or incomplete to stand behind; its scorecard says "
                     "why. Measure again before publishing it.");
    }
}

int ojh_scorecard_write(const char *result_path, const char *dir, char *written, size_t written_len, char *error,
                        size_t error_len) {
    char why[256];
    ojh_jvalue *root = ojh_jparse_file(result_path, why, sizeof why);
    if (!root) {
        if (error && error_len) snprintf(error, error_len, "cannot read %s: %s", result_path, why);
        return -1;
    }
    ojh_score s;
    int status = -1;
    if (ojh_score_result(root, &s) != 0) {
        if (error && error_len) snprintf(error, error_len, "%s is not a result OJH can score", result_path);
    } else if (write_scorecard(dir, root, &s, written, written_len) != 0) {
        if (error && error_len) snprintf(error, error_len, "cannot write the scorecard in %s", dir);
    } else {
        status = 0;
    }
    ojh_jfree(root);
    return status;
}

static void tpm_section(out_pair *o, const collection *c) {
    heading(o, 2, "TPM: turns per minute");
    paragraph(o, "How many complete turns each game plays in one minute with every player controlled by its own AI. "
                 "Start-up and world generation are timed separately and are not part of the turns. The two "
                 "normalised columns multiply TPM by the number of players and by the size of the map, so a "
                 "larger game is not punished for its size.");

    table *t = calloc(1, sizeof *t);
    if (!t) return;
    const char *header[] = {"Game", "Turns timed", "TPM", "TPM × players", "TPM × regions", "Median turn (s)",
                            "Early → late turn (s)", "Start-up (s)", "Players", "Map"};
    t->columns = 10;
    for (int i = 0; i < t->columns; i++) set_cell(t, 0, i, header[i]);
    int rows = 1;

    int fewest = 0, most = 0, least_players = 0, most_players = 0, averages_only = 0, no_regions = 0, failed = 0;
    for (int i = 0; i < c->count && rows < MAX_ROWS; i++) {
        const ojh_jvalue *root = c->items[i].root;
        if (strcmp(ojh_jstring(ojh_jget(root, "metric"), ""), "tpm") != 0) continue;
        const ojh_jvalue *r = ojh_jget(root, "result");
        char buf[CELL], a[48], b[48];
        int turns = (int)ojh_jnumber(ojh_jget(r, "turns"), 0);
        int players = (int)ojh_jnumber(ojh_jget(r, "players"), 0);

        title_of(buf, sizeof buf, root, c->items[i].name);
        set_cell(t, rows, 0, buf);
        grouped(buf, sizeof buf, turns);
        set_cell(t, rows, 1, buf);
        if (turns > 0 && number_present(ojh_jget(r, "tpm"))) grouped_decimal(buf, sizeof buf, ojh_jget(r, "tpm")->number, 1);
        else snprintf(buf, sizeof buf, "n/a");
        set_cell(t, rows, 2, buf);
        grouped_or_na(buf, sizeof buf, ojh_jget(r, "tpm_x_players"));
        set_cell(t, rows, 3, buf);
        grouped_or_na(buf, sizeof buf, ojh_jget(r, "tpm_x_regions"));
        set_cell(t, rows, 4, buf);

        const ojh_jvalue *per_turn = ojh_jget(r, "per_turn");
        fixed(buf, sizeof buf, ojh_jget(per_turn, "median_seconds"), 3);
        set_cell(t, rows, 5, buf);
        if (number_present(ojh_jget(per_turn, "early_median_seconds")) &&
            number_present(ojh_jget(per_turn, "late_median_seconds"))) {
            fixed(a, sizeof a, ojh_jget(per_turn, "early_median_seconds"), 3);
            fixed(b, sizeof b, ojh_jget(per_turn, "late_median_seconds"), 3);
            snprintf(buf, sizeof buf, "%s → %s", a, b);
        } else {
            snprintf(buf, sizeof buf, "n/a");
        }
        set_cell(t, rows, 6, buf);
        fixed(buf, sizeof buf, ojh_jget(r, "boot_seconds"), 2);
        set_cell(t, rows, 7, buf);
        if (players > 0) grouped(buf, sizeof buf, players);
        else snprintf(buf, sizeof buf, "n/a");
        set_cell(t, rows, 8, buf);
        if (number_present(ojh_jget(r, "regions"))) {
            grouped(a, sizeof a, ojh_jnumber(ojh_jget(r, "regions"), 0));
            snprintf(buf, sizeof buf, "%s %s", a, ojh_jstring(ojh_jget(r, "region_kind"), "regions"));
        } else {
            snprintf(buf, sizeof buf, "n/a");
        }
        set_cell(t, rows, 9, buf);
        rows++;

        if (turns > 0 && (fewest == 0 || turns < fewest)) fewest = turns;
        if (turns > most) most = turns;
        if (players > 0 && (least_players == 0 || players < least_players)) least_players = players;
        if (players > most_players) most_players = players;
        if (!ojh_jpresent(per_turn)) averages_only++;
        if (!ojh_jpresent(ojh_jget(r, "regions"))) no_regions++;
        if (ojh_jpresent(ojh_jget(root, "error"))) failed++;
    }
    t->rows = rows;
    if (rows == 1) {
        paragraph(o, "No TPM results in this folder.");
        free(t);
        return;
    }
    write_table(o, t);
    free(t);

    heading(o, 3, "How each game was timed");
    for (int i = 0; i < c->count; i++) {
        const ojh_jvalue *root = c->items[i].root;
        if (strcmp(ojh_jstring(ojh_jget(root, "metric"), ""), "tpm") != 0) continue;
        const ojh_jvalue *r = ojh_jget(root, "result");
        const ojh_jvalue *s = ojh_jget(root, "settings");
        char line[1024], who[96];
        const ojh_jvalue *chosen = ojh_jget(s, "players_chosen");
        const char *game = ojh_jstring(ojh_jget(r, "game"), "");
        int by_ojh = ojh_jpresent(chosen) ? chosen->number != 0
                                          : (strcmp(game, "freeciv") == 0 || strcmp(game, "unciv") == 0);
        if (by_ojh) snprintf(who, sizeof who, "%.0f players, chosen by OJH", ojh_jnumber(ojh_jget(s, "players_requested"), 0));
        else snprintf(who, sizeof who, "players as the game's own scenario or world sets them");
        snprintf(line, sizeof line, "%s. Asked for %.0f turns, seed %.0f, %s.", ojh_jstring(ojh_jget(r, "how"), "not recorded"),
                 ojh_jnumber(ojh_jget(s, "turns_requested"), 0), ojh_jnumber(ojh_jget(s, "seed"), 0), who);
        bullet(o, ojh_jstring(ojh_jget(r, "name"), c->items[i].name), line);
    }
    end_list(o);

    heading(o, 3, "Before comparing these numbers");
    char line[1024];
    if (most > fewest && fewest > 0) {
        snprintf(line, sizeof line,
                 "The runs timed different numbers of turns (%d to %d). Turns get slower as a game goes on, so a "
                 "shorter run reads faster: compare runs of the same length.", fewest, most);
        bullet(o, "Run length", line);
    }
    if (most_players > least_players && least_players > 0) {
        snprintf(line, sizeof line,
                 "Player counts differ (%d to %d), and so do map sizes. The × players and × regions columns account "
                 "for size, not for rules or how much thinking each AI does.", least_players, most_players);
        bullet(o, "Game size", line);
    }
    if (averages_only) {
        bullet(o, "Spread", "A game that reports only an average turn time has n/a for its median and its early and "
                            "late turns; its TPM is still exact over the turns it timed.");
    }
    if (no_regions) {
        bullet(o, "Map size", "A game that does not report its map size has n/a for TPM × regions.");
    }
    if (failed) {
        snprintf(line, sizeof line, "%d run(s) did not finish cleanly; their result file has the error.", failed);
        bullet(o, "Failed runs", line);
    }
    bullet(o, "Scope", "Every number comes from one machine under the fairness rules in README.md. None of it is a "
                       "claim about other hardware.");
    end_list(o);
}

/* ---------------------------------------------------------------- entry point */

int ojh_report_write(const char *dir, char *error, size_t error_len) {
    collection *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->dir = dir;
    if (ojh_list_dir(dir, collect, c) != 0) {
        if (error && error_len) snprintf(error, error_len, "cannot read the folder %s", dir);
        free(c);
        return -1;
    }
    if (c->count == 0) {
        if (error && error_len) {
            snprintf(error, error_len, "no OJH result files in %s%s%s", dir, c->unreadable[0] ? "; unreadable: " : "",
                     c->unreadable);
        }
        free(c);
        return -1;
    }
    qsort(c->items, (size_t)c->count, sizeof c->items[0], by_game_name);

    char md_path[4400], txt_path[4400];
    snprintf(md_path, sizeof md_path, "%s/report.md", dir);
    snprintf(txt_path, sizeof txt_path, "%s/report.txt", dir);
    out_pair o = {fopen(md_path, "wb"), fopen(txt_path, "wb")};
    if (!o.md || !o.txt) {
        if (o.md) fclose(o.md);
        if (o.txt) fclose(o.txt);
        if (error && error_len) snprintf(error, error_len, "cannot write the report in %s", dir);
        for (int i = 0; i < c->count; i++) ojh_jfree(c->items[i].root);
        free(c);
        return -1;
    }

    heading(&o, 1, "Objective Judge Horizon (OJH) report");
    char when[64] = "";
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (local) strftime(when, sizeof when, "%Y-%m-%d %H:%M", local);
    char intro[1200];
    snprintf(intro, sizeof intro, "Written by OJH %s on %s from %d result file(s).", OJH_REPORT_VERSION, when, c->count);
    paragraph(&o, intro);
    if (c->skipped) {
        snprintf(intro, sizeof intro, "%d file(s) were skipped%s%s.", c->skipped, c->unreadable[0] ? ", first: " : "",
                 c->unreadable);
        paragraph(&o, intro);
    }

    const ojh_jvalue *machine = ojh_jget(c->items[0].root, "machine");
    int same = 1;
    for (int i = 1; i < c->count; i++) same &= same_machine(machine, ojh_jget(c->items[i].root, "machine"));
    machine_section(&o, machine, same);
    scores_section(&o, c);
    tpm_section(&o, c);

    fclose(o.md);
    fclose(o.txt);
    for (int i = 0; i < c->count; i++) ojh_jfree(c->items[i].root);
    free(c);
    return 0;
}
