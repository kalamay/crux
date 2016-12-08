#include "mu.h"
#include "../include/crux/common.h"
#include "../include/crux/task.h"

static union xvalue
fib (void *data, union xvalue val)
{
	(void)data;
	(void)val;
	uint64_t a = 0, b = 1;
	while (true) {
		uint64_t r = a;
		a = b;
		b += r;
		xyield (XU64 (r));
	}
	return XZERO;
}

static union xvalue
fib3 (void *data, union xvalue val)
{
	(void)val;
	while (true) {
		xresume (data, XZERO);
		xresume (data, XZERO);
		xyield (xresume (data, XZERO));
	}
	return XZERO;
}

static void
test_fibonacci (void)
{
	static const uint64_t expect[10] = {
		1,
		5,
		21,
		89,
		377,
		1597,
		6765,
		28657,
		121393,
		514229
	};

	struct xtask *t1, *t2;
	uint64_t got[10];

	mu_assert_int_eq (xtask_new (&t1, fib, NULL), 0);
	mu_assert_int_eq (xtask_new (&t2, fib3, t1), 0);

	for (uint64_t n = 0; n < 10; n++) {
		uint64_t val = xresume (t2, XU64 (n)).u64;
		got[n] = val;
	}

	xtask_free (&t1);
	xtask_free (&t2);

	mu_assert_uint_eq (expect[0], got[0]);
	mu_assert_uint_eq (expect[1], got[1]);
	mu_assert_uint_eq (expect[2], got[2]);
	mu_assert_uint_eq (expect[3], got[3]);
	mu_assert_uint_eq (expect[4], got[4]);
	mu_assert_uint_eq (expect[5], got[5]);
	mu_assert_uint_eq (expect[6], got[6]);
	mu_assert_uint_eq (expect[7], got[7]);
	mu_assert_uint_eq (expect[8], got[8]);
	mu_assert_uint_eq (expect[9], got[9]);
}

static void
defer_count (void *ptr)
{
	int *n = ptr;
	*n = *n + 1;
}

static union xvalue
defer_coro (void *ptr, union xvalue val)
{
	xdefer (defer_count, ptr);
	xdefer (defer_count, ptr);
	xdefer (defer_count, ptr);
	return val;
}

static void
test_defer (void)
{
	int n = 0;

	struct xtask *t;
	mu_assert_int_eq (xtask_new (&t, defer_coro, &n), 0);
	xresume (t, XZERO);

	mu_assert (!xtask_alive (t))
	mu_assert_int_eq (n, 3);

	xtask_free (&t);
}

int
main (void)
{
	xtask_configure (X_STACK_DEFAULT, X_FDEBUG);

	mu_init ("task");
	mu_run (test_fibonacci);
	mu_run (test_defer);
}

