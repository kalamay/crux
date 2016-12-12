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
	struct xtask **tp = data;
	while (true) {
		xresume (*tp, XZERO);
		xresume (*tp, XZERO);
		xyield (xresume (*tp, XZERO));
	}
	return val;
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

	mu_assert_int_eq (xtask_new_fn (&t1, fib), 0);
	mu_assert_int_eq (xtask_new_tls (&t2, &t1, 8, fib3), 0);

	for (uint64_t n = 0; n < 10; n++) {
		uint64_t val = xresume (t2, XZERO).u64;
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
defer_coro (void *tls, union xvalue val)
{
	(void)tls;
	xdefer (defer_count, val.ptr);
	xdefer (defer_count, val.ptr);
	xdefer (defer_count, val.ptr);
	return XZERO;
}

static void
test_defer (void)
{
	int n = 0;

	struct xtask *t;
	mu_assert_int_eq (xtask_new_fn (&t, defer_coro), 0);
	xresume (t, XPTR (&n));

	mu_assert (!xtask_alive (t))
	mu_assert_int_eq (n, 3);

	xtask_free (&t);
}

static void
defer_fib (void *ptr)
{
	struct xtask *t;
	mu_assert_int_eq (xtask_new_fn (&t, fib), 0);

	mu_assert_uint_eq (xresume (t, XZERO).u64, 0);
	mu_assert_uint_eq (xresume (t, XZERO).u64, 1);
	mu_assert_uint_eq (xresume (t, XZERO).u64, 1);
	mu_assert_uint_eq (xresume (t, XZERO).u64, 2);
	mu_assert_uint_eq (xresume (t, XZERO).u64, 3);
	mu_assert_uint_eq (xresume (t, XZERO).u64, 5);
	
	*(int *)ptr = 1;

	xtask_free (&t);
}

static union xvalue
defer_resume_coro (void *tls, union xvalue val)
{
	(void)tls;
	xdefer (defer_fib, val.ptr);
	return XZERO;
}

static void
test_defer_resume (void)
{
	struct xtask *t;
	int n = 0;

	mu_assert_int_eq (xtask_new_fn (&t, defer_resume_coro), 0);

	xresume (t, XPTR (&n));

	mu_assert (!xtask_alive (t))
	mu_assert_int_eq (n, 1);

	xtask_free (&t);
}

static union xvalue
doexit (void *ptr, union xvalue v)
{
	(void)ptr;
	(void)v;
	xyield (XINT (10));
	xyield (XINT (20));
	xtask_exit (NULL, 1);
	return XZERO;
}

static void
test_exit (void)
{
	struct xtask *t;
	mu_assert_int_eq (xtask_new_fn (&t, doexit), 0);

	union xvalue val;

	val = xresume (t, XZERO);
	mu_assert (xtask_alive (t));
	mu_assert_int_eq (xtask_exitcode (t), -1);
	mu_assert_int_eq (val.i, 10);

	val = xresume (t, XZERO);
	mu_assert (xtask_alive (t));
	mu_assert_int_eq (xtask_exitcode (t), -1);
	mu_assert_int_eq (val.i, 20);

	val = xresume (t, XZERO);
	mu_assert (!xtask_alive (t));
	mu_assert_int_eq (xtask_exitcode (t), 1);
	mu_assert_int_eq (val.i, 0);

	xtask_free (&t);
}

static void
test_exit_external (void)
{
	struct xtask *t;
	mu_assert_int_eq (xtask_new_fn (&t, doexit), 0);

	union xvalue val;

	val = xresume (t, XZERO);
	mu_assert (xtask_alive (t));
	mu_assert_int_eq (xtask_exitcode (t), -1);
	mu_assert_int_eq (val.i, 10);

	xtask_exit (t, 2);

	mu_assert (!xtask_alive (t));
	mu_assert_int_eq (xtask_exitcode (t), 2);

	xtask_free (&t);
}

static void
test_tls (void)
{
	struct xtask *t;

	mu_assert_int_eq (xtask_new_tls (&t, NULL, 24, doexit), 0);

	void *tls = xtask_local (t);
	mu_assert_ptr_ne (tls, NULL);
	mu_assert_uint_eq ((uintptr_t)tls % 16, 0);

	xtask_free (&t);
}

int
main (void)
{
	XTASK_FLAGS = XTASK_FDEBUG;

	mu_init ("task");
	mu_run (test_fibonacci);
	mu_run (test_defer);
	mu_run (test_defer_resume);
	mu_run (test_exit);
	mu_run (test_exit_external);
	mu_run (test_tls);
}

