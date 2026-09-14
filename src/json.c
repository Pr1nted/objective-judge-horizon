#include "json.h"

#include <math.h>

void ojh_json_init(ojh_json *w, FILE *out) {
    w->out = out;
    w->depth = 0;
    w->after_key = 0;
    w->broken = 0;
    for (int i = 0; i <= OJH_JSON_MAX_DEPTH; i++) w->has_items[i] = 0;
}

static void indent(ojh_json *w, int depth) {
    fputc('\n', w->out);
    for (int i = 0; i < depth; i++) fputs("  ", w->out);
}

static void separate(ojh_json *w) {
    if (w->after_key) {
        w->after_key = 0;
        return;
    }
    if (w->depth == 0) return;
    if (w->has_items[w->depth]) fputc(',', w->out);
    w->has_items[w->depth] = 1;
    indent(w, w->depth);
}

static void open_container(ojh_json *w, char opener) {
    if (w->broken) return;
    if (w->depth >= OJH_JSON_MAX_DEPTH) {
        w->broken = 1;
        return;
    }
    separate(w);
    fputc(opener, w->out);
    w->depth++;
    w->has_items[w->depth] = 0;
}

void ojh_json_object(ojh_json *w) { open_container(w, '{'); }
void ojh_json_array(ojh_json *w) { open_container(w, '['); }

void ojh_json_end(ojh_json *w, char closer) {
    if (w->broken || w->depth == 0) return;
    int had = w->has_items[w->depth];
    w->depth--;
    if (had) indent(w, w->depth);
    fputc(closer, w->out);
    if (w->depth == 0) fputc('\n', w->out);
}

static void write_escaped(FILE *out, const char *s) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if (*p < 0x20) fprintf(out, "\\u%04x", (unsigned)*p);
                else fputc(*p, out);
        }
    }
    fputc('"', out);
}

void ojh_json_key(ojh_json *w, const char *key) {
    if (w->broken) return;
    separate(w);
    write_escaped(w->out, key);
    fputs(": ", w->out);
    w->after_key = 1;
}

void ojh_json_string(ojh_json *w, const char *s) {
    if (w->broken) return;
    separate(w);
    write_escaped(w->out, s);
}

void ojh_json_int(ojh_json *w, int64_t v) {
    if (w->broken) return;
    separate(w);
    fprintf(w->out, "%lld", (long long)v);
}

void ojh_json_uint(ojh_json *w, uint64_t v) {
    if (w->broken) return;
    separate(w);
    fprintf(w->out, "%llu", (unsigned long long)v);
}

void ojh_json_double(ojh_json *w, double v, int decimals) {
    if (w->broken) return;
    separate(w);
    if (!isfinite(v)) fputs("null", w->out);
    else fprintf(w->out, "%.*f", decimals, v);
}

void ojh_json_bool(ojh_json *w, int v) {
    if (w->broken) return;
    separate(w);
    fputs(v ? "true" : "false", w->out);
}

void ojh_json_null(ojh_json *w) {
    if (w->broken) return;
    separate(w);
    fputs("null", w->out);
}
