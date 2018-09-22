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

static union xseed SEED_RANDOM;
static union xseed SEED_DEFAULT = {
	.u128 = { 0x9ae16a3b2f90404f, 0xc3a5c85c97cb3127 }
};

const union xseed *const XSEED_RANDOM = &SEED_RANDOM;
const union xseed *const XSEED_DEFAULT = &SEED_DEFAULT;

XLOCAL int xrand_init(void);
XLOCAL int xrand_init_thread(void);

int
xinit_thread(void)
{
	return xrand_init_thread();
}

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

	int rc = xrand_init();
	if (rc < 0) {
		return rc;
	}

	ssize_t n = xrand_secure(&SEED_RANDOM, sizeof(SEED_RANDOM));
	if (n < 0) {
		return (int)n;
	}

	return xinit_thread();
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
		return xerrno;
	}
#else
	struct timeval now;
    if (gettimeofday(&now, NULL) < 0) {
		return xerrno;
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
		return xerrno;
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

