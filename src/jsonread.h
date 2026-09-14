#ifndef OJH_JSONREAD_H
#define OJH_JSONREAD_H

#include <stddef.h>

/* Reads JSON back in: the result files OJH writes, so reports can be built from them.
   The whole document becomes a tree of values; lookups by key or dotted path return
   NULL for anything missing, and the accessors take a fallback for that case. */

typedef enum {
    OJH_JNULL,
    OJH_JBOOL,
    OJH_JNUMBER,
    OJH_JSTRING,
    OJH_JARRAY,
    OJH_JOBJECT
} ojh_jtype;

typedef struct ojh_jvalue ojh_jvalue;
struct ojh_jvalue {
    ojh_jtype type;
    double number;      /* OJH_JNUMBER; OJH_JBOOL as 0 or 1 */
    char *string;       /* OJH_JSTRING, UTF-8 */
    char *key;          /* set when this value is a member of an object */
    ojh_jvalue *items;  /* OJH_JARRAY elements or OJH_JOBJECT members */
    size_t count;
};

/* Parses len bytes of text. Returns NULL on malformed input, with the reason and the
   byte offset in error. */
ojh_jvalue *ojh_jparse(const char *text, size_t len, char *error, size_t error_len);
ojh_jvalue *ojh_jparse_file(const char *path, char *error, size_t error_len);

const ojh_jvalue *ojh_jget(const ojh_jvalue *object, const char *key);
/* "result.per_turn.median_seconds": object members only, no array indexes. */
const ojh_jvalue *ojh_jpath(const ojh_jvalue *root, const char *dotted);

double ojh_jnumber(const ojh_jvalue *v, double fallback);   /* numbers and booleans */
const char *ojh_jstring(const ojh_jvalue *v, const char *fallback);
int ojh_jpresent(const ojh_jvalue *v);                      /* not missing and not null */

void ojh_jfree(ojh_jvalue *v);

#endif
