#ifndef OJH_REPORT_H
#define OJH_REPORT_H

#include <stddef.h>

int ojh_report_write(const char *dir, char *error, size_t error_len);

int ojh_scorecard_write(const char *const *result_paths, int count, const char *dir, char *written,
                        size_t written_len, char *error, size_t error_len);

#endif
