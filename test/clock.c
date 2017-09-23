#include "mu.h"
#include "../include/crux/clock.h"

static void
test_convert(void)
{
	int64_t a, b;

	a = 123;

	b = X_SEC_TO_NSEC(a);
	mu_assert_int_eq(b, 123000000000);
	mu_assert_int_eq(a, X_NSEC_TO_SEC(b));

	b = X_SEC_TO_USEC(a);
	mu_assert_int_eq(b, 123000000);
	mu_assert_int_eq(a, X_USEC_TO_SEC(b));

	b = X_SEC_TO_MSEC(a);
	mu_assert_int_eq(b, 123000);
	mu_assert_int_eq(a, X_MSEC_TO_SEC(b));

	b = X_SEC_TO_SEC(a);
	mu_assert_int_eq(b, 123);
	mu_assert_int_eq(a, X_SEC_TO_SEC(b));

	b = X_MSEC_TO_NSEC(a);
	mu_assert_int_eq(b, 123000000);
	mu_assert_int_eq(a, X_NSEC_TO_MSEC(b));

	b = X_MSEC_TO_USEC(a);
	mu_assert_int_eq(b, 123000);
	mu_assert_int_eq(a, X_USEC_TO_MSEC(b));

	b = X_USEC_TO_NSEC(a);
	mu_assert_int_eq(b, 123000);
	mu_assert_int_eq(a, X_NSEC_TO_USEC(b));

	b = X_NSEC_TO_NSEC(a);
	mu_assert_int_eq(b, 123);
	mu_assert_int_eq(a, X_NSEC_TO_NSEC(b));
}

static void
test_remainder(void)
{
	int64_t a, b;

	a = 100000023456;
	b = X_NSEC_REM(a);
	mu_assert_int_eq(b, 23456);

	a = 100023456;
	b = X_USEC_REM(a);
	mu_assert_int_eq(b, 23456000);

	a = 123456;
	b = X_MSEC_REM(a);
	mu_assert_int_eq(b, 456000000);
}

static void
test_make(void)
{
	struct xclock clock;

	clock = XCLOCK_MAKE_NSEC(123456789000);
	mu_assert_int_eq(clock.ts.tv_sec, 123);
	mu_assert_int_eq(clock.ts.tv_nsec, 456789000);

	clock = XCLOCK_MAKE_USEC(123456789);
	mu_assert_int_eq(clock.ts.tv_sec, 123);
	mu_assert_int_eq(clock.ts.tv_nsec, 456789000);

	clock = XCLOCK_MAKE_MSEC(123456);
	mu_assert_int_eq(clock.ts.tv_sec, 123);
	mu_assert_int_eq(clock.ts.tv_nsec, 456000000);
}

static void
test_get(void)
{
	struct xclock clock = XCLOCK_MAKE_NSEC(123456789000);

	mu_assert_int_eq(XCLOCK_NSEC(&clock), 123456789000);
	mu_assert_int_eq(XCLOCK_USEC(&clock), 123456789);
	mu_assert_int_eq(XCLOCK_MSEC(&clock), 123456);
	mu_assert_int_eq(XCLOCK_SEC(&clock), 123);
}

static void
test_set(void)
{
	struct xclock clock = XCLOCK_MAKE_NSEC(0);

	XCLOCK_SET_NSEC(&clock, 123456789000);
	mu_assert_int_eq(XCLOCK_NSEC(&clock), 123456789000);

	XCLOCK_SET_USEC(&clock, 123456789);
	mu_assert_int_eq(XCLOCK_NSEC(&clock), 123456789000);

	XCLOCK_SET_MSEC(&clock, 123456);
	mu_assert_int_eq(XCLOCK_NSEC(&clock), 123456000000);

	XCLOCK_SET_SEC(&clock, 123);
	mu_assert_int_eq(XCLOCK_NSEC(&clock), 123000000000);
}

static void
test_abs(void)
{
	struct xclock clock = XCLOCK_MAKE_NSEC(123456789000);

	int64_t abs;

	abs = 54321 + XCLOCK_NSEC(&clock);
	mu_assert_int_eq(abs, 123456843321);

	abs = 54321 + XCLOCK_USEC(&clock);
	mu_assert_int_eq(abs, 123511110);

	abs = 54321 + XCLOCK_MSEC(&clock);
	mu_assert_int_eq(abs, 177777);
}

static void
test_rel(void)
{
	struct xclock clock = XCLOCK_MAKE_NSEC(123456789000);

	int64_t rel;

	rel = 123456843321 - XCLOCK_NSEC(&clock);
	mu_assert_int_eq(rel, 54321);

	rel = 123511110 - XCLOCK_USEC(&clock);
	mu_assert_int_eq(rel, 54321);

	rel = 177777 - XCLOCK_MSEC(&clock);
	mu_assert_int_eq(rel, 54321);
}

static void
test_add(void)
{
	struct xclock clock, add;

	clock = XCLOCK_MAKE_NSEC(123456789000);
	add = XCLOCK_MAKE_NSEC(123456789000);
	XCLOCK_ADD(&clock, &add);
	mu_assert_int_eq(clock.ts.tv_sec, 246);
	mu_assert_int_eq(clock.ts.tv_nsec, 913578000);
}

static void
test_sub(void)
{
	struct xclock clock, sub;

	clock = XCLOCK_MAKE_NSEC(123456789000);
	sub = XCLOCK_MAKE_NSEC(123456789000);
	XCLOCK_SUB(&clock, &sub);
	mu_assert_int_eq(clock.ts.tv_sec, 0);
	mu_assert_int_eq(clock.ts.tv_nsec, 0);
}

static void
test_timeout(void)
{
	struct xtimeout t;
	struct xclock c, delay;

	XCLOCK_SET_MSEC(&delay, 50);
	xclock_mono(&c);

	xtimeout_start(&t, 200, &c);

	XCLOCK_ADD(&c, &delay);

	mu_assert_int_eq(xtimeout(&t, &c), 150);
}

int
main(void)
{
	mu_init("clock");
	mu_run(test_convert);
	mu_run(test_remainder);
	mu_run(test_make);
	mu_run(test_get);
	mu_run(test_set);
	mu_run(test_abs);
	mu_run(test_rel);
	mu_run(test_add);
	mu_run(test_sub);
	mu_run(test_timeout);
}

