#ifndef OJH_REPORT_H
#define OJH_REPORT_H

#include <stddef.h>

/* Builds the human-readable report from every OJH result file (*.json) in a folder:
   report.md for publishing and report.txt for reading anywhere, with SVG graphs in
   a graphs folder beside them. It carries every statistic OJH measured for every game (turn speed, CPU
   and memory, frame rate, network and footprint), each game's OJH score, what went best
   and worst for every game and every statistic, how each game was measured, and the
   reasons the numbers must not be compared naively, worked out from the results
   themselves. A statistic a game was not measured for is n/a, never zero. */
int ojh_report_write(const char *dir, char *error, size_t error_len); /* 0 on success */

/* One game's scorecard from its result files alone (all for the same game): score-<id>.md,
   score-<id>.txt and a score-<id>.svg badge in dir. The report writes one for every game;
   this writes one without a report. The file name without extension goes in written.
   0 on success. */
int ojh_scorecard_write(const char *const *result_paths, int count, const char *dir, char *written,
                        size_t written_len, char *error, size_t error_len);

#endif
