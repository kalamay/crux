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
test_chain_or(void)
{
	struct xfilter *a, *b, *f;
	struct xfilter_err err;

	struct xfilter_expr expra[] = {
		{ "^x-", NULL, XFILTER_DOTALL },
	};

	struct xfilter_expr exprb[] = {
		{ "^x-foo", "^ba[zt]", XFILTER_DOTALL },
		{ "^[a-z]{2}", NULL, XFILTER_DOTALL },
	};

	mu_assert_int_eq(xfilter_new(&a, expra, xlen(expra), XFILTER_REJECT, &err), 0);
	mu_assert_int_eq(xfilter_new(&b, exprb, xlen(exprb), XFILTER_ACCEPT, &err), 0);

	struct xfilter *chain[2] = { a, b };
	mu_assert_int_eq(xfilter_chain(&f, chain, xlen(chain), XFILTER_CHAIN_OR), 0);
	xfilter_free(&a);
	xfilter_free(&b);

	mu_assert_int_eq(xfilter_key(f, "foo", 3), 0);
	mu_assert_int_eq(xfilter_key(f, "x-foo", 5), 0);
	mu_assert_int_eq(xfilter_key(f, "x-bar", 5), -1);

	mu_assert_int_eq(xfilter(f, "foo", 3, "bar", 3), 0);
	mu_assert_int_eq(xfilter(f, "x-foo", 5, "bar", 3), -1);
	mu_assert_int_eq(xfilter(f, "x-bar", 5, "bar", 3), -1);
	mu_assert_int_eq(xfilter(f, "x-foo", 5, "baz", 3), 0);
	mu_assert_int_eq(xfilter(f, "x-foo", 5, "bat", 3), 0);
	mu_assert_int_eq(xfilter(f, "x-foo", 5, "bats", 4), 0);

	xfilter_free(&f);
}

static void
test_chain_and(void)
{
	struct xfilter *a, *b, *f;
	struct xfilter_err err;

	struct xfilter_expr expra[] = {
		{ "^x-", NULL, XFILTER_DOTALL },
	};

	struct xfilter_expr exprb[] = {
		{ "bar$", NULL, XFILTER_DOTALL },
		{ "bag$", "^ba[zt]", XFILTER_DOTALL },
	};

	mu_assert_int_eq(xfilter_new(&a, expra, xlen(expra), XFILTER_ACCEPT, &err), 0);
	mu_assert_int_eq(xfilter_new(&b, exprb, xlen(exprb), XFILTER_REJECT, &err), 0);

	struct xfilter *chain[2] = { a, b };
	mu_assert_int_eq(xfilter_chain(&f, chain, xlen(chain), XFILTER_CHAIN_AND), 0);
	xfilter_free(&a);
	xfilter_free(&b);

	mu_assert_int_eq(xfilter_key(f, "foo", 3), -1);
	mu_assert_int_eq(xfilter_key(f, "x-foo", 5), 0);
	mu_assert_int_eq(xfilter_key(f, "x-bar", 5), -1);
	mu_assert_int_eq(xfilter_key(f, "x-bag", 5), -1);

	mu_assert_int_eq(xfilter(f, "foo", 3, "baz", 3), -1);
	mu_assert_int_eq(xfilter(f, "bar", 3, "baz", 3), -1);
	mu_assert_int_eq(xfilter(f, "x-foo", 5, "baz", 3), 0);
	mu_assert_int_eq(xfilter(f, "x-bar", 5, "baz", 3), -1);
	mu_assert_int_eq(xfilter(f, "x-bag", 5, "baz", 3), -1);
	mu_assert_int_eq(xfilter(f, "x-bag", 5, "bat", 3), -1);
	mu_assert_int_eq(xfilter(f, "x-bag", 5, "bats", 4), -1);
	mu_assert_int_eq(xfilter(f, "x-bag", 5, "bay", 3), 0);

	xfilter_free(&f);
}

static void
test_clone_accept(void)
{
	struct xfilter *f, *c;
	struct xfilter_err err;

	struct xfilter_expr expr[] = {
		{ "^foo", NULL, XFILTER_DOTALL },
		{ "^bar", "^ba[zt]", XFILTER_DOTALL },
	};

	mu_assert_int_eq(xfilter_new(&f, expr, xlen(expr), XFILTER_ACCEPT, &err), 0);
	mu_assert_int_eq(xfilter_clone(&c, f), 0);
	xfilter_free(&f);

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
}

static void
test_clone_reject(void)
{
	struct xfilter *f, *c;
	struct xfilter_err err;

	struct xfilter_expr expr[] = {
		{ "^foo", NULL, XFILTER_DOTALL },
		{ "^bar", "^ba[zt]", XFILTER_DOTALL },
	};

	mu_assert_int_eq(xfilter_new(&f, expr, xlen(expr), XFILTER_REJECT, &err), 0);
	mu_assert_int_eq(xfilter_clone(&c, f), 0);
	xfilter_free(&f);

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
}

static void
test_clone_chain_or(void)
{
	struct xfilter *a, *b, *f, *c;
	struct xfilter_err err;

	struct xfilter_expr expra[] = {
		{ "^x-", NULL, XFILTER_DOTALL },
	};

	struct xfilter_expr exprb[] = {
		{ "^x-foo", "^ba[zt]", XFILTER_DOTALL },
		{ "^[a-z]{2}", NULL, XFILTER_DOTALL },
	};

	mu_assert_int_eq(xfilter_new(&a, expra, xlen(expra), XFILTER_REJECT, &err), 0);
	mu_assert_int_eq(xfilter_new(&b, exprb, xlen(exprb), XFILTER_ACCEPT, &err), 0);

	struct xfilter *chain[2] = { a, b };
	mu_assert_int_eq(xfilter_chain(&f, chain, xlen(chain), XFILTER_CHAIN_OR), 0);
	xfilter_free(&a);
	xfilter_free(&b);
	mu_assert_int_eq(xfilter_clone(&c, f), 0);
	xfilter_free(&f);

	mu_assert_int_eq(xfilter_key(c, "foo", 3), 0);
	mu_assert_int_eq(xfilter_key(c, "x-foo", 5), 0);
	mu_assert_int_eq(xfilter_key(c, "x-bar", 5), -1);

	mu_assert_int_eq(xfilter(c, "foo", 3, "bar", 3), 0);
	mu_assert_int_eq(xfilter(c, "x-foo", 5, "bar", 3), -1);
	mu_assert_int_eq(xfilter(c, "x-bar", 5, "bar", 3), -1);
	mu_assert_int_eq(xfilter(c, "x-foo", 5, "baz", 3), 0);
	mu_assert_int_eq(xfilter(c, "x-foo", 5, "bat", 3), 0);
	mu_assert_int_eq(xfilter(c, "x-foo", 5, "bats", 4), 0);

	xfilter_free(&c);
}

static void
test_clone_chain_and(void)
{
	struct xfilter *a, *b, *f, *c;
	struct xfilter_err err;

	struct xfilter_expr expra[] = {
		{ "^x-", NULL, XFILTER_DOTALL },
	};

	struct xfilter_expr exprb[] = {
		{ "bar$", NULL, XFILTER_DOTALL },
		{ "bag$", "^ba[zt]", XFILTER_DOTALL },
	};

	mu_assert_int_eq(xfilter_new(&a, expra, xlen(expra), XFILTER_ACCEPT, &err), 0);
	mu_assert_int_eq(xfilter_new(&b, exprb, xlen(exprb), XFILTER_REJECT, &err), 0);

	struct xfilter *chain[2] = { a, b };
	mu_assert_int_eq(xfilter_chain(&f, chain, xlen(chain), XFILTER_CHAIN_AND), 0);
	xfilter_free(&a);
	xfilter_free(&b);
	mu_assert_int_eq(xfilter_clone(&c, f), 0);
	xfilter_free(&f);

	mu_assert_int_eq(xfilter_key(c, "foo", 3), -1);
	mu_assert_int_eq(xfilter_key(c, "x-foo", 5), 0);
	mu_assert_int_eq(xfilter_key(c, "x-bar", 5), -1);
	mu_assert_int_eq(xfilter_key(c, "x-bag", 5), -1);

	mu_assert_int_eq(xfilter(c, "foo", 3, "baz", 3), -1);
	mu_assert_int_eq(xfilter(c, "bar", 3, "baz", 3), -1);
	mu_assert_int_eq(xfilter(c, "x-foo", 5, "baz", 3), 0);
	mu_assert_int_eq(xfilter(c, "x-bar", 5, "baz", 3), -1);
	mu_assert_int_eq(xfilter(c, "x-bag", 5, "baz", 3), -1);
	mu_assert_int_eq(xfilter(c, "x-bag", 5, "bat", 3), -1);
	mu_assert_int_eq(xfilter(c, "x-bag", 5, "bats", 4), -1);
	mu_assert_int_eq(xfilter(c, "x-bag", 5, "bay", 3), 0);

	xfilter_free(&c);
}

int
main(void)
{
	mu_init("filter");

	mu_run(test_accept);
	mu_run(test_reject);
	mu_run(test_chain_or);
	mu_run(test_chain_and);
	mu_run(test_clone_accept);
	mu_run(test_clone_reject);
	mu_run(test_clone_chain_or);
	mu_run(test_clone_chain_and);
}

