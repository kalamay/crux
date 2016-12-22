#ifndef CRUX_HASH_H
#define CRUX_HASH_H

#include "seed.h"

#include <stdint.h>
#include <sys/types.h>

extern uint64_t
xhash_metro64 (const void *s, size_t len, const union xseed *seed);

extern uint64_t
xhash_sip (const void *s, size_t len, const union xseed *seed);

extern uint64_t
xhash_sipcase (const void *s, size_t len, const union xseed *seed);

extern uint64_t
xhash_xx64 (const void *input, size_t len, const union xseed *seed);

#endif

