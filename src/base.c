#include "../include/crux/err.h"
#include "../include/crux/version.h"
#include "../include/crux/clock.h"
#include "../include/crux/rand.h"
#include "../include/crux/seed.h"
#include "heap.h"

#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>
#include <assert.h>

size_t xpagesize;

#if HAS_MACH_TIME
# include <mach/mach_time.h>
static mach_timebase_info_data_t info = { 1, 1 };
#endif

#if !HAS_ARC4 && !HAS_GETRANDOM
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

#if !HAS_GETRANDOM && !HAS_ARC4

# ifdef S_ISNAM
#  define IS_RAND_MODE(mode) (S_ISNAM(mode) || S_ISCHR(mode))
# else
#  define IS_RAND_MODE(mode) (S_ISCHR(mode))
# endif

# if defined(__linux__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(1, 9))
# elif defined(__APPLE__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(14, 1))
# elif defined(__FreeBSD__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(0, 10))
# elif defined(__DragonFly__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(8, 4))
# elif defined(__NetBSD__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(46, 1))
# elif defined(__OpenBSD__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(45, 2))
# else
#  define IS_RAND_DEVICE(dev) 1
# endif

# define IS_RAND(st) (IS_RAND_MODE((st).st_mode) && IS_RAND_DEVICE((st).st_rdev))

static int
random_verify(int fd)
{
	struct stat st;
	if (fstat(fd, &st) < 0) { return XERRNO; }
	if (!IS_RAND(st)) { return XESYS(EBADF); }
	return 0;
}

static int
random_open(void)
{
	int fd, err;
	for (;;) {
		fd = open("/dev/urandom", O_RDONLY|O_CLOEXEC);
		if (fd >= 0) {
			err = random_verify(fd);
			if (err == 0) { return fd; }
			close(fd);
		}
		else {
			err = XERRNO;
		}
		if (err != XESYS(EINTR)) {
			return err;
		}
	}
}
#endif

int
xinit(void)
{
	xpagesize = sysconf(_SC_PAGESIZE);
	xheap_pagecount = xpagesize / sizeof(void *);
	xheap_pagemask = xheap_pagecount - 1;
	xheap_pageshift = log2(xheap_pagecount);
#if HAS_MACH_TIME
	(void)mach_timebase_info(&info);
#endif
#if !HAS_GETRANDOM && !HAS_ARC4
	int fd = random_open();
	if (fd < 0) { return fd; }
	if (randfd >= 0) { close(randfd); }
	randfd = fd;
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

int64_t
xmono(void)
{
#if HAS_MACH_TIME
	return (mach_absolute_time() * info.numer) / info.denom;
#else
# if HAS_CLOCK_GETTIME
	struct timespec c;
	if (clock_gettime(CLOCK_MONOTONIC, &c) == 0) {
		return XCLOCK_NSEC(&c);
	}
# endif
	return clock() * (X_NSEC_PER_SEC / CLOCKS_PER_SEC);
#endif
}

int
xclock_real(struct timespec *c)
{
	assert(c != NULL);

#if HAS_CLOCK_GETTIME
	if (clock_gettime(CLOCK_REALTIME, c) < 0) {
		return XERRNO;
	}
#else
	struct timeval now;
    if (gettimeofday(&now, NULL) < 0) {
		return XERRNO;
	}
    c->tv_sec = now.tv_sec;
    c->tv_nsec = now.tv_usec * 1000;
#endif
	return 0;
}

int
xclock_mono(struct timespec *c)
{
	assert(c != NULL);

#if HAS_CLOCK_GETTIME
	if (clock_gettime(CLOCK_MONOTONIC, c) < 0) {
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
xclock_diff(struct timespec *c)
{
	assert(c != NULL);

	struct timespec now;
	if (xclock_mono(&now) < 0) {
		return NAN;
	}
	XCLOCK_SUB(&now, c);
	return XCLOCK_TIME(&now);
}

double
xclock_step(struct timespec *c)
{
	assert(c != NULL);

	struct timespec now;
	if (xclock_mono(&now) < 0) {
		return NAN;
	}
	double time = XCLOCK_TIME(&now) - XCLOCK_TIME(c);
	*c = now;
	return time;
}

void
xclock_print(const struct timespec *c, FILE *out)
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

void
xtimeout_start(struct xtimeout *t, int ms, const struct timespec *c)
{
	if (ms < 0) {
		t->rel = -1;
	}
	else {
		t->rel = X_MSEC_TO_NSEC(ms);
		t->abs = t->rel + XCLOCK_NSEC(c);
	}
}

int
xtimeout(struct xtimeout *t, const struct timespec *c)
{
	if (t->rel < 0) { return -1; }
	t->rel = t->abs - XCLOCK_NSEC(c);
	if (t->rel < 0) { t->rel = 0; }
	int64_t ms = X_NSEC_TO_MSEC(t->rel);
	return ms < INT_MAX ? (int)ms : INT_MAX;
}



#if HAS_GETRANDOM

#include <sys/random.h>

int
xrand(void *const restrict dst, size_t len)
{
	return getrandom(dst, len, 0);
}

#elif HAS_ARC4

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

#endif

#if !HAS_ARC4

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

