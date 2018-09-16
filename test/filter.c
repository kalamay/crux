#include "mu.h"
#include "../include/crux/filter.h"

static void
test_accept(void)
{
	struct xfilter *f;
	struct xfilter_err err;

	struct xfilter_expr expr[] = {
		{ "^foo", NULL, XFILTER_DOTALL },
		{ "^bar", "^ba[zt]", XFILTER_DOTALL },
	};

	mu_assert_int_eq(xfilter_new(&f, expr, xlen(expr), XFILTER_ACCEPT, &err), 0);

	mu_assert_int_eq(xfilter_key(f, "foo", 3), 1);
	mu_assert_int_eq(xfilter_key(f, "food", 4), 1);
	mu_assert_int_eq(xfilter_key(f, "sfood", 5), -1);
	mu_assert_int_eq(xfilter_key(f, "bar", 3), 2);

	mu_assert_int_eq(xfilter(f, "foo", 3, "bar", 3), 1);
	mu_assert_int_eq(xfilter(f, "food", 4, "bar", 3), 1);
	mu_assert_int_eq(xfilter(f, "bar", 3, "baz", 3), 2);
	mu_assert_int_eq(xfilter(f, "bar", 3, "bat", 3), 2);
	mu_assert_int_eq(xfilter(f, "bar", 3, "bats", 4), 2);
	mu_assert_int_eq(xfilter(f, "bar", 3, "bag", 3), -1);

	xfilter_free(&f);
}

static void
test_reject(void)
{
	struct xfilter *f;
	struct xfilter_err err;

	struct xfilter_expr expr[] = {
		{ "^foo", NULL, XFILTER_DOTALL },
		{ "^bar", "^ba[zt]", XFILTER_DOTALL },
	};

	mu_assert_int_eq(xfilter_new(&f, expr, xlen(expr), XFILTER_REJECT, &err), 0);

	mu_assert_int_eq(xfilter_key(f, "foo", 3), -1);
	mu_assert_int_eq(xfilter_key(f, "food", 4), -1);
	mu_assert_int_eq(xfilter_key(f, "sfood", 5), 0);
	mu_assert_int_eq(xfilter_key(f, "bar", 3), -1);

	mu_assert_int_eq(xfilter(f, "foo", 3, "bar", 3), -1);
	mu_assert_int_eq(xfilter(f, "food", 4, "bar", 3), -1);
	mu_assert_int_eq(xfilter(f, "bar", 3, "baz", 3), -1);
	mu_assert_int_eq(xfilter(f, "bar", 3, "bat", 3), -1);
	mu_assert_int_eq(xfilter(f, "bar", 3, "bats", 4), -1);
	mu_assert_int_eq(xfilter(f, "bar", 3, "bag", 3), 0);

	xfilter_free(&f);
}

static void
test_clone_accept(void)
{
	struct xfilter *f;
	struct xfilter *c;
	struct xfilter_err err;

	struct xfilter_expr expr[] = {
		{ "^foo", NULL, XFILTER_DOTALL },
		{ "^bar", "^ba[zt]", XFILTER_DOTALL },
	};

	mu_assert_int_eq(xfilter_new(&f, expr, xlen(expr), XFILTER_ACCEPT, &err), 0);
	mu_assert_int_eq(xfilter_clone(&c, f), 0);

	mu_assert_int_eq(xfilter_key(c, "foo", 3), 1);
	mu_assert_int_eq(xfilter_key(c, "food", 4), 1);
	mu_assert_int_eq(xfilter_key(c, "sfood", 5), -1);
	mu_assert_int_eq(xfilter_key(c, "bar", 3), 2);

	mu_assert_int_eq(xfilter(c, "foo", 3, "bar", 3), 1);
	mu_assert_int_eq(xfilter(c, "food", 4, "bar", 3), 1);
	mu_assert_int_eq(xfilter(c, "bar", 3, "baz", 3), 2);
	mu_assert_int_eq(xfilter(c, "bar", 3, "bat", 3), 2);
	mu_assert_int_eq(xfilter(c, "bar", 3, "bats", 4), 2);
	mu_assert_int_eq(xfilter(c, "bar", 3, "bag", 3), -1);

	xfilter_free(&c);
	xfilter_free(&f);
}

static void
test_clone_reject(void)
{
	struct xfilter *f;
	struct xfilter *c;
	struct xfilter_err err;

	struct xfilter_expr expr[] = {
		{ "^foo", NULL, XFILTER_DOTALL },
		{ "^bar", "^ba[zt]", XFILTER_DOTALL },
	};

	mu_assert_int_eq(xfilter_new(&f, expr, xlen(expr), XFILTER_REJECT, &err), 0);
	mu_assert_int_eq(xfilter_clone(&c, f), 0);

	mu_assert_int_eq(xfilter_key(c, "foo", 3), -1);
	mu_assert_int_eq(xfilter_key(c, "food", 4), -1);
	mu_assert_int_eq(xfilter_key(c, "sfood", 5), 0);
	mu_assert_int_eq(xfilter_key(c, "bar", 3), -1);

	mu_assert_int_eq(xfilter(c, "foo", 3, "bar", 3), -1);
	mu_assert_int_eq(xfilter(c, "food", 4, "bar", 3), -1);
	mu_assert_int_eq(xfilter(c, "bar", 3, "baz", 3), -1);
	mu_assert_int_eq(xfilter(c, "bar", 3, "bat", 3), -1);
	mu_assert_int_eq(xfilter(c, "bar", 3, "bats", 4), -1);
	mu_assert_int_eq(xfilter(c, "bar", 3, "bag", 3), 0);

	xfilter_free(&c);
	xfilter_free(&f);
}

int
main(void)
{
	mu_init("filter");

	mu_run(test_accept);
	mu_run(test_reject);
	mu_run(test_clone_accept);
	mu_run(test_clone_reject);
}

