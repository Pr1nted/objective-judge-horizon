#ifndef OJH_JSON_H
#define OJH_JSON_H

#include <stdint.h>
#include <stdio.h>

#define OJH_JSON_MAX_DEPTH 32

typedef struct {
    FILE *out;
    int depth;
    int after_key;
    int broken;
    unsigned char has_items[OJH_JSON_MAX_DEPTH + 1];
} ojh_json;

void ojh_json_init(ojh_json *w, FILE *out);
void ojh_json_object(ojh_json *w);
void ojh_json_array(ojh_json *w);
void ojh_json_end(ojh_json *w, char closer);
void ojh_json_key(ojh_json *w, const char *key);
void ojh_json_string(ojh_json *w, const char *s);
void ojh_json_int(ojh_json *w, int64_t v);
void ojh_json_uint(ojh_json *w, uint64_t v);
void ojh_json_double(ojh_json *w, double v, int decimals);
void ojh_json_bool(ojh_json *w, int v);
void ojh_json_null(ojh_json *w);

#define ojh_json_end_object(w) ojh_json_end((w), '}')
#define ojh_json_end_array(w) ojh_json_end((w), ']')

#endif
