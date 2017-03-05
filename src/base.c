#include "../include/crux/err.h"
#include "../include/crux/version.h"
#include "../include/crux/clock.h"
#include "../include/crux/rand.h"
#include "../include/crux/seed.h"

#include "config.h"

#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>
#include <assert.h>

#if HAS_MACH_TIME
# include <mach/mach_time.h>
static mach_timebase_info_data_t info = { 1, 1 };
#endif

#if !HAS_ARC4
# include <unistd.h>
# include <sys/stat.h>
static int randfd = -1;
#endif

const uint64_t XPOWER2_PRIMES[64] = {
	1ULL, 2ULL, 3ULL,
	7ULL, 13ULL, 31ULL,
	61ULL, 127ULL, 251ULL,
	509ULL, 1021ULL, 2039ULL,
	4093ULL, 8191ULL, 16381ULL,
	32749ULL, 65521ULL, 131071ULL,
	262139ULL, 524287ULL, 1048573ULL,
	2097143ULL, 4194301ULL, 8388593ULL,
	16777213ULL, 33554393ULL, 67108859ULL,
	134217689ULL, 268435399ULL, 536870909ULL,
	1073741789ULL, 2147483647ULL, 4294967291ULL,
	8589934583ULL, 17179869143ULL, 34359738337ULL,
	68719476731ULL, 137438953447ULL, 274877906899ULL,
	549755813881ULL, 1099511627689ULL, 2199023255531ULL,
	4398046511093ULL, 8796093022151ULL, 17592186044399ULL,
	35184372088777ULL, 70368744177643ULL, 140737488355213ULL,
	281474976710597ULL, 562949953421231ULL, 1125899906842597ULL,
	2251799813685119ULL, 4503599627370449ULL, 9007199254740881ULL,
	18014398509481951ULL, 36028797018963913ULL, 72057594037927931ULL,
	144115188075855859ULL, 288230376151711717ULL, 576460752303423433ULL,
	1152921504606846883ULL, 2305843009213693951ULL, 4611686018427387847ULL,
	9223372036854775783ULL,
};

static union xseed SEED_RANDOM;
static union xseed SEED_DEFAULT = {
	.u128 = {
		0x9ae16a3b2f90404fULL,
		0xc3a5c85c97cb3127ULL
	}
};

const union xseed *const XSEED_RANDOM = &SEED_RANDOM;
const union xseed *const XSEED_DEFAULT = &SEED_DEFAULT;

int
xinit(void)
{
#if HAS_MACH_TIME
	(void)mach_timebase_info(&info);
#endif

#if !HAS_ARC4
	if (randfd >= 0) { close(randfd); }

	while (1) {
		randfd = open("/dev/urandom", O_RDONLY);
		if (randfd >= 0) {
			break;
		}
		int rc = XERRNO;
		if (rc != XESYS(EINTR)) {
			fprintf(stderr, "[cux:init:error failed to open /dev/urandom: %s", xerr_str(rc));
			return rc;
		}
	}

	// stat the randfd so it can be verified
	struct stat sbuf;
	if (fstat(randfd, &sbuf) < 0) {
		int rc = XERRNO;
		fprintf(stderr, "[crux:init:error] failed to stat /dev/urandom: %s", xerr_str(rc));
		close(randfd);
		randfd = -1;
		return rc;
	}

	// check that it is a char special
	if (!S_ISCHR(sbuf.st_mode) ||
			// verify that the device is /dev/random or /dev/urandom (linux only)
			(sbuf.st_rdev != makedev(1, 8) && sbuf.st_rdev != makedev(1, 9))) {
		int rc = XESYS(ENODEV);
		fprintf(stderr, "[crux:init:error] /dev/urandom is an invalid device");
		close(randfd);
		randfd = -1;
		return rc;
	}
#endif

	return xrand(&SEED_RANDOM, sizeof(SEED_RANDOM));
}

static void __attribute__((constructor))
auto_init(void)
{
	int rc = xinit();
	if (rc < 0) { exit(1); }
}

struct xversion
xversion(void)
{
	return (struct xversion) { VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH };
}

int
xclock_real(struct xclock *c)
{
	assert(c != NULL);

#if HAS_CLOCK_GETTIME
	if (clock_gettime(CLOCK_REALTIME, &c->ts) < 0) {
		return XERRNO;
	}
#else
	struct timeval now;
    if (gettimeofday(&now, NULL) < 0) {
		return XERRNO;
	}
    c->ts.tv_sec = now.tv_sec;
    c->ts.tv_nsec = now.tv_usec * 1000;
#endif
	return 0;
}

int
xclock_mono(struct xclock *c)
{
	assert(c != NULL);

#if HAS_CLOCK_GETTIME
	if (clock_gettime(CLOCK_MONOTONIC, &c->ts) < 0) {
		return XERRNO;
	}
#elif HAS_MACH_TIME
	XCLOCK_SET_NSEC(c, (mach_absolute_time() * info.numer) / info.denom);
#else
	XCLOCK_SET_NSEC(c, clock() * (X_NSEC_PER_SEC / CLOCKS_PER_SEC));
#endif
	return 0;
}

double
xclock_diff(struct xclock *c)
{
	assert(c != NULL);

	struct xclock now;
	if (xclock_mono(&now) < 0) {
		return NAN;
	}
	XCLOCK_SUB(&now, c);
	return XCLOCK_TIME(&now);
}

double
xclock_step(struct xclock *c)
{
	assert(c != NULL);

	struct xclock now;
	if (xclock_mono(&now) < 0) {
		return NAN;
	}
	double time = XCLOCK_TIME(&now) - XCLOCK_TIME(c);
	*c = now;
	return time;
}

void
xclock_print(const struct xclock *c, FILE *out)
{
	if (out == NULL) {
		out = stdout;
	}

	if (c == NULL) {
		fprintf(out, "<crux:clock:(null)>\n");
	}
	else {
		fprintf(out, "<crux:clock:%p time=%f>\n", (void *)c, XCLOCK_TIME(c));
	}
}



#if HAS_ARC4

int
xrand(void *const restrict dst, size_t len)
{
	arc4random_buf(dst, len);
	return 0;
}

int
xrand_u32(uint32_t bound, uint32_t *out)
{
	*out = bound ? arc4random_uniform(bound) : arc4random();
	return 0;
}

#else

int
xrand(void *const restrict dst, size_t len)
{
	size_t amt = 0;
	while (amt < len) {
		ssize_t r = read(randfd, (char *)dst+amt, len-amt);
		if (r > 0) {
			amt += (size_t)r;
		}
		else if (r == 0 || errno != EINTR) {
			return XERRNO;
		}
	}
	return 0;
}

int
xrand_u32(uint32_t bound, uint32_t *out)
{
	uint32_t val;
	int rc = xrand(&val, sizeof(val));
	if (rc < 0) { return rc; }
	if (bound) {
		val = ((double)val / (double)UINT32_MAX) * bound;
	}
	*out = val;
	return 0;
}

#endif

int
xrand_u64(uint64_t bound, uint64_t *out)
{
	uint64_t val;
	int rc = xrand(&val, sizeof(val));
	if (rc < 0) { return rc; }
	if (bound) {
		val = ((double)val / (double)UINT64_MAX) * bound;
	}
	*out = val;
	return 0;
}

int
xrand_num(double *out)
{
	uint64_t val;
	int rc = xrand(&val, sizeof(val));
	if (rc < 0) { return rc; }
	*out = (double)val / (double)UINT64_MAX;
	return 0;
}

