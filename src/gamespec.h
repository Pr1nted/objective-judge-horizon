#ifndef OJH_GAMESPEC_H
#define OJH_GAMESPEC_H

#include <stddef.h>

#define OJH_GAMESPEC_VERSION 1

typedef enum { OJH_TURNS_PROTOCOL, OJH_TURNS_MARKER } ojh_turn_source;
enum { OJH_STREAM_EITHER = 0 };

typedef struct {
    char id[64];
    char name[128];
    char version[64];
    char license[64];
    char homepage[256];
    char spec_dir[1024];
    char **command;
    int command_count;
    char **environment;
    int environment_count;
    char *working_directory;
    ojh_turn_source turns_from;
    char *turn_ends;
    char *game_starts;
    int stream;
    char *players_after;
    char *regions_after;
    int players;
    long regions;
    char region_kind[16];
    int default_turns;
    double timeout_seconds;
    int players_chosen;
} ojh_gamespec;

typedef struct {
    int turns;
    unsigned seed;
    int players;
    const char *work;
    const char *ojh;
} ojh_spec_values;

int ojh_gamespec_load(const char *path, ojh_gamespec *out, char *error, size_t error_len);
int ojh_gamespec_parse(const char *text, size_t len, const char *spec_dir, ojh_gamespec *out, char *error,
                       size_t error_len);

char *ojh_gamespec_expand(const char *template_text, const ojh_gamespec *s, const ojh_spec_values *v);

char *ojh_gamespec_path(const ojh_gamespec *s, const char *path, int bare_names_stay);

int ojh_gamespec_write_template(const char *path, char *error, size_t error_len);

void ojh_gamespec_free(ojh_gamespec *s);

#endif
