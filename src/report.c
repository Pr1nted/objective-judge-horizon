#include "platform.h"
#include "report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "chart.h"
#include "jsonread.h"
#include "score.h"
#include "stats.h"

#define OJH_REPORT_VERSION "0.2.0"
#define MAX_RESULTS 64
#define MAX_COLUMNS 14
#define MAX_ROWS (MAX_RESULTS + 1)
#define CELL 192
#define TEXT_WIDTH 88
#define MAX_STATS 64

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
    if (!ojh_jpresent(ojh_jget(root, "metric"))) {
        ojh_jfree(root);
        return;
    }
    snprintf(c->items[c->count].name, sizeof c->items[0].name, "%s", name);
    c->items[c->count].root = root;
    c->count++;
}

static void ignore_name(const char *name, void *user) {
    (void)name;
    (void)user;
}

static int by_file_name(const void *a, const void *b) {
    return strcmp(((const result_file *)a)->name, ((const result_file *)b)->name);
}

static size_t display_width(const char *s) {
    size_t w = 0;
    for (; *s; s++) {
        if (((unsigned char)*s & 0xC0) != 0x80) w++;
    }
    return w;
}

static int number_present(const ojh_jvalue *v) { return v && v->type == OJH_JNUMBER; }

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

static void grouped_or_na(char *out, size_t n, const ojh_jvalue *v) {
    if (!number_present(v)) snprintf(out, n, "n/a");
    else grouped(out, n, v->number);
}

static const char *ordinal_suffix(int n) {
    int tens = n % 100;
    if (tens >= 11 && tens <= 13) return "th";
    return n % 10 == 1 ? "st" : n % 10 == 2 ? "nd" : n % 10 == 3 ? "rd" : "th";
}

static void stat_value_text(char *out, size_t n, const ojh_stat *s, double v) {
    ojh_format_value(out, n, v, s->unit, s->word);
}

typedef struct {
    FILE *md;
    FILE *txt;
} out_pair;

static void heading(out_pair *o, int level, const char *text) {
    fprintf(o->md, "%s %s\n\n", level == 1 ? "#" : level == 2 ? "##" : level == 3 ? "###" : "####", text);
    fprintf(o->txt, "%s\n", text);
    size_t w = display_width(text);
    for (size_t i = 0; i < w; i++) fputc(level == 1 ? '=' : level == 2 ? '-' : '.', o->txt);
    fputs("\n\n", o->txt);
}

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

static void bullet(out_pair *o, const char *lead, const char *rest) {
    if (lead && *lead) fprintf(o->md, "- **%s**: %s\n", lead, rest);
    else fprintf(o->md, "- %s\n", rest);
    char line[2400];
    if (lead && *lead) snprintf(line, sizeof line, "%.300s: %.2090s", lead, rest);
    else snprintf(line, sizeof line, "%.2390s", rest);
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
        if (*c == '|') *c = '/';
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

typedef struct {
    int stat;
    int place;
    int last;
    int of;
    double value;
    double margin;
} standing;

typedef struct {
    const char *dir;
    collection *c;
    ojh_game_results games[OJH_MAX_GAMES];
    int game_count;
    ojh_score scores[OJH_MAX_GAMES];
    int scored[OJH_MAX_GAMES];
    standing standings[OJH_MAX_GAMES][MAX_STATS];
    int standing_count[OJH_MAX_GAMES];
    int firsts[OJH_MAX_GAMES], lasts[OJH_MAX_GAMES];
    int graphs;
} report_ctx;

static void game_title(char *out, size_t n, const ojh_game_results *g) {
    if (*g->version) snprintf(out, n, "%.110s (version %.60s)", g->name, g->version);
    else snprintf(out, n, "%s", g->name);
}

static void compute_standings(report_ctx *x) {
    ojh_placing placings[OJH_MAX_GAMES];
    for (int i = 0; i < ojh_stat_count() && i < MAX_STATS; i++) {
        const ojh_stat *s = ojh_stat_at(i);
        if (s->better == OJH_NOT_RANKED) continue;
        int n = ojh_stat_rank(s, x->games, x->game_count, placings);
        if (n < 2) continue;
        int last_place = placings[n - 1].place;
        for (int k = 0; k < n; k++) {
            int g = placings[k].game;
            standing *st = &x->standings[g][x->standing_count[g]++];
            st->stat = i;
            st->place = placings[k].place;
            st->last = last_place;
            st->of = n;
            st->value = placings[k].value;
            double neighbour = k + 1 < n ? placings[k + 1].value : placings[k - 1].value;
            if (st->place == last_place && k > 0) neighbour = placings[k - 1].value;
            double a = st->value, b = neighbour;
            st->margin = (a > 0 && b > 0) ? (a > b ? a / b : b / a) : 1.0;
            if (st->place == 1) x->firsts[g]++;
            if (st->place == last_place && last_place > 1) x->lasts[g]++;
        }
    }
}

static const standing *pick(const report_ctx *x, int g, int best, int skip_stat) {
    const standing *chosen = NULL;
    double chosen_where = 0;
    for (int i = 0; i < x->standing_count[g]; i++) {
        const standing *s = &x->standings[g][i];
        if (s->stat == skip_stat) continue;
        double where = s->last > 1 ? (double)(s->place - 1) / (s->last - 1) : 0;
        int better = !chosen || (best ? where < chosen_where : where > chosen_where) ||
                     (where == chosen_where && s->margin > chosen->margin);
        if (better) {
            chosen = s;
            chosen_where = where;
        }
    }
    return chosen;
}

static void standing_text(char *out, size_t n, const standing *s) {
    const ojh_stat *stat = ojh_stat_at(s->stat);
    char value[80];
    stat_value_text(value, sizeof value, stat, s->value);
    snprintf(out, n, "%s: %s, %d%s of %d", stat->name, value, s->place, ordinal_suffix(s->place), s->of);
}

static void graph_path(char *out, size_t n, const report_ctx *x, const char *name) {
    snprintf(out, n, "%s/graphs/%s.svg", x->dir, name);
}

static void embed_graph(out_pair *o, const char *name, const char *alt) {
    fprintf(o->md, "![%s](graphs/%s.svg)\n\n", alt, name);
}

static void stat_graph(out_pair *o, const report_ctx *x, const ojh_stat *s, const int *rows, int row_count) {
    ojh_placing placings[OJH_MAX_GAMES];
    int n = ojh_stat_rank(s, x->games, x->game_count, placings);
    if (n == 0) return;
    ojh_bar bars[OJH_MAX_GAMES];
    int count = 0;
    for (int k = 0; k < n; k++) {
        const ojh_game_results *g = &x->games[placings[k].game];
        bars[count].label = g->name;
        bars[count].game_id = g->id;
        bars[count].value = placings[k].value;
        bars[count].missing = 0;
        count++;
    }
    for (int r = 0; r < row_count; r++) {
        int has = 0;
        for (int k = 0; k < n; k++) has |= placings[k].game == rows[r];
        if (has) continue;
        bars[count].label = x->games[rows[r]].name;
        bars[count].game_id = x->games[rows[r]].id;
        bars[count].value = 0;
        bars[count].missing = 1;
        count++;
    }
    char subtitle[300], path[4400];
    snprintf(subtitle, sizeof subtitle, "%s. Shown for: %s.",
             s->better == OJH_MORE_IS_BETTER ? "More is better, best first"
             : s->better == OJH_LESS_IS_BETTER ? "Less is better, best first" : "Not ranked, largest first",
             s->meaning);
    graph_path(path, sizeof path, x, s->id);
    if (x->graphs && ojh_chart_bars_svg(path, s->name, subtitle, s->unit, s->word, bars, count) == 0) {
        char alt[200];
        snprintf(alt, sizeof alt, "%s, %s", s->name, s->better == OJH_NOT_RANKED ? "largest first" : "best first");
        embed_graph(o, s->id, alt);
    }
    ojh_chart_bars_text(o->txt, s->name, s->unit, s->word, bars, count);
}

static void machine_section(out_pair *o, const ojh_jvalue *machine, int same_everywhere) {
    heading(o, 2, "Machine");
    table *t = calloc(1, sizeof *t);
    if (!t) return;
    t->columns = 2;
    int r = 0;
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
    char score[CELL];
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

static int score_before(const report_ctx *x, int a, int b) {
    if (x->scored[a] != x->scored[b]) return x->scored[a];
    if (x->scores[a].total != x->scores[b].total) return x->scores[a].total > x->scores[b].total;
    return strcmp(x->games[a].name, x->games[b].name) < 0;
}

static void games_by_score(const report_ctx *x, int *out) {
    for (int i = 0; i < x->game_count; i++) out[i] = i;
    for (int i = 1; i < x->game_count; i++) {
        int v = out[i], j = i - 1;
        while (j >= 0 && score_before(x, v, out[j])) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = v;
    }
}

static void glance_section(out_pair *o, report_ctx *x) {
    heading(o, 2, "At a glance");
    paragraph(o, "Games in order of their OJH score. Firsts and lasts count the statistics below on which a game "
                 "came best or worst of the games measured for it; a statistic only one game has is not counted. "
                 "Went best and went worst name the statistic where the game stood furthest ahead or behind.");
    table *t = calloc(1, sizeof *t);
    if (!t) return;
    const char *header[] = {"Game", "OJH score", "Firsts", "Lasts", "Went best", "Went worst"};
    t->columns = 6;
    for (int i = 0; i < t->columns; i++) set_cell(t, 0, i, header[i]);
    int order[OJH_MAX_GAMES];
    games_by_score(x, order);
    int rows = 1;
    ojh_bar bars[OJH_MAX_GAMES];
    int bar_count = 0;
    for (int k = 0; k < x->game_count && rows < MAX_ROWS; k++) {
        int g = order[k];
        char cell[CELL];
        game_title(cell, sizeof cell, &x->games[g]);
        set_cell(t, rows, 0, cell);
        if (x->scored[g]) {
            grouped(cell, sizeof cell, x->scores[g].total);
            if (x->scores[g].provisional) snprintf(cell + strlen(cell), sizeof cell - strlen(cell), " (provisional)");
        } else {
            snprintf(cell, sizeof cell, "n/a");
        }
        set_cell(t, rows, 1, cell);
        snprintf(cell, sizeof cell, "%d", x->firsts[g]);
        set_cell(t, rows, 2, cell);
        snprintf(cell, sizeof cell, "%d", x->lasts[g]);
        set_cell(t, rows, 3, cell);
        const standing *best = pick(x, g, 1, -1);
        const standing *worst = pick(x, g, 0, best ? best->stat : -1);
        if (best) standing_text(cell, sizeof cell, best);
        else snprintf(cell, sizeof cell, "n/a");
        set_cell(t, rows, 4, cell);
        if (worst && worst->place > 1) standing_text(cell, sizeof cell, worst);
        else snprintf(cell, sizeof cell, worst ? "no place below 1st" : "n/a");
        set_cell(t, rows, 5, cell);
        rows++;
        if (x->scored[g]) {
            bars[bar_count].label = x->games[g].name;
            bars[bar_count].game_id = x->games[g].id;
            bars[bar_count].value = x->scores[g].total;
            bars[bar_count].missing = 0;
            bar_count++;
        }
    }
    t->rows = rows;
    write_table(o, t);
    free(t);
    if (bar_count > 0) {
        char path[4400], subtitle[160];
        graph_path(path, sizeof path, x, "score");
        snprintf(subtitle, sizeof subtitle, "Score version %d. Each game's score is its own; more is better, best first.",
                 OJH_SCORE_VERSION);
        if (x->graphs && ojh_chart_bars_svg(path, "OJH score", subtitle, OJH_UNIT_NUMBER, "points", bars, bar_count) == 0) {
            embed_graph(o, "score", "OJH score, best first");
        }
        ojh_chart_bars_text(o->txt, "OJH score", OJH_UNIT_NUMBER, "points", bars, bar_count);
    }
}

static int extremes(const report_ctx *x, int g, int want_first, standing *out) {
    int n = 0;
    for (int i = 0; i < x->standing_count[g]; i++) {
        const standing *s = &x->standings[g][i];
        int wanted = want_first ? s->place == 1 : (s->place == s->last && s->last > 1);
        if (!wanted) continue;
        int slot = n < 3 ? n++ : -1;
        if (slot < 0) {
            int weakest = 0;
            for (int j = 1; j < 3; j++) {
                if (out[j].margin < out[weakest].margin) weakest = j;
            }
            if (out[weakest].margin >= s->margin) continue;
            slot = weakest;
        }
        out[slot] = *s;
    }
    for (int i = 1; i < n; i++) {
        standing v = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].margin < v.margin) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = v;
    }
    return n;
}

static void best_worst_section(out_pair *o, report_ctx *x) {
    heading(o, 2, "What went best and what went worst");
    heading(o, 3, "For every statistic");
    table *t = calloc(1, sizeof *t);
    if (!t) return;
    const char *header[] = {"Statistic", "Better when", "Best", "Worst", "Best vs worst"};
    t->columns = 5;
    for (int i = 0; i < t->columns; i++) set_cell(t, 0, i, header[i]);
    int rows = 1;
    ojh_placing placings[OJH_MAX_GAMES];
    for (int i = 0; i < ojh_stat_count() && rows < MAX_ROWS; i++) {
        const ojh_stat *s = ojh_stat_at(i);
        if (s->better == OJH_NOT_RANKED) continue;
        int n = ojh_stat_rank(s, x->games, x->game_count, placings);
        if (n < 2) continue;
        char cell[CELL], value[80];
        set_cell(t, rows, 0, s->name);
        set_cell(t, rows, 1, s->better == OJH_MORE_IS_BETTER ? "more" : "less");
        stat_value_text(value, sizeof value, s, placings[0].value);
        snprintf(cell, sizeof cell, "%s, %s", x->games[placings[0].game].name, value);
        set_cell(t, rows, 2, cell);
        stat_value_text(value, sizeof value, s, placings[n - 1].value);
        snprintf(cell, sizeof cell, "%s, %s", x->games[placings[n - 1].game].name, value);
        set_cell(t, rows, 3, cell);
        double a = placings[0].value, b = placings[n - 1].value;
        if (a == b) snprintf(cell, sizeof cell, "level");
        else if (a > 0 && b > 0) snprintf(cell, sizeof cell, "%.1fx %s", a > b ? a / b : b / a,
                                          s->better == OJH_MORE_IS_BETTER ? "more" : "less");
        else snprintf(cell, sizeof cell, "n/a");
        set_cell(t, rows, 4, cell);
        rows++;
    }
    t->rows = rows;
    if (rows == 1) paragraph(o, "Nothing to compare yet: every statistic here was measured for fewer than two games.");
    else write_table(o, t);
    free(t);

    heading(o, 3, "For every game");
    int order[OJH_MAX_GAMES];
    games_by_score(x, order);
    for (int k = 0; k < x->game_count; k++) {
        int g = order[k];
        char text[2200], item[300];
        size_t at = 0;
        standing best[3], worst[3];
        int nb = extremes(x, g, 1, best), nw = extremes(x, g, 0, worst);
        at += (size_t)snprintf(text + at, sizeof text - at, "went best at ");
        if (nb == 0) at += (size_t)snprintf(text + at, sizeof text - at, "nothing (no first places)");
        for (int i = 0; i < nb && at < sizeof text; i++) {
            standing_text(item, sizeof item, &best[i]);
            at += (size_t)snprintf(text + at, sizeof text - at, "%s%s", i ? "; " : "", item);
        }
        if (at < sizeof text) at += (size_t)snprintf(text + at, sizeof text - at, ". Went worst at ");
        if (nw == 0 && at < sizeof text) at += (size_t)snprintf(text + at, sizeof text - at, "nothing (no last places)");
        for (int i = 0; i < nw && at < sizeof text; i++) {
            standing_text(item, sizeof item, &worst[i]);
            at += (size_t)snprintf(text + at, sizeof text - at, "%s%s", i ? "; " : "", item);
        }
        if (at < sizeof text) snprintf(text + at, sizeof text - at, ".");
        char title[256];
        game_title(title, sizeof title, &x->games[g]);
        bullet(o, title, text);
    }
    end_list(o);
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

static int write_scorecard(const char *dir, const ojh_game_results *g, const ojh_score *s, char *base, size_t base_len) {
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
    const ojh_jvalue *any = NULL;
    for (int m = 0; m < OJH_METRIC_COUNT && !any; m++) any = g->result[m];
    const ojh_jvalue *tpm = g->result[OJH_METRIC_TPM];
    const ojh_jvalue *machine = ojh_jget(any, "machine");
    char title[256], text[1400], number[48], a[64], b[48];
    game_title(title, sizeof title, g);
    snprintf(text, sizeof text, "%s: OJH score", title);
    heading(&o, 1, text);

    grouped(number, sizeof number, s->total);
    fprintf(o.md, "**%s points**, OJH score version %d%s\n\n![OJH score %s](%s.svg)\n\n", number, OJH_SCORE_VERSION,
            s->provisional ? ", provisional" : "", number, base);
    snprintf(text, sizeof text, "%s points, OJH score version %d%s", number, OJH_SCORE_VERSION,
             s->provisional ? ", provisional" : "");
    wrap(o.txt, "", "", text);
    fputc('\n', o.txt);
    paragraph(&o, "This score is built from this game's own result files and nothing else. Every part is measured "
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
        const char *header[] = {"Part", "Measured by", "Weight", "Measured", "On the reference CPU", "Worth 1,000 points",
                                "Points"};
        t->columns = 7;
        for (int i = 0; i < t->columns; i++) set_cell(t, 0, i, header[i]);
        for (int i = 0; i < s->part_count; i++) {
            const ojh_score_part *p = &s->parts[i];
            char cell[CELL];
            set_cell(t, i + 1, 0, p->name);
            snprintf(cell, sizeof cell, "ojh %s", ojh_metric_id(p->metric));
            set_cell(t, i + 1, 1, cell);
            snprintf(cell, sizeof cell, "%.0f%%", p->weight * 100);
            set_cell(t, i + 1, 2, cell);
            if (p->present) ojh_format_value(cell, sizeof cell, p->measured, p->unit, p->word);
            else snprintf(cell, sizeof cell, "not measured");
            set_cell(t, i + 1, 3, cell);
            if (!p->present) snprintf(cell, sizeof cell, "n/a");
            else if (p->hardware_adjusted) ojh_format_value(cell, sizeof cell, p->value, p->unit, p->word);
            else if (p->capped) snprintf(cell, sizeof cell, "%.2f (capped)", p->value);
            else snprintf(cell, sizeof cell, "same");
            set_cell(t, i + 1, 4, cell);
            ojh_format_value(cell, sizeof cell, p->reference, p->unit, p->word);
            set_cell(t, i + 1, 5, cell);
            if (p->present) grouped(cell, sizeof cell, p->points);
            else snprintf(cell, sizeof cell, "n/a");
            set_cell(t, i + 1, 6, cell);
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

    heading(&o, 2, "The runs");
    const ojh_jvalue *r_any = ojh_jget(any, "result");
    const char *license = ojh_jstring(ojh_jget(r_any, "license"), "");
    const char *homepage = ojh_jstring(ojh_jget(r_any, "homepage"), "");
    if (*license || *homepage) {
        snprintf(text, sizeof text, "%s%s%s%s%s", title, *license ? ", " : "", license, *homepage ? ", " : "", homepage);
        bullet(&o, "Game", text);
    }
    for (int m = 0; m < OJH_METRIC_COUNT; m++) {
        if (!g->result[m]) continue;
        bullet(&o, ojh_metric_name((ojh_metric)m), ojh_jstring(ojh_jpath(g->result[m], "result.how"), "not recorded"));
    }
    if (tpm) {
        const ojh_jvalue *settings = ojh_jget(tpm, "settings");
        const ojh_jvalue *chosen = ojh_jget(settings, "players_chosen");
        if (ojh_jpresent(chosen) && chosen->number != 0) {
            snprintf(a, sizeof a, "%.0f players chosen by OJH", ojh_jnumber(ojh_jget(settings, "players_requested"), 0));
        } else {
            snprintf(a, sizeof a, "players set by the game");
        }
        snprintf(text, sizeof text, "%.0f turns asked for, %d timed, seed %.0f, %s",
                 ojh_jnumber(ojh_jget(settings, "turns_requested"), 0), s->turns,
                 ojh_jnumber(ojh_jget(settings, "seed"), 0), a);
        bullet(&o, "Turn speed settings", text);
    }
    snprintf(text, sizeof text, "%s, %s", ojh_jstring(ojh_jget(machine, "cpu"), "unknown CPU"),
             ojh_jstring(ojh_jget(machine, "os"), "unknown OS"));
    bullet(&o, "Machine", text);
    if (s->hardware_known) {
        grouped_or_na(a, sizeof a, ojh_jpath(machine, "reference.single_core_rounds_per_second"));
        grouped(b, sizeof b, 1000);
        snprintf(text, sizeof text, "%s rounds/s on one core; CPU-bound figures were put on OJH's reference CPU (%s "
                                    "rounds/s) with a factor of %.3f", a, b, s->hardware_factor);
        bullet(&o, "CPU reference score", text);
    }
    end_list(&o);

    heading(&o, 2, "How the score is built");
    bullet(&o, "Points", "each part scores 1000 × log2(1 + value ÷ reference level): the reference level is worth 1,000 "
                         "points, three times it 2,000 and seven times it 3,000, and nothing scores below zero. Where "
                         "less is better, the ratio is turned around.");
    bullet(&o, "Hardware", "turn speed, start-up, CPU time and turn delivery are put on OJH's reference CPU with the "
                           "machine's single-core reference score (src/machine.c), so a faster computer does not make a "
                           "faster game. A game that uses more cores keeps that advantage. Frame rate, memory and data "
                           "are used as measured.");
    snprintf(text, sizeof text, "the weighted mean of the parts the results have. Coverage is how much of the weight "
                                "that was, here %.0f%%; a part a game was not measured for is left out, never counted as "
                                "zero.", s->coverage * 100);
    bullet(&o, "Total", text);
    snprintf(text, sizeof text, "score version %d. Its parts, weights and reference levels are fixed in src/score.c; "
                                "any change makes a new version, and scores of different versions are not compared.",
             OJH_SCORE_VERSION);
    bullet(&o, "Version", text);
    end_list(&o);

    int ok = fclose(o.md) == 0;
    ok &= fclose(o.txt) == 0;
    ok &= write_badge(svg_path, s, title) == 0;
    return ok ? 0 : -1;
}

static void scores_section(out_pair *o, report_ctx *x) {
    heading(o, 2, "OJH scores");
    paragraph(o, "Each game's score is its own: it is built from that game's result files alone, against fixed "
                 "reference levels, so no game's score depends on which other games are in this report. Each "
                 "scorecard next to this report shows every part, and has a badge (.svg) to go with it.");
    table *t = calloc(1, sizeof *t);
    if (!t) return;
    const char *header[] = {"Game", "OJH score", "Coverage", "Status", "Scorecard"};
    t->columns = 5;
    for (int i = 0; i < t->columns; i++) set_cell(t, 0, i, header[i]);
    int rows = 1, provisional = 0;
    int order[OJH_MAX_GAMES];
    games_by_score(x, order);
    for (int k = 0; k < x->game_count && rows < MAX_ROWS; k++) {
        int g = order[k];
        if (!x->scored[g]) continue;
        const ojh_score *s = &x->scores[g];
        char base[128], cell[CELL];
        int written = write_scorecard(x->dir, &x->games[g], s, base, sizeof base) == 0;
        game_title(cell, sizeof cell, &x->games[g]);
        set_cell(t, rows, 0, cell);
        grouped(cell, sizeof cell, s->total);
        set_cell(t, rows, 1, cell);
        snprintf(cell, sizeof cell, "%.0f%%", s->coverage * 100);
        set_cell(t, rows, 2, cell);
        set_cell(t, rows, 3, s->provisional ? "provisional" : "final");
        snprintf(cell, sizeof cell, "%s.md", base);
        set_cell(t, rows, 4, written ? cell : "not written");
        provisional += s->provisional;
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

static const char *measure_command(ojh_metric m) {
    switch (m) {
        case OJH_METRIC_TPM: return "ojh tpm";
        case OJH_METRIC_FPS: return "ojh fps";
        case OJH_METRIC_NET: return "ojh net";
        default: return "ojh footprint";
    }
}

static const char *group_intro(const char *group) {
    if (strcmp(group, "Turn speed") == 0) {
        return "How fast each game plays its turns with every player run by its own AI. Start-up and world generation "
               "are timed separately and are not part of the turns. Player-turns and region-turns multiply turn speed "
               "by the size of the game, so a larger game is not punished for its size.";
    }
    if (strcmp(group, "CPU and memory") == 0) {
        return "What the game cost the machine while it played those turns, sampled ten times a second over the game "
               "and every process it started.";
    }
    if (strcmp(group, "Frame rate") == 0) {
        return "Frames per second in the same scenes in every game, with frame caps and vsync off where the game allows "
               "it. A scene a game does not have is n/a.";
    }
    if (strcmp(group, "Network") == 0) {
        return "The game's netcode, measured on this machine through OJH's counting relay, so the internet is not in "
               "the numbers: bytes per turn, information per minute and how quickly a finished turn reaches the "
               "clients.";
    }
    return "What a player downloads and keeps: install size, and the size and speed of a save.";
}

static void turn_series_graph(out_pair *o, const report_ctx *x, const int *rows, int row_count) {
    ojh_series series[OJH_MAX_GAMES];
    double *values[OJH_MAX_GAMES];
    int count = 0;
    for (int r = 0; r < row_count; r++) {
        const ojh_game_results *g = &x->games[rows[r]];
        const ojh_jvalue *list = ojh_jpath(g->result[OJH_METRIC_TPM], "result.turn_series_seconds");
        if (!list || list->type != OJH_JARRAY || list->count < 2) continue;
        values[count] = malloc(list->count * sizeof(double));
        if (!values[count]) continue;
        for (size_t i = 0; i < list->count; i++) values[count][i] = list->items[i].number;
        series[count].label = g->name;
        series[count].game_id = g->id;
        series[count].y = values[count];
        series[count].n = (int)list->count;
        count++;
    }
    if (count > 0) {
        char path[4400];
        graph_path(path, sizeof path, x, "turn-times");
        if (x->graphs && ojh_chart_lines_svg(path, "Turn time through the run",
                                             "Each run stretched from its first turn to its last, so runs of different "
                                             "lengths line up. Lower is faster.",
                                             "share of the run", OJH_UNIT_SECONDS, "seconds per turn", series, count, 1) == 0) {
            embed_graph(o, "turn-times", "Turn time through the run");
        }
    }
    for (int i = 0; i < count; i++) free(values[i]);
}

static void dpt_graph(out_pair *o, const report_ctx *x, const int *rows, int row_count) {
    ojh_range ranges[OJH_MAX_GAMES];
    int count = 0, any = 0;
    for (int r = 0; r < row_count; r++) {
        const ojh_game_results *g = &x->games[rows[r]];
        const ojh_jvalue *net = g->result[OJH_METRIC_NET];
        double low = ojh_jnumber(ojh_jpath(net, "result.dpt.lowest_bytes"), -1);
        double mid = ojh_jnumber(ojh_jpath(net, "result.dpt.median_bytes"), -1);
        double high = ojh_jnumber(ojh_jpath(net, "result.dpt.highest_bytes"), -1);
        ranges[count].label = g->name;
        ranges[count].game_id = g->id;
        ranges[count].low = low;
        ranges[count].middle = mid;
        ranges[count].high = high;
        ranges[count].missing = !(low >= 0 && mid >= 0 && high >= 0);
        any |= !ranges[count].missing;
        count++;
    }
    if (!any) return;
    char path[4400];
    graph_path(path, sizeof path, x, "data-per-turn");
    if (x->graphs && ojh_chart_ranges_svg(path, "Data per turn, lowest to highest",
                                          "Both ways, all clients. The dot is the median turn. Less is better.",
                                          OJH_UNIT_BYTES, NULL, "lowest", "median", "highest", ranges, count) == 0) {
        embed_graph(o, "data-per-turn", "Data per turn, lowest to highest");
    }
    ojh_chart_ranges_text(o->txt, "Data per turn: lowest .. [median] .. highest", OJH_UNIT_BYTES, NULL, ranges, count);
}

static void tpm_notes(out_pair *o, const report_ctx *x, const int *rows, int row_count) {
    heading(o, 3, "How each game was timed");
    int fewest = 0, most = 0, least_players = 0, most_players = 0, averages_only = 0, no_regions = 0, failed = 0;
    for (int k = 0; k < row_count; k++) {
        const ojh_game_results *g = &x->games[rows[k]];
        const ojh_jvalue *root = g->result[OJH_METRIC_TPM];
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
        bullet(o, g->name, line);

        int turns = (int)ojh_jnumber(ojh_jget(r, "turns"), 0);
        int players = (int)ojh_jnumber(ojh_jget(r, "players"), 0);
        if (turns > 0 && (fewest == 0 || turns < fewest)) fewest = turns;
        if (turns > most) most = turns;
        if (players > 0 && (least_players == 0 || players < least_players)) least_players = players;
        if (players > most_players) most_players = players;
        if (!ojh_jpresent(ojh_jget(r, "per_turn"))) averages_only++;
        if (!ojh_jpresent(ojh_jget(r, "regions"))) no_regions++;
        if (ojh_jpresent(ojh_jget(root, "error"))) failed++;
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
                 "Player counts differ (%d to %d), and so do map sizes. The player-turn and region-turn figures account "
                 "for size, not for rules or how much thinking each AI does.", least_players, most_players);
        bullet(o, "Game size", line);
    }
    if (averages_only) {
        bullet(o, "Spread", "A game that reports only an average turn time has n/a for its median, slow and slowest "
                            "turns and its late-game slowdown; its turn speed is still exact over the turns it timed.");
    }
    if (no_regions) {
        bullet(o, "Map size", "A game that does not report its map size has n/a for region-turns per minute.");
    }
    if (failed) {
        snprintf(line, sizeof line, "%d run(s) did not finish cleanly; their result file has the error.", failed);
        bullet(o, "Failed runs", line);
    }
    bullet(o, "Scope", "Every number comes from one machine under the fairness rules in README.md. None of it is a "
                       "claim about other hardware.");
    end_list(o);
}

static int stat_index(const ojh_stat *s) {
    for (int i = 0; i < ojh_stat_count(); i++) {
        if (ojh_stat_at(i) == s) return i;
    }
    return -1;
}

static void group_section(out_pair *o, report_ctx *x, const char *group) {
    heading(o, 2, group);
    paragraph(o, group_intro(group));

    const ojh_stat *present[MAX_STATS];
    int stat_count = 0;
    ojh_metric metric = OJH_METRIC_TPM;
    for (int i = 0; i < ojh_stat_count(); i++) {
        const ojh_stat *s = ojh_stat_at(i);
        if (strcmp(s->group, group) != 0) continue;
        metric = s->metric;
        double v;
        int any = 0;
        for (int g = 0; g < x->game_count && !any; g++) any = ojh_stat_value(s, &x->games[g], &v);
        if (any && stat_count < MAX_STATS) present[stat_count++] = s;
    }

    int rows[OJH_MAX_GAMES], row_count = 0;
    const ojh_stat *lead = NULL;
    for (int i = 0; i < stat_count && !lead; i++) {
        if (present[i]->better != OJH_NOT_RANKED) lead = present[i];
    }
    if (lead) {
        ojh_placing placings[OJH_MAX_GAMES];
        int n = ojh_stat_rank(lead, x->games, x->game_count, placings);
        for (int k = 0; k < n; k++) rows[row_count++] = placings[k].game;
    }
    for (int g = 0; g < x->game_count; g++) {
        if (!x->games[g].result[metric]) continue;
        int listed = 0;
        for (int r = 0; r < row_count; r++) listed |= rows[r] == g;
        if (!listed) rows[row_count++] = g;
    }

    if (row_count == 0 || stat_count == 0) {
        char text[400];
        snprintf(text, sizeof text, "Not measured for any game yet. Run `%s <game>` for each game, then build the "
                                    "report again.", measure_command(metric));
        paragraph(o, text);
        return;
    }

    int per_table = MAX_COLUMNS - 1;
    for (int from = 0; from < stat_count; from += per_table) {
        int to = from + per_table < stat_count ? from + per_table : stat_count;
        table *t = calloc(1, sizeof *t);
        if (!t) return;
        t->columns = 1 + (to - from);
        set_cell(t, 0, 0, "Game");
        for (int i = from; i < to; i++) set_cell(t, 0, 1 + i - from, present[i]->name);
        int r = 0;
        for (; r < row_count && r + 1 < MAX_ROWS; r++) {
            int g = rows[r];
            set_cell(t, r + 1, 0, x->games[g].name);
            for (int i = from; i < to; i++) {
                const ojh_stat *s = present[i];
                char cell[CELL], value[96];
                double v;
                if (!ojh_stat_value(s, &x->games[g], &v)) {
                    set_cell(t, r + 1, 1 + i - from, "n/a");
                    continue;
                }
                stat_value_text(value, sizeof value, s, v);
                snprintf(cell, sizeof cell, "%s", value);
                int index = stat_index(s);
                for (int k = 0; k < x->standing_count[g]; k++) {
                    const standing *st = &x->standings[g][k];
                    if (st->stat == index) {
                        snprintf(cell, sizeof cell, "%s (%d%s)", value, st->place, ordinal_suffix(st->place));
                    }
                }
                set_cell(t, r + 1, 1 + i - from, cell);
            }
        }
        t->rows = r + 1;
        write_table(o, t);
        free(t);
    }
    paragraph(o, "Places are in brackets: 1st is the best of the games measured for that statistic. A statistic that "
                 "is not ranked (players, map size, cores in use, information per minute) is context: more of it is "
                 "neither better nor worse on its own.");

    heading(o, 3, "Graphs");
    for (int i = 0; i < stat_count; i++) stat_graph(o, x, present[i], rows, row_count);
    if (strcmp(group, "Turn speed") == 0) turn_series_graph(o, x, rows, row_count);
    if (strcmp(group, "Network") == 0) dpt_graph(o, x, rows, row_count);

    heading(o, 3, "What each statistic means");
    for (int i = 0; i < stat_count; i++) {
        char text[400];
        snprintf(text, sizeof text, "%s.%s", present[i]->meaning,
                 present[i]->better == OJH_MORE_IS_BETTER ? " More is better."
                 : present[i]->better == OJH_LESS_IS_BETTER ? " Less is better." : " Not ranked.");
        bullet(o, present[i]->name, text);
    }
    end_list(o);

    if (strcmp(group, "Turn speed") == 0) {
        tpm_notes(o, x, rows, row_count);
    } else if (metric != OJH_METRIC_TPM) {
        heading(o, 3, "How each game was measured");
        for (int r = 0; r < row_count; r++) {
            const ojh_game_results *g = &x->games[rows[r]];
            bullet(o, g->name, ojh_jstring(ojh_jpath(g->result[metric], "result.how"), "not recorded"));
        }
        end_list(o);
    }
}

static void not_measured_section(out_pair *o, const report_ctx *x) {
    int missing = 0;
    for (int g = 0; g < x->game_count; g++) {
        for (int m = 0; m < OJH_METRIC_COUNT; m++) missing += x->games[g].result[m] == NULL;
    }
    if (!missing) return;
    heading(o, 2, "Not measured yet");
    paragraph(o, "These measurements have no result file in this folder, so their statistics are n/a above and their "
                 "score parts are left out.");
    for (int g = 0; g < x->game_count; g++) {
        char text[600];
        size_t at = 0;
        int any = 0;
        for (int m = 0; m < OJH_METRIC_COUNT && at < sizeof text; m++) {
            if (x->games[g].result[m]) continue;
            at += (size_t)snprintf(text + at, sizeof text - at, "%s%s (`%s %s`)", any ? ", " : "",
                                   ojh_metric_name((ojh_metric)m), measure_command((ojh_metric)m), x->games[g].id);
            any = 1;
        }
        if (any) bullet(o, x->games[g].name, text);
    }
    end_list(o);
}

int ojh_scorecard_write(const char *const *paths, int count, const char *dir, char *written, size_t written_len,
                        char *error, size_t error_len) {
    if (count <= 0 || count > MAX_RESULTS) {
        if (error && error_len) snprintf(error, error_len, "give between 1 and %d result files", MAX_RESULTS);
        return -1;
    }
    ojh_jvalue *roots[MAX_RESULTS];
    int loaded = 0, status = -1;
    for (int i = 0; i < count; i++) {
        char why[256];
        roots[loaded] = ojh_jparse_file(paths[i], why, sizeof why);
        if (!roots[loaded]) {
            if (error && error_len) snprintf(error, error_len, "cannot read %s: %s", paths[i], why);
            for (int k = 0; k < loaded; k++) ojh_jfree(roots[k]);
            return -1;
        }
        loaded++;
    }
    ojh_game_results games[2];
    int n = ojh_group_results((const ojh_jvalue *const *)roots, paths, loaded, games, 2);
    ojh_score s;
    if (n != 1) {
        if (error && error_len) {
            snprintf(error, error_len, "%s", n == 0 ? "no OJH results to score" : "these files measure more than one game");
        }
    } else if (ojh_score_game(&games[0], &s) != 0) {
        if (error && error_len) snprintf(error, error_len, "nothing in these files can be scored");
    } else if (write_scorecard(dir, &games[0], &s, written, written_len) != 0) {
        if (error && error_len) snprintf(error, error_len, "cannot write the scorecard in %s", dir);
    } else {
        status = 0;
    }
    for (int i = 0; i < loaded; i++) ojh_jfree(roots[i]);
    return status;
}

int ojh_report_write(const char *dir, char *error, size_t error_len) {
    collection *c = calloc(1, sizeof *c);
    report_ctx *x = calloc(1, sizeof *x);
    if (!c || !x) {
        free(c);
        free(x);
        return -1;
    }
    c->dir = dir;
    if (ojh_list_dir(dir, collect, c) != 0) {
        if (error && error_len) snprintf(error, error_len, "cannot read the folder %s", dir);
        free(c);
        free(x);
        return -1;
    }
    if (c->count == 0) {
        if (error && error_len) {
            snprintf(error, error_len, "no OJH result files in %s%s%s", dir, c->unreadable[0] ? "; unreadable: " : "",
                     c->unreadable);
        }
        free(c);
        free(x);
        return -1;
    }
    qsort(c->items, (size_t)c->count, sizeof c->items[0], by_file_name);
    const ojh_jvalue *roots[MAX_RESULTS];
    const char *names[MAX_RESULTS];
    for (int i = 0; i < c->count; i++) {
        roots[i] = c->items[i].root;
        names[i] = c->items[i].name;
    }
    x->dir = dir;
    x->c = c;
    x->game_count = ojh_group_results(roots, names, c->count, x->games, OJH_MAX_GAMES);
    for (int g = 0; g < x->game_count; g++) x->scored[g] = ojh_score_game(&x->games[g], &x->scores[g]) == 0;
    compute_standings(x);
    char graphs[4400];
    snprintf(graphs, sizeof graphs, "%s/graphs", dir);
    ojh_make_dir(graphs);
    x->graphs = ojh_list_dir(graphs, ignore_name, NULL) == 0;

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
        free(x);
        return -1;
    }

    heading(&o, 1, "Objective Judge Horizon (OJH) report");
    char when[64] = "";
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (local) strftime(when, sizeof when, "%Y-%m-%d %H:%M", local);
    int measured[OJH_METRIC_COUNT] = {0};
    int replaced = 0;
    for (int g = 0; g < x->game_count; g++) {
        for (int m = 0; m < OJH_METRIC_COUNT; m++) measured[m] += x->games[g].result[m] != NULL;
        replaced += x->games[g].replaced;
    }
    char intro[1200];
    snprintf(intro, sizeof intro,
             "Written by OJH %s on %s from %d result file(s) covering %d game(s). Measured: turn speed for %d, frame "
             "rate for %d, network for %d and footprint for %d.", OJH_REPORT_VERSION, when, c->count, x->game_count,
             measured[OJH_METRIC_TPM], measured[OJH_METRIC_FPS], measured[OJH_METRIC_NET], measured[OJH_METRIC_FOOTPRINT]);
    paragraph(&o, intro);
    if (c->skipped) {
        snprintf(intro, sizeof intro, "%d file(s) were skipped%s%s.", c->skipped, c->unreadable[0] ? ", first: " : "",
                 c->unreadable);
        paragraph(&o, intro);
    }
    if (replaced) {
        snprintf(intro, sizeof intro, "%d result file(s) were set aside because a later file (by name) measured the "
                                      "same thing for the same game.", replaced);
        paragraph(&o, intro);
    }

    glance_section(&o, x);
    best_worst_section(&o, x);

    const ojh_jvalue *machine = ojh_jget(c->items[0].root, "machine");
    int same = 1;
    for (int i = 1; i < c->count; i++) same &= same_machine(machine, ojh_jget(c->items[i].root, "machine"));
    machine_section(&o, machine, same);
    scores_section(&o, x);

    const char *groups[16];
    int group_count = 0;
    for (int i = 0; i < ojh_stat_count(); i++) {
        const char *group = ojh_stat_at(i)->group;
        int seen = 0;
        for (int k = 0; k < group_count; k++) seen |= strcmp(groups[k], group) == 0;
        if (!seen && group_count < 16) groups[group_count++] = group;
    }
    for (int k = 0; k < group_count; k++) group_section(&o, x, groups[k]);
    not_measured_section(&o, x);

    fclose(o.md);
    fclose(o.txt);
    for (int i = 0; i < c->count; i++) ojh_jfree(c->items[i].root);
    free(c);
    free(x);
    return 0;
}
