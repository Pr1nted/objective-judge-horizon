#ifndef OJH_JSONREAD_H
#define OJH_JSONREAD_H

#include <stddef.h>

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
    double number;
    char *string;
    char *key;
    ojh_jvalue *items;
    size_t count;
};

ojh_jvalue *ojh_jparse(const char *text, size_t len, char *error, size_t error_len);
ojh_jvalue *ojh_jparse_file(const char *path, char *error, size_t error_len);

const ojh_jvalue *ojh_jget(const ojh_jvalue *object, const char *key);
const ojh_jvalue *ojh_jpath(const ojh_jvalue *root, const char *dotted);

double ojh_jnumber(const ojh_jvalue *v, double fallback);
const char *ojh_jstring(const ojh_jvalue *v, const char *fallback);
int ojh_jpresent(const ojh_jvalue *v);

void ojh_jfree(ojh_jvalue *v);

#endif
