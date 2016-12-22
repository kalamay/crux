#ifndef CRUX_HASH_H
#define CRUX_HASH_H

#include "def.h"

#include "seed.h"

XEXTERN uint64_t
xhash_metro64 (const void *s, size_t len, const union xseed *seed);

XEXTERN uint64_t
xhash_sip (const void *s, size_t len, const union xseed *seed);

XEXTERN uint64_t
xhash_sipcase (const void *s, size_t len, const union xseed *seed);

XEXTERN uint64_t
xhash_xx64 (const void *input, size_t len, const union xseed *seed);

#endif

