#ifndef OJH_REPORT_H
#define OJH_REPORT_H

#include <stddef.h>

/* Builds the human-readable report from every OJH result file (*.json) in a folder:
   report.md for publishing and report.txt for reading anywhere. Both carry the machine,
   each metric's table with raw and normalised figures side by side (n/a, never zero, for
   what a game does not report), how each game was measured, and the reasons the numbers
   must not be compared naively, worked out from the results themselves. */
int ojh_report_write(const char *dir, char *error, size_t error_len); /* 0 on success */

#endif
