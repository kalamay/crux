#ifndef CRUX_CLOCK_H
#define CRUX_CLOCK_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

/**
 * @brief  Transparent clock type
 *
 * This is simple clock that supports cross-platform monotonic and real time.
 */
struct xclock {
	struct timespec ts;
};

#define X_NSEC_PER_USEC 1000LL
#define X_NSEC_PER_MSEC 1000000LL
#define X_NSEC_PER_SEC  1000000000LL
#define X_USEC_PER_MSEC 1000LL
#define X_USEC_PER_SEC  1000000LL
#define X_MSEC_PER_SEC  1000LL

#define X_NSEC_TO_USEC(nsec) ((nsec) / X_NSEC_PER_USEC)
#define X_NSEC_TO_MSEC(nsec) ((nsec) / X_NSEC_PER_MSEC)
#define X_NSEC_TO_SEC(nsec)  ((nsec) / X_NSEC_PER_SEC)
#define X_USEC_TO_NSEC(usec) ((usec) * X_NSEC_PER_USEC)
#define X_USEC_TO_MSEC(usec) ((usec) / X_USEC_PER_MSEC)
#define X_USEC_TO_SEC(usec)  ((usec) / X_USEC_PER_SEC)
#define X_MSEC_TO_NSEC(usec) ((usec) * X_NSEC_PER_MSEC)
#define X_MSEC_TO_USEC(usec) ((usec) * X_USEC_PER_MSEC)
#define X_MSEC_TO_SEC(usec)  ((usec) / X_MSEC_PER_SEC)
#define X_SEC_TO_NSEC(sec)   ((sec)  * X_NSEC_PER_SEC)
#define X_SEC_TO_USEC(sec)   ((sec)  * X_USEC_PER_SEC)
#define X_SEC_TO_MSEC(sec)   ((sec)  * X_MSEC_PER_SEC)

#define X_NSEC_TO_NSEC(nsec)  (nsec)
#define X_SEC_TO_SEC(sec)     (sec)

#define X_NSEC_REM(nsec) (((nsec) % X_NSEC_PER_SEC))
#define X_USEC_REM(usec) (((usec) % X_USEC_PER_SEC) * X_NSEC_PER_USEC)
#define X_MSEC_REM(msec) (((msec) % X_MSEC_PER_SEC) * X_NSEC_PER_MSEC)
#define X_SEC_REM(sec)   0


#define XCLOCK_MAKE(val, n) \
	((struct xclock){{ X_##n##_TO_SEC (val), X_##n##_REM (val) }})

#define XCLOCK_MAKE_NSEC(nsec) XCLOCK_MAKE(nsec, NSEC)
#define XCLOCK_MAKE_USEC(usec) XCLOCK_MAKE(usec, USEC)
#define XCLOCK_MAKE_MSEC(msec) XCLOCK_MAKE(msec, MSEC)
#define XCLOCK_MAKE_SEC(sec)   XCLOCK_MAKE(sec,  SEC)


#define XCLOCK_GET(c, n) \
	((int64_t)(X_NSEC_TO_##n ((c)->ts.tv_nsec) + X_SEC_TO_##n ((c)->ts.tv_sec)))

#define XCLOCK_NSEC(c) XCLOCK_GET(c, NSEC)
#define XCLOCK_USEC(c) XCLOCK_GET(c, USEC)
#define XCLOCK_MSEC(c) XCLOCK_GET(c, MSEC)
#define XCLOCK_SEC(c)  XCLOCK_GET(c, SEC)


#define XCLOCK_SET(c, val, n) { \
	(c)->ts.tv_sec = X_##n##_TO_SEC (val); \
	(c)->ts.tv_nsec = X_##n##_REM (val); \
} while (0)

#define XCLOCK_SET_NSEC(c, nsec) XCLOCK_SET(c, nsec, NSEC)
#define XCLOCK_SET_USEC(c, usec) XCLOCK_SET(c, usec, USEC)
#define XCLOCK_SET_MSEC(c, msec) XCLOCK_SET(c, msec, MSEC)
#define XCLOCK_SET_SEC(c, sec)   XCLOCK_SET(c, sec,  SEC)


#define XCLOCK_TIME(c) \
	((double)(c)->ts.tv_sec + 1e-9 * (c)->ts.tv_nsec)
    
#define XCLOCK_SET_TIME(c, time) do { \
	double sec; \
	(c)->ts.tv_nsec = round (modf ((time), &sec) * X_NSEC_PER_SEC); \
	(c)->ts.tv_sec = sec; \
} while (0)


#define XCLOCK_ADD(c, v) do { \
	(c)->ts.tv_sec += (v)->ts.tv_sec; \
	(c)->ts.tv_nsec += (v)->ts.tv_nsec; \
	if ((c)->ts.tv_nsec >= X_NSEC_PER_SEC) { \
		(c)->ts.tv_sec++; \
		(c)->ts.tv_nsec -= X_NSEC_PER_SEC; \
	} \
} while (0)

#define XCLOCK_SUB(c, v) do { \
	(c)->ts.tv_sec -= (v)->ts.tv_sec; \
	(c)->ts.tv_nsec -= (v)->ts.tv_nsec; \
	if ((c)->ts.tv_nsec < 0LL) { \
		(c)->ts.tv_sec--; \
		(c)->ts.tv_nsec += X_NSEC_PER_SEC; \
	} \
} while (0)


#define XCLOCK_CMP(c, v, op) ( \
	(c)->ts.tv_sec == (v)->ts.tv_sec \
		? (c)->ts.tv_nsec op (v)->ts.tv_nsec \
		: (c)->ts.tv_sec op (v)->ts.tv_sec)

#define XCLOCK_LT(c, v) XCLOCK_CMP(c, v <)
#define XCLOCK_LE(c, v) XCLOCK_CMP(c, v <=)
#define XCLOCK_GT(c, v) XCLOCK_CMP(c, v >)
#define XCLOCK_GE(c, v) XCLOCK_CMP(c, v >=)


/**
 * @brief  Updates the clock to the current real time
 *
 * @param  c  clock pointer
 * @return  0 on succes, -errno on error
 */
extern int
xclock_real (struct xclock *c);

/**
 * @brief  Updates the clock to the current monotonic time
 *
 * @param  c  clock pointer
 * @return  0 on succes, -errno on error
 */
extern int
xclock_mono (struct xclock *c);

/**
 * @brief  Calculates the current monotonic time delta from the clock
 *
 * The clock must be initialized with `xclock_mono`.
 *
 * @param  c  clock pointer
 * @return  delta time in seconds, NAN on error
 */
extern double
xclock_diff (struct xclock *c);

/**
 * @brief  Update the clock to current monotonic time an returns the delta
 *
 * The clock must be initialized with `xclock_mono`.
 *
 * @param  c  clock pointer
 * @return  delta time in seconds, NAN on error
 */
extern double
xclock_step (struct xclock *c);

#endif

