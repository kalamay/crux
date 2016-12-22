#ifndef CRUX_RAND_H
#define CRUX_RAND_H

#include <stdint.h>
#include <sys/types.h>

extern int
xrand (void *const dst, size_t len);

extern int
xrand_u32 (uint32_t bound, uint32_t *out);

extern int
xrand_u64 (uint64_t bound, uint64_t *out);

extern int
xrand_num (double *out);

#endif

