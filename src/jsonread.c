#include "jsonread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DEPTH 64

typedef struct {
    const char *text;
    size_t len;
    size_t at;
    int depth;
    char *error;
    size_t error_len;
    int failed;
} parser;

static void set_error(parser *p, const char *what) {
    if (p->failed) return;
    p->failed = 1;
    if (p->error && p->error_len) snprintf(p->error, p->error_len, "%s at byte %lu", what, (unsigned long)p->at);
}

static void skip_space(parser *p) {
    while (p->at < p->len) {
        char c = p->text[p->at];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
        p->at++;
    }
}

static int peek(parser *p) { return p->at < p->len ? (unsigned char)p->text[p->at] : -1; }

static int literal(parser *p, const char *word) {
    size_t n = strlen(word);
    if (p->len - p->at < n || memcmp(p->text + p->at, word, n) != 0) return 0;
    p->at += n;
    return 1;
}

static void free_contents(ojh_jvalue *v);

static size_t put_utf8(char *out, unsigned long cp) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static int hex4(parser *p, unsigned long *out) {
    if (p->len - p->at < 4) return 0;
    unsigned long v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p->text[p->at + (size_t)i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned long)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned long)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned long)(c - 'A' + 10);
        else return 0;
    }
    p->at += 4;
    *out = v;
    return 1;
}

static char *parse_string(parser *p) {
    if (peek(p) != '"') {
        set_error(p, "expected a string");
        return NULL;
    }
    p->at++;
    size_t start = p->at;
    char *out = malloc(p->len - start + 1);
    if (!out) {
        set_error(p, "out of memory");
        return NULL;
    }
    size_t n = 0;
    while (p->at < p->len) {
        unsigned char c = (unsigned char)p->text[p->at++];
        if (c == '"') {
            out[n] = '\0';
            return out;
        }
        if (c < 0x20) break;
        if (c != '\\') {
            out[n++] = (char)c;
            continue;
        }
        if (p->at >= p->len) break;
        char e = p->text[p->at++];
        switch (e) {
            case '"': out[n++] = '"'; break;
            case '\\': out[n++] = '\\'; break;
            case '/': out[n++] = '/'; break;
            case 'b': out[n++] = '\b'; break;
            case 'f': out[n++] = '\f'; break;
            case 'n': out[n++] = '\n'; break;
            case 'r': out[n++] = '\r'; break;
            case 't': out[n++] = '\t'; break;
            case 'u': {
                unsigned long cp;
                if (!hex4(p, &cp)) goto bad;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    unsigned long low;
                    if (p->len - p->at < 6 || p->text[p->at] != '\\' || p->text[p->at + 1] != 'u') goto bad;
                    p->at += 2;
                    if (!hex4(p, &low) || low < 0xDC00 || low > 0xDFFF) goto bad;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    goto bad;
                }
                n += put_utf8(out + n, cp);
                break;
            }
            default:
                goto bad;
        }
    }
bad:
    free(out);
    set_error(p, "malformed string");
    return NULL;
}

static int parse_value(parser *p, ojh_jvalue *v);

static int add_item(parser *p, ojh_jvalue *container, size_t *cap, ojh_jvalue **slot) {
    if (container->count == *cap) {
        size_t grown = *cap ? *cap * 2 : 8;
        ojh_jvalue *items = realloc(container->items, grown * sizeof *items);
        if (!items) {
            set_error(p, "out of memory");
            return 0;
        }
        container->items = items;
        *cap = grown;
    }
    *slot = &container->items[container->count];
    memset(*slot, 0, sizeof **slot);
    return 1;
}

static int parse_array(parser *p, ojh_jvalue *v) {
    v->type = OJH_JARRAY;
    p->at++;
    size_t cap = 0;
    skip_space(p);
    if (peek(p) == ']') {
        p->at++;
        return 1;
    }
    for (;;) {
        ojh_jvalue *item;
        if (!add_item(p, v, &cap, &item)) return 0;
        v->count++;
        if (!parse_value(p, item)) return 0;
        skip_space(p);
        int c = peek(p);
        p->at++;
        if (c == ']') return 1;
        if (c != ',') {
            p->at--;
            set_error(p, "expected , or ] in an array");
            return 0;
        }
    }
}

static int parse_object(parser *p, ojh_jvalue *v) {
    v->type = OJH_JOBJECT;
    p->at++;
    size_t cap = 0;
    skip_space(p);
    if (peek(p) == '}') {
        p->at++;
        return 1;
    }
    for (;;) {
        skip_space(p);
        char *key = parse_string(p);
        if (!key) return 0;
        skip_space(p);
        if (peek(p) != ':') {
            free(key);
            set_error(p, "expected : after an object key");
            return 0;
        }
        p->at++;
        ojh_jvalue *member;
        if (!add_item(p, v, &cap, &member)) {
            free(key);
            return 0;
        }
        v->count++;
        member->key = key;
        if (!parse_value(p, member)) return 0;
        skip_space(p);
        int c = peek(p);
        p->at++;
        if (c == '}') return 1;
        if (c != ',') {
            p->at--;
            set_error(p, "expected , or } in an object");
            return 0;
        }
    }
}

static int parse_number(parser *p, ojh_jvalue *v) {
    size_t start = p->at;
    if (peek(p) == '-') p->at++;
    int digits = 0;
    while (p->at < p->len && p->text[p->at] >= '0' && p->text[p->at] <= '9') {
        p->at++;
        digits++;
    }
    if (p->at < p->len && p->text[p->at] == '.') {
        p->at++;
        while (p->at < p->len && p->text[p->at] >= '0' && p->text[p->at] <= '9') {
            p->at++;
            digits++;
        }
    }
    if (p->at < p->len && (p->text[p->at] == 'e' || p->text[p->at] == 'E')) {
        p->at++;
        if (p->at < p->len && (p->text[p->at] == '+' || p->text[p->at] == '-')) p->at++;
        while (p->at < p->len && p->text[p->at] >= '0' && p->text[p->at] <= '9') p->at++;
    }
    if (digits == 0) {
        p->at = start;
        set_error(p, "expected a value");
        return 0;
    }
    char token[64];
    size_t n = p->at - start;
    if (n >= sizeof token) {
        set_error(p, "number too long");
        return 0;
    }
    memcpy(token, p->text + start, n);
    token[n] = '\0';
    v->type = OJH_JNUMBER;
    v->number = strtod(token, NULL);
    return 1;
}

static int parse_value(parser *p, ojh_jvalue *v) {
    if (++p->depth > MAX_DEPTH) {
        set_error(p, "nested too deeply");
        return 0;
    }
    skip_space(p);
    int ok;
    switch (peek(p)) {
        case '{': ok = parse_object(p, v); break;
        case '[': ok = parse_array(p, v); break;
        case '"':
            v->type = OJH_JSTRING;
            v->string = parse_string(p);
            ok = v->string != NULL;
            break;
        case 't':
            ok = literal(p, "true");
            v->type = OJH_JBOOL;
            v->number = 1;
            if (!ok) set_error(p, "expected true");
            break;
        case 'f':
            ok = literal(p, "false");
            v->type = OJH_JBOOL;
            v->number = 0;
            if (!ok) set_error(p, "expected false");
            break;
        case 'n':
            ok = literal(p, "null");
            v->type = OJH_JNULL;
            if (!ok) set_error(p, "expected null");
            break;
        default:
            ok = parse_number(p, v);
    }
    p->depth--;
    return ok;
}

ojh_jvalue *ojh_jparse(const char *text, size_t len, char *error, size_t error_len) {
    parser p = {text, len, 0, 0, error, error_len, 0};
    if (error && error_len) error[0] = '\0';
    ojh_jvalue *root = calloc(1, sizeof *root);
    if (!root) return NULL;
    if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) {
        p.at = 3;
    }
    if (!parse_value(&p, root)) {
        ojh_jfree(root);
        return NULL;
    }
    skip_space(&p);
    if (p.at != p.len) {
        set_error(&p, "unexpected text after the value");
        ojh_jfree(root);
        return NULL;
    }
    return root;
}

ojh_jvalue *ojh_jparse_file(const char *path, char *error, size_t error_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (error && error_len) snprintf(error, error_len, "cannot open %s", path);
        return NULL;
    }
    size_t cap = 1 << 16, len = 0;
    char *buf = malloc(cap);
    while (buf) {
        if (len == cap) {
            char *grown = realloc(buf, cap * 2);
            if (!grown) {
                free(buf);
                buf = NULL;
                break;
            }
            buf = grown;
            cap *= 2;
        }
        size_t got = fread(buf + len, 1, cap - len, f);
        len += got;
        if (got == 0) break;
    }
    fclose(f);
    if (!buf) {
        if (error && error_len) snprintf(error, error_len, "out of memory reading %s", path);
        return NULL;
    }
    ojh_jvalue *root = ojh_jparse(buf, len, error, error_len);
    free(buf);
    return root;
}

const ojh_jvalue *ojh_jget(const ojh_jvalue *object, const char *key) {
    if (!object || object->type != OJH_JOBJECT) return NULL;
    for (size_t i = 0; i < object->count; i++) {
        if (object->items[i].key && strcmp(object->items[i].key, key) == 0) return &object->items[i];
    }
    return NULL;
}

const ojh_jvalue *ojh_jpath(const ojh_jvalue *root, const char *dotted) {
    const ojh_jvalue *v = root;
    char part[128];
    const char *at = dotted;
    while (v && *at) {
        const char *dot = strchr(at, '.');
        size_t n = dot ? (size_t)(dot - at) : strlen(at);
        if (n >= sizeof part) return NULL;
        memcpy(part, at, n);
        part[n] = '\0';
        v = ojh_jget(v, part);
        at = dot ? dot + 1 : at + n;
    }
    return v;
}

double ojh_jnumber(const ojh_jvalue *v, double fallback) {
    return v && (v->type == OJH_JNUMBER || v->type == OJH_JBOOL) ? v->number : fallback;
}

const char *ojh_jstring(const ojh_jvalue *v, const char *fallback) {
    return v && v->type == OJH_JSTRING ? v->string : fallback;
}

int ojh_jpresent(const ojh_jvalue *v) { return v && v->type != OJH_JNULL; }

static void free_contents(ojh_jvalue *v) {
    for (size_t i = 0; i < v->count; i++) free_contents(&v->items[i]);
    free(v->items);
    free(v->string);
    free(v->key);
}

void ojh_jfree(ojh_jvalue *v) {
    if (!v) return;
    free_contents(v);
    free(v);
}
