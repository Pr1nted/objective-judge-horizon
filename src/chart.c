#include "chart.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Okabe-Ito, readable for the common kinds of colour blindness, then two extras. */
static const char *const PALETTE[] = {"#0072B2", "#E69F00", "#009E73", "#CC79A7", "#56B4E9",
                                      "#D55E00", "#8C6D1F", "#5B5B8F", "#7A9A01", "#B03A48"};
#define PALETTE_SIZE (sizeof PALETTE / sizeof PALETTE[0])

#define INK "#1F2328"
#define MUTED "#57606A"
#define GRID "#D8DEE4"
#define PAPER "#FFFFFF"
#define FONT "font-family=\"Helvetica Neue,Helvetica,Arial,DejaVu Sans,sans-serif\""

const char *ojh_game_colour(const char *game_id) {
    /* FNV-1a: stable across runs, machines and the order games appear in. */
    unsigned long h = 2166136261ul;
    for (const char *c = game_id ? game_id : ""; *c; c++) h = (h ^ (unsigned char)*c) * 16777619ul;
    return PALETTE[h % PALETTE_SIZE];
}

/* ---------------------------------------------------------------- numbers */

static void grouped_int(char *out, size_t n, double value) {
    long long x = (long long)(value < 0 ? value - 0.5 : value + 0.5);
    char digits[32], buf[48];
    snprintf(digits, sizeof digits, "%lld", x < 0 ? -x : x);
    size_t len = strlen(digits), at = 0;
    if (x < 0) buf[at++] = '-';
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) buf[at++] = ',';
        buf[at++] = digits[i];
    }
    buf[at] = '\0';
    snprintf(out, n, "%s", buf);
}

void ojh_format_value(char *out, size_t n, double v, ojh_unit unit, const char *word) {
    char number[64];
    const char *suffix = "";
    switch (unit) {
        case OJH_UNIT_BYTES: {
            static const char *const names[] = {"B", "KiB", "MiB", "GiB", "TiB"};
            int i = 0;
            double x = v;
            while (fabs(x) >= 1024 && i < 4) {
                x /= 1024;
                i++;
            }
            if (i == 0) snprintf(out, n, "%.0f B", x);
            else snprintf(out, n, "%.*f %s", fabs(x) < 10 ? 2 : fabs(x) < 100 ? 1 : 0, x, names[i]);
            if (word && *word) {
                size_t len = strlen(out);
                snprintf(out + len, n > len ? n - len : 0, "%s", word);
            }
            return;
        }
        case OJH_UNIT_SECONDS:
            if (fabs(v) >= 120) snprintf(out, n, "%.0f min %.0f s", floor(v / 60), fmod(v, 60));
            else snprintf(out, n, "%.*f s", fabs(v) < 0.1 ? 4 : fabs(v) < 10 ? 3 : 1, v);
            return;
        case OJH_UNIT_RATIO:
            snprintf(out, n, "%.2f%s", v, word ? word : ""); /* "7.08x" */
            return;
        case OJH_UNIT_PERCENT:
            snprintf(out, n, "%.0f%%", v);
            return;
        default:
            break;
    }
    if (fabs(v) >= 1000) {
        grouped_int(number, sizeof number, v);
    } else if (fabs(v) >= 100 || v == floor(v)) {
        snprintf(number, sizeof number, "%.*f", v == floor(v) ? 0 : 1, v);
    } else {
        snprintf(number, sizeof number, "%.*f", fabs(v) < 1 ? 3 : 2, v);
    }
    if (word && *word) suffix = word;
    /* joined by hand: the pieces are bounded, and snprintf of unknown lengths trips GCC's truncation check */
    size_t at = 0;
    const char *pieces[3] = {number, *suffix ? " " : "", suffix};
    for (int i = 0; i < 3 && n > 0; i++) {
        for (const char *c = pieces[i]; *c && at + 1 < n; c++) out[at++] = *c;
    }
    if (n > 0) out[at] = '\0';
}

/* ---------------------------------------------------------------- SVG pieces */

static void xml(FILE *f, const char *s) {
    for (; s && *s; s++) {
        if (*s == '&') fputs("&amp;", f);
        else if (*s == '<') fputs("&lt;", f);
        else if (*s == '>') fputs("&gt;", f);
        else if (*s == '"') fputs("&quot;", f);
        else fputc(*s, f);
    }
}

/* Roughly how wide text is at a font size, for laying out labels without a font engine. */
static double text_width(const char *s, double size) {
    double w = 0;
    for (; s && *s; s++) {
        if (((unsigned char)*s & 0xC0) == 0x80) continue;
        w += (*s == ' ' || *s == '.' || *s == ',') ? 0.3 : (*s >= 'A' && *s <= 'Z') ? 0.66 : 0.55;
    }
    return w * size;
}

static FILE *begin(const char *path, int width, int height, const char *title, const char *subtitle) {
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\" width=\"%d\" height=\"%d\" role=\"img\" aria-label=\"",
            width, height, width, height);
    xml(f, title);
    fprintf(f, "\">\n<title>");
    xml(f, title);
    fprintf(f, "</title>\n<rect width=\"%d\" height=\"%d\" fill=\"" PAPER "\"/>\n", width, height);
    fprintf(f, "<text x=\"24\" y=\"34\" " FONT " font-size=\"18\" font-weight=\"600\" fill=\"" INK "\">");
    xml(f, title);
    fputs("</text>\n", f);
    if (subtitle && *subtitle) {
        fputs("<text x=\"24\" y=\"56\" " FONT " font-size=\"12.5\" fill=\"" MUTED "\">", f);
        xml(f, subtitle);
        fputs("</text>\n", f);
    }
    return f;
}

static int finish(FILE *f) {
    fputs("</svg>\n", f);
    return fclose(f) == 0 ? 0 : -1;
}

/* A round step so an axis of span has about `ticks` divisions. */
static double nice_step(double span, int ticks) {
    if (!(span > 0)) return 1;
    double raw = span / ticks;
    double magnitude = pow(10, floor(log10(raw)));
    double r = raw / magnitude;
    double step = r < 1.5 ? 1 : r < 3 ? 2 : r < 7 ? 5 : 10;
    return step * magnitude;
}

/* ---------------------------------------------------------------- bars */

int ojh_chart_bars_svg(const char *path, const char *title, const char *subtitle, ojh_unit unit, const char *word,
                       const ojh_bar *bars, int count) {
    const int row = 34, top = 76, bottom = 40, right = 120;
    double label_w = 0;
    double max = 0;
    for (int i = 0; i < count; i++) {
        double w = text_width(bars[i].label, 13);
        if (w > label_w) label_w = w;
        if (!bars[i].missing && bars[i].value > max) max = bars[i].value;
    }
    int left = 24 + (int)label_w + 16;
    int width = 760, plot = width - left - right;
    if (plot < 240) {
        width = left + right + 240;
        plot = 240;
    }
    int height = top + row * (count > 0 ? count : 1) + bottom;
    FILE *f = begin(path, width, height, title, subtitle);
    if (!f) return -1;

    double step = nice_step(max, 4), axis = max > 0 ? ceil(max / step) * step : 1;
    for (double t = 0; t <= axis + step * 0.001; t += step) {
        double x = left + plot * t / axis;
        char label[64];
        ojh_format_value(label, sizeof label, t, unit, NULL);
        fprintf(f, "<line x1=\"%.1f\" y1=\"%d\" x2=\"%.1f\" y2=\"%d\" stroke=\"" GRID "\" stroke-width=\"1\"/>\n", x,
                top - 6, x, height - bottom + 4);
        fprintf(f, "<text x=\"%.1f\" y=\"%d\" " FONT " font-size=\"11\" fill=\"" MUTED "\" text-anchor=\"middle\">", x,
                height - bottom + 20);
        xml(f, label);
        fputs("</text>\n", f);
    }
    for (int i = 0; i < count; i++) {
        int y = top + row * i;
        fprintf(f, "<text x=\"%d\" y=\"%d\" " FONT " font-size=\"13\" fill=\"" INK "\" text-anchor=\"end\">", left - 12,
                y + 21);
        xml(f, bars[i].label);
        fputs("</text>\n", f);
        char value[96];
        if (bars[i].missing) {
            fprintf(f, "<text x=\"%d\" y=\"%d\" " FONT " font-size=\"12\" font-style=\"italic\" fill=\"" MUTED "\">n/a</text>\n",
                    left + 6, y + 21);
            continue;
        }
        double w = axis > 0 ? plot * bars[i].value / axis : 0;
        if (w < 1.5 && bars[i].value > 0) w = 1.5;
        fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%.1f\" height=\"22\" rx=\"3\" fill=\"%s\"/>\n", left, y + 5, w,
                ojh_game_colour(bars[i].game_id));
        ojh_format_value(value, sizeof value, bars[i].value, unit, word);
        fprintf(f, "<text x=\"%.1f\" y=\"%d\" " FONT " font-size=\"12\" fill=\"" INK "\">", left + w + 8, y + 21);
        xml(f, value);
        fputs("</text>\n", f);
    }
    return finish(f);
}

void ojh_chart_bars_text(FILE *f, const char *title, ojh_unit unit, const char *word, const ojh_bar *bars, int count) {
    const int width = 36;
    size_t label_w = 0;
    double max = 0;
    for (int i = 0; i < count; i++) {
        size_t w = 0;
        for (const char *c = bars[i].label; *c; c++) w += ((unsigned char)*c & 0xC0) != 0x80;
        if (w > label_w) label_w = w;
        if (!bars[i].missing && bars[i].value > max) max = bars[i].value;
    }
    fprintf(f, "%s\n", title);
    for (int i = 0; i < count; i++) {
        size_t w = 0;
        for (const char *c = bars[i].label; *c; c++) w += ((unsigned char)*c & 0xC0) != 0x80;
        fprintf(f, "  %s", bars[i].label);
        for (size_t k = w; k < label_w + 2; k++) fputc(' ', f);
        if (bars[i].missing) {
            fputs("n/a\n", f);
            continue;
        }
        int cells = max > 0 ? (int)(width * bars[i].value / max + 0.5) : 0;
        if (cells == 0 && bars[i].value > 0) cells = 1;
        for (int k = 0; k < cells; k++) fputc('#', f);
        for (int k = cells; k < width; k++) fputc(' ', f);
        char value[96];
        ojh_format_value(value, sizeof value, bars[i].value, unit, word);
        fprintf(f, "  %s\n", value);
    }
    fputc('\n', f);
}

/* ---------------------------------------------------------------- ranges */

int ojh_chart_ranges_svg(const char *path, const char *title, const char *subtitle, ojh_unit unit, const char *word,
                         const char *low_name, const char *middle_name, const char *high_name,
                         const ojh_range *ranges, int count) {
    const int row = 38, top = 96, bottom = 44, right = 40;
    double label_w = 0, lo = 0, hi = 0;
    int any = 0;
    for (int i = 0; i < count; i++) {
        double w = text_width(ranges[i].label, 13);
        if (w > label_w) label_w = w;
        if (ranges[i].missing) continue;
        if (!any || ranges[i].low < lo) lo = ranges[i].low;
        if (!any || ranges[i].high > hi) hi = ranges[i].high;
        any = 1;
    }
    int left = 24 + (int)label_w + 16, width = 760, plot = width - left - right;
    int height = top + row * (count > 0 ? count : 1) + bottom;
    int log_scale = any && lo > 0 && hi / lo > 100;
    double a = log_scale ? floor(log10(lo)) : 0;
    double b = log_scale ? ceil(log10(hi)) : 0;
    double step = 1, axis = 1;
    if (!log_scale) {
        step = nice_step(hi, 4);
        axis = hi > 0 ? ceil(hi / step) * step : 1;
    }
    FILE *f = begin(path, width, height, title, subtitle);
    if (!f) return -1;
    #define XPOS(v) (log_scale ? left + plot * ((log10((v) > 0 ? (v) : pow(10, a)) - a) / (b - a > 0 ? b - a : 1)) \
                               : left + plot * (v) / axis)

    /* legend */
    fprintf(f, "<g " FONT " font-size=\"11.5\" fill=\"" MUTED "\">"
               "<line x1=\"24\" y1=\"74\" x2=\"52\" y2=\"74\" stroke=\"" MUTED "\" stroke-width=\"4\" stroke-linecap=\"round\"/>"
               "<text x=\"58\" y=\"78\">");
    xml(f, low_name);
    fputs(" to ", f);
    xml(f, high_name);
    fprintf(f, "</text><circle cx=\"%.1f\" cy=\"74\" r=\"5\" fill=\"" PAPER "\" stroke=\"" INK "\" stroke-width=\"2\"/>"
               "<text x=\"%.1f\" y=\"78\">", 70 + text_width(low_name, 11.5) + text_width(high_name, 11.5) + 40,
            80 + text_width(low_name, 11.5) + text_width(high_name, 11.5) + 40);
    xml(f, middle_name);
    fputs("</text></g>\n", f);

    if (log_scale) {
        for (double e = a; e <= b + 1e-9; e += 1) {
            double x = XPOS(pow(10, e));
            char label[64];
            ojh_format_value(label, sizeof label, pow(10, e), unit, NULL);
            fprintf(f, "<line x1=\"%.1f\" y1=\"%d\" x2=\"%.1f\" y2=\"%d\" stroke=\"" GRID "\"/>\n", x, top - 6, x,
                    height - bottom + 4);
            fprintf(f, "<text x=\"%.1f\" y=\"%d\" " FONT " font-size=\"11\" fill=\"" MUTED "\" text-anchor=\"middle\">", x,
                    height - bottom + 20);
            xml(f, label);
            fputs("</text>\n", f);
        }
        fprintf(f, "<text x=\"%d\" y=\"%d\" " FONT " font-size=\"11\" fill=\"" MUTED "\" text-anchor=\"end\">log scale</text>\n",
                width - right, height - 8);
    } else {
        for (double t = 0; t <= axis + step * 0.001; t += step) {
            double x = XPOS(t);
            char label[64];
            ojh_format_value(label, sizeof label, t, unit, NULL);
            fprintf(f, "<line x1=\"%.1f\" y1=\"%d\" x2=\"%.1f\" y2=\"%d\" stroke=\"" GRID "\"/>\n", x, top - 6, x,
                    height - bottom + 4);
            fprintf(f, "<text x=\"%.1f\" y=\"%d\" " FONT " font-size=\"11\" fill=\"" MUTED "\" text-anchor=\"middle\">", x,
                    height - bottom + 20);
            xml(f, label);
            fputs("</text>\n", f);
        }
    }
    for (int i = 0; i < count; i++) {
        int y = top + row * i + 19;
        fprintf(f, "<text x=\"%d\" y=\"%d\" " FONT " font-size=\"13\" fill=\"" INK "\" text-anchor=\"end\">", left - 12, y + 4);
        xml(f, ranges[i].label);
        fputs("</text>\n", f);
        if (ranges[i].missing) {
            fprintf(f, "<text x=\"%d\" y=\"%d\" " FONT " font-size=\"12\" font-style=\"italic\" fill=\"" MUTED "\">n/a</text>\n",
                    left + 6, y + 4);
            continue;
        }
        const char *colour = ojh_game_colour(ranges[i].game_id);
        double x0 = XPOS(ranges[i].low), x1 = XPOS(ranges[i].high), xm = XPOS(ranges[i].middle);
        if (x1 - x0 < 2) x1 = x0 + 2;
        fprintf(f, "<line x1=\"%.1f\" y1=\"%d\" x2=\"%.1f\" y2=\"%d\" stroke=\"%s\" stroke-width=\"8\" stroke-linecap=\"round\"/>\n",
                x0, y, x1, y, colour);
        fprintf(f, "<circle cx=\"%.1f\" cy=\"%d\" r=\"6\" fill=\"" PAPER "\" stroke=\"" INK "\" stroke-width=\"2\"/>\n", xm, y);
        char low[64], high[64];
        ojh_format_value(low, sizeof low, ranges[i].low, unit, word);
        ojh_format_value(high, sizeof high, ranges[i].high, unit, word);
        fprintf(f, "<text x=\"%.1f\" y=\"%d\" " FONT " font-size=\"10.5\" fill=\"" MUTED "\" text-anchor=\"middle\">", x0, y - 9);
        xml(f, low);
        fprintf(f, "</text>\n<text x=\"%.1f\" y=\"%d\" " FONT " font-size=\"10.5\" fill=\"" MUTED "\" text-anchor=\"middle\">",
                x1, y + 20);
        xml(f, high);
        fputs("</text>\n", f);
    }
    #undef XPOS
    return finish(f);
}

void ojh_chart_ranges_text(FILE *f, const char *title, ojh_unit unit, const char *word, const ojh_range *ranges,
                           int count) {
    fprintf(f, "%s\n", title);
    for (int i = 0; i < count; i++) {
        if (ranges[i].missing) {
            fprintf(f, "  %s: n/a\n", ranges[i].label);
            continue;
        }
        char low[64], mid[64], high[64];
        ojh_format_value(low, sizeof low, ranges[i].low, unit, word);
        ojh_format_value(mid, sizeof mid, ranges[i].middle, unit, word);
        ojh_format_value(high, sizeof high, ranges[i].high, unit, word);
        fprintf(f, "  %s: %s .. [%s] .. %s\n", ranges[i].label, low, mid, high);
    }
    fputc('\n', f);
}

/* ---------------------------------------------------------------- lines */

int ojh_chart_lines_svg(const char *path, const char *title, const char *subtitle, const char *x_name,
                        ojh_unit unit, const char *word, const ojh_series *series, int count, int x_as_share) {
    const int left = 84, right = 24, top = 76, bottom = 74, width = 760, height = 420;
    int plot_w = width - left - right, plot_h = height - top - bottom;
    double max_y = 0;
    int max_n = 0;
    for (int s = 0; s < count; s++) {
        if (series[s].n > max_n) max_n = series[s].n;
        for (int i = 0; i < series[s].n; i++) {
            if (series[s].y[i] > max_y) max_y = series[s].y[i];
        }
    }
    FILE *f = begin(path, width, height, title, subtitle);
    if (!f) return -1;
    double step = nice_step(max_y, 5), axis = max_y > 0 ? ceil(max_y / step) * step : 1;
    for (double t = 0; t <= axis + step * 0.001; t += step) {
        double y = top + plot_h - plot_h * t / axis;
        char label[64];
        ojh_format_value(label, sizeof label, t, unit, NULL);
        fprintf(f, "<line x1=\"%d\" y1=\"%.1f\" x2=\"%d\" y2=\"%.1f\" stroke=\"" GRID "\"/>\n", left, y, width - right, y);
        fprintf(f, "<text x=\"%d\" y=\"%.1f\" " FONT " font-size=\"11\" fill=\"" MUTED "\" text-anchor=\"end\">", left - 8,
                y + 4);
        xml(f, label);
        fputs("</text>\n", f);
    }
    const char *const shares[] = {"start", "25%", "50%", "75%", "end"};
    for (int k = 0; k <= 4; k++) {
        double x = left + plot_w * k / 4.0;
        char label[32];
        if (x_as_share) snprintf(label, sizeof label, "%s", shares[k]);
        else snprintf(label, sizeof label, "%d", max_n > 1 ? 1 + (int)((max_n - 1) * k / 4.0 + 0.5) : 1);
        fprintf(f, "<text x=\"%.1f\" y=\"%d\" " FONT " font-size=\"11\" fill=\"" MUTED "\" text-anchor=\"middle\">", x,
                top + plot_h + 18);
        xml(f, label);
        fputs("</text>\n", f);
    }
    fprintf(f, "<text x=\"%d\" y=\"%d\" " FONT " font-size=\"11.5\" fill=\"" MUTED "\" text-anchor=\"middle\">", left + plot_w / 2,
            top + plot_h + 36);
    xml(f, x_name);
    fputs("</text>\n", f);
    if (word && *word) {
        fprintf(f, "<text x=\"%d\" y=\"%d\" " FONT " font-size=\"11.5\" fill=\"" MUTED "\">", 24, top - 10);
        xml(f, word);
        fputs("</text>\n", f);
    }
    for (int s = 0; s < count; s++) {
        if (series[s].n < 1) continue;
        const char *colour = ojh_game_colour(series[s].game_id);
        fprintf(f, "<polyline fill=\"none\" stroke=\"%s\" stroke-width=\"2\" stroke-linejoin=\"round\" points=\"", colour);
        int span = x_as_share ? series[s].n : max_n;
        for (int i = 0; i < series[s].n; i++) {
            double x = left + (span > 1 ? plot_w * (double)i / (span - 1) : plot_w / 2.0);
            double y = top + plot_h - plot_h * (series[s].y[i] / axis);
            fprintf(f, "%s%.1f,%.1f", i ? " " : "", x, y);
        }
        fputs("\"/>\n", f);
    }
    /* legend, one row under the axis title */
    double lx = left;
    for (int s = 0; s < count; s++) {
        const char *colour = ojh_game_colour(series[s].game_id);
        fprintf(f, "<rect x=\"%.1f\" y=\"%d\" width=\"14\" height=\"4\" fill=\"%s\"/>", lx, height - 20, colour);
        fprintf(f, "<text x=\"%.1f\" y=\"%d\" " FONT " font-size=\"11.5\" fill=\"" INK "\">", lx + 19, height - 15);
        xml(f, series[s].label);
        fputs("</text>\n", f);
        lx += 19 + text_width(series[s].label, 11.5) + 22;
    }
    return finish(f);
}
