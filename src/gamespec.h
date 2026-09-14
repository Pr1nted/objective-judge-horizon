#ifndef OJH_GAMESPEC_H
#define OJH_GAMESPEC_H

#include <stddef.h>

/* A game described by a spec file, so any developer can put their own game through OJH
   without changing OJH. The spec says how to start the game's own AI-only mode and how
   OJH tells when a turn ends: either the game prints OJH's turn lines (the protocol), or
   OJH watches for a line the game already prints once per turn (a marker). The format is
   documented in docs/adding-your-game.md. */

#define OJH_GAMESPEC_VERSION 1

typedef enum { OJH_TURNS_PROTOCOL, OJH_TURNS_MARKER } ojh_turn_source;
enum { OJH_STREAM_EITHER = 0 }; /* otherwise OJH_STDOUT or OJH_STDERR from runner.h */

typedef struct {
    char id[64];
    char name[128];
    char version[64];
    char license[64];
    char homepage[256];
    char spec_dir[1024];     /* the folder the spec is in; relative paths start here */
    char **command;          /* templates: {turns} {seed} {players} {work} {spec_dir} {ojh} */
    int command_count;
    char **environment;      /* "NAME=value" templates */
    int environment_count;
    char *working_directory; /* template, or NULL for the spec's folder */
    ojh_turn_source turns_from;
    char *turn_ends;         /* marker: a line containing this ends a turn */
    char *game_starts;       /* marker, optional: the first line containing this ends start-up */
    int stream;              /* marker: which output to watch */
    char *players_after;     /* optional: the number after this text is the player count */
    char *regions_after;     /* optional: the number after this text is the map size */
    int players;             /* fixed counts for a game that does not print them; 0 unknown */
    long regions;
    char region_kind[16];
    int default_turns;
    double timeout_seconds;
    int players_chosen;      /* the command or environment passes {players} to the game */
} ojh_gamespec;

typedef struct {
    int turns;
    unsigned seed;
    int players;
    const char *work; /* scratch folder */
    const char *ojh;  /* this executable */
} ojh_spec_values;

/* Both return 0 on success; otherwise error says what is wrong and how to fix it. */
int ojh_gamespec_load(const char *path, ojh_gamespec *out, char *error, size_t error_len);
int ojh_gamespec_parse(const char *text, size_t len, const char *spec_dir, ojh_gamespec *out, char *error,
                       size_t error_len);

/* The template with its placeholders filled in. malloc'd; NULL when out of memory. */
char *ojh_gamespec_expand(const char *template_text, const ojh_gamespec *s, const ojh_spec_values *v);

/* A path from the spec, made relative to the spec's folder. With bare_names_stay, a name
   without a folder ("python3") is left alone so it is found on PATH; "./game" is not.
   malloc'd. */
char *ojh_gamespec_path(const ojh_gamespec *s, const char *path, int bare_names_stay);

/* A starter spec to fill in. Refuses to overwrite an existing file. */
int ojh_gamespec_write_template(const char *path, char *error, size_t error_len);

void ojh_gamespec_free(ojh_gamespec *s);

#endif
