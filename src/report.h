#ifndef OJH_REPORT_H
#define OJH_REPORT_H

#include <stddef.h>

/* Builds the human-readable report from every OJH result file (*.json) in a folder:
   report.md for publishing and report.txt for reading anywhere. Both carry the machine,
   each metric's table with raw and normalised figures side by side (n/a, never zero, for
   what a game does not report), how each game was measured, and the reasons the numbers
   must not be compared naively, worked out from the results themselves. */
int ojh_report_write(const char *dir, char *error, size_t error_len); /* 0 on success */

/* One game's scorecard from its result file alone: score-<id>.md, score-<id>.txt and a
   score-<id>.svg badge in dir. The report writes one for every game; this writes one
   without a report. The file name without extension goes in written. 0 on success. */
int ojh_scorecard_write(const char *result_path, const char *dir, char *written, size_t written_len, char *error,
                        size_t error_len);

#endif
