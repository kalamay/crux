#ifndef CRUX_SEED_H
#define CRUX_SEED_H

#include <stdint.h>

union xseed {
	struct { uint64_t low, high; } u128;
	uint64_t u64;
	uint32_t u32;
	uint8_t bytes[16];
};

extern const union xseed *const XSEED_RANDOM;
extern const union xseed *const XSEED_DEFAULT;

#endif

