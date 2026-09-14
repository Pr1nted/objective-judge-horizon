#ifndef OJH_CHART_H
#define OJH_CHART_H

#include <stdio.h>

/* Graphs for the report: SVG files for report.md and plain-text bars for report.txt.
   Every chart draws its entries in the order given (the report sorts them best first),
   gives every game the same colour in every chart, and marks a game without a value
   as n/a instead of drawing it at zero. SVGs carry their own white background so they
   read the same in light and dark viewers. */

typedef enum {
    OJH_UNIT_NUMBER,  /* 1,234.5 */
    OJH_UNIT_BYTES,   /* 12.3 MiB */
    OJH_UNIT_SECONDS, /* 0.214 s, 12.4 s, 3 min 20 s */
    OJH_UNIT_RATIO,   /* 0.52 */
    OJH_UNIT_PERCENT  /* 41% */
} ojh_unit;

/* A value for a person: grouped digits, bytes in KiB/MiB/GiB, seconds with sensible
   decimals, and the unit word after plain numbers when one is given. */
void ojh_format_value(char *out, size_t n, double value, ojh_unit unit, const char *word);

/* The same colour for the same game id in every chart. */
const char *ojh_game_colour(const char *game_id);

typedef struct {
    const char *label;   /* the game's name */
    const char *game_id; /* picks the colour */
    double value;
    int missing;         /* drawn as n/a */
} ojh_bar;

/* Horizontal bars. Returns 0 when the file was written. */
int ojh_chart_bars_svg(const char *path, const char *title, const char *subtitle, ojh_unit unit, const char *word,
                       const ojh_bar *bars, int count);
void ojh_chart_bars_text(FILE *f, const char *title, ojh_unit unit, const char *word, const ojh_bar *bars, int count);

typedef struct {
    const char *label;
    const char *game_id;
    double low, middle, high; /* e.g. lowest, median and highest data per turn */
    int missing;
} ojh_range;

/* A low-to-high span per game with a mark at the middle value, on a log scale when the
   values cover more than two orders of magnitude. */
int ojh_chart_ranges_svg(const char *path, const char *title, const char *subtitle, ojh_unit unit, const char *word,
                         const char *low_name, const char *middle_name, const char *high_name,
                         const ojh_range *ranges, int count);
void ojh_chart_ranges_text(FILE *f, const char *title, ojh_unit unit, const char *word, const ojh_range *ranges,
                           int count);

typedef struct {
    const char *label;
    const char *game_id;
    const double *y; /* y[i] is the value at x = i + 1 */
    int n;
} ojh_series;

/* One line per game over a shared x axis (turns), each series stretched to the width
   when x_as_share is set, so runs of different lengths are compared early to late. */
int ojh_chart_lines_svg(const char *path, const char *title, const char *subtitle, const char *x_name,
                        ojh_unit unit, const char *word, const ojh_series *series, int count, int x_as_share);

#endif
