#include "../include/crux/seed.h"
#include "../include/crux/rand.h"

static union xseed SEED_RANDOM;
static union xseed SEED_DEFAULT = {
	.u128 = {
		0x9ae16a3b2f90404fULL,
		0xc3a5c85c97cb3127ULL
	}
};

const union xseed *const XSEED_RANDOM = &SEED_RANDOM;
const union xseed *const XSEED_DEFAULT = &SEED_DEFAULT;

void
xinit_seed (void)
{
	xrand (&SEED_RANDOM, sizeof SEED_RANDOM);
}

