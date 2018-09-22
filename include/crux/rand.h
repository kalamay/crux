#ifndef CRUX_RAND_H
#define CRUX_RAND_H

#include "def.h"

XEXTERN ssize_t
xrand_secure(void *const dst, size_t len);

struct xrand32
{
	uint64_t state;
	uint64_t inc;
};

struct xrand64
{
    xuint128 state;
    xuint128 inc;
};

XEXTERN void xrand32_seed(struct xrand32 *rng, uint64_t state, uint64_t seq);
XEXTERN void xrand64_seed(struct xrand64 *rng, xuint128 state, xuint128 seq);
XEXTERN int xrand32_seed_secure(struct xrand32 *rng);
XEXTERN int xrand64_seed_secure(struct xrand64 *rng);
XEXTERN uint32_t xrand32(struct xrand32 *rng);
XEXTERN uint64_t xrand64(struct xrand64 *rng);
XEXTERN uint32_t xrand32_bound(struct xrand32 *rng, uint32_t bound);
XEXTERN uint64_t xrand64_bound(struct xrand64 *rng, uint64_t bound);
XEXTERN uint32_t xrand32_global(void);
XEXTERN uint64_t xrand64_global(void);
XEXTERN double xrand_real(struct xrand64 *rng);
XEXTERN double xrand_real_global(void);

#endif

