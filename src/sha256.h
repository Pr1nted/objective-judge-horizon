#ifndef OJH_SHA256_H
#define OJH_SHA256_H

#include <stddef.h>
#include <stdint.h>

void ojh_sha256(const uint8_t *data, size_t len, uint8_t out[32]);

#endif
