#ifndef OJH_FOOTPRINT_H
#define OJH_FOOTPRINT_H

#include <stddef.h>
#include <stdint.h>

#include "gamespec.h"
#include "json.h"
#include "tpm.h"

typedef struct {
    ojh_game game;
    char id[64];
    char name[128];
    char version[64];
    int exit_code;
    int has_install;
    uint64_t install_bytes;
    uint64_t install_files;
    char install_paths[1200];
    int has_save;
    uint64_t save_bytes;
    double save_seconds;
    double load_seconds;
    char how[900];
} ojh_footprint;

int ojh_footprint_run(ojh_game game, const ojh_tpm_options *options, ojh_footprint *out, char *error, size_t error_len);
int ojh_footprint_run_spec(const ojh_gamespec *spec, const ojh_tpm_options *options, ojh_footprint *out, char *error,
                           size_t error_len);
void ojh_footprint_json(ojh_json *w, const ojh_footprint *f);
int ojh_unciv_prepare(const ojh_tpm_options *o, char *classpath, size_t classpath_len, char *assets, size_t assets_len,
                      char *error, size_t error_len);

#endif
