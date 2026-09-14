#ifndef OJH_CHART_H
#define OJH_CHART_H

#include <stdio.h>

typedef enum {
    OJH_UNIT_NUMBER,
    OJH_UNIT_BYTES,
    OJH_UNIT_SECONDS,
    OJH_UNIT_RATIO,
    OJH_UNIT_PERCENT
} ojh_unit;

void ojh_format_value(char *out, size_t n, double value, ojh_unit unit, const char *word);

const char *ojh_game_colour(const char *game_id);

typedef struct {
    const char *label;
    const char *game_id;
    double value;
    int missing;
} ojh_bar;

int ojh_chart_bars_svg(const char *path, const char *title, const char *subtitle, ojh_unit unit, const char *word,
                       const ojh_bar *bars, int count);
void ojh_chart_bars_text(FILE *f, const char *title, ojh_unit unit, const char *word, const ojh_bar *bars, int count);

typedef struct {
    const char *label;
    const char *game_id;
    double low, middle, high;
    int missing;
} ojh_range;

int ojh_chart_ranges_svg(const char *path, const char *title, const char *subtitle, ojh_unit unit, const char *word,
                         const char *low_name, const char *middle_name, const char *high_name,
                         const ojh_range *ranges, int count);
void ojh_chart_ranges_text(FILE *f, const char *title, ojh_unit unit, const char *word, const ojh_range *ranges,
                           int count);

typedef struct {
    const char *label;
    const char *game_id;
    const double *y;
    int n;
} ojh_series;

int ojh_chart_lines_svg(const char *path, const char *title, const char *subtitle, const char *x_name,
                        ojh_unit unit, const char *word, const ojh_series *series, int count, int x_as_share);

#endif
