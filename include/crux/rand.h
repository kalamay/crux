#ifndef CRUX_RAND_H
#define CRUX_RAND_H

#include "def.h"

XEXTERN int
xrand (void *const dst, size_t len);

XEXTERN int
xrand_u32 (uint32_t bound, uint32_t *out);

XEXTERN int
xrand_u64 (uint64_t bound, uint64_t *out);

XEXTERN int
xrand_num (double *out);

#endif

