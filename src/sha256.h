#ifndef OJH_SHA256_H
#define OJH_SHA256_H

#include <stddef.h>
#include <stdint.h>

/* SHA-256 of a buffer, written into 32 bytes. Part of the CPU reference workload. */
void ojh_sha256(const uint8_t *data, size_t len, uint8_t out[32]);

#endif
