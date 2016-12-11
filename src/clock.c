#include "../include/crux/clock.h"
#include "../include/crux/err.h"
#include "config.h"

#include <errno.h>
#include <assert.h>
#include <math.h>
#include <time.h>

#if !HAS_CLOCK_GETTIME && HAS_MACH_TIME

#include <mach/mach_time.h>

static mach_timebase_info_data_t info = { 1, 1 };

static void __attribute__((constructor))
init (void)
{
	(void)mach_timebase_info (&info);
}

#endif

int
xclock_real (struct xclock *c)
{
	assert (c != NULL);

#if HAS_CLOCK_GETTIME
	if (clock_gettime (CLOCK_REALTIME, &c->ts) < 0) {
		return XERRNO;
	}
#else
	struct timeval now;
    if (gettimeofday (&now, NULL) < 0) {
		return XERRNO;
	}
    c->ts.tv_sec = now.tv_sec;
    c->ts.tv_nsec = now.tv_usec * 1000;
#endif
	return 0;
}

int
xclock_mono (struct xclock *c)
{
	assert (c != NULL);

#if HAS_CLOCK_GETTIME
	if (clock_gettime (CLOCK_MONOTONIC, &c->ts) < 0) {
		return XERRNO;
	}
#elif HAS_MACH_TIME
	XCLOCK_SET_NSEC (c, (mach_absolute_time () * info.numer) / info.denom);
#else
	XCLOCK_SET_NSEC (c, clock () * (X_NSEC_PER_SEC / CLOCKS_PER_SEC));
#endif
	return 0;
}

double
xclock_diff (struct xclock *c)
{
	assert (c != NULL);

	struct xclock now;
	if (xclock_mono (&now) < 0) {
		return NAN;
	}
	XCLOCK_SUB (&now, c);
	return XCLOCK_TIME (&now);
}

double
xclock_step (struct xclock *c)
{
	assert (c != NULL);

	struct xclock now;
	if (xclock_mono (&now) < 0) {
		return NAN;
	}
	double time = XCLOCK_TIME (&now) - XCLOCK_TIME (c);
	*c = now;
	return time;
}

void
xclock_print (const struct xclock *c, FILE *out)
{
	if (out == NULL) {
		out = stdout;
	}

	if (c == NULL) {
		fprintf (out, "<crux:clock:(null)>\n");
	}
	else {
		fprintf (out, "<crux:clock:%p time=%f>\n", (void *)c, XCLOCK_TIME (c));
	}
}

