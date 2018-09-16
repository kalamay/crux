#include "mu.h"
#include "../include/crux/err.h"

static void
test_sys(void)
{
	int rc = xerr_sys(ENOBUFS);
	mu_assert_int_lt(rc, 0);
	mu_assert_int_eq(xerr_type(rc), XERR_SYS);
	mu_assert_int_eq(xerr_code(rc), ENOBUFS);
	mu_assert_int_eq(xerr_is_sys(rc), 1);
	mu_assert_int_eq(xerr_is_addr(rc), 0);
	mu_assert_int_eq(xerr_is_http(rc), 0);
}

static void
test_addr(void)
{
	int rc = xerr_addr(EAI_BADFLAGS);
	mu_assert_int_lt(rc, 0);
	mu_assert_int_eq(xerr_type(rc), XERR_ADDR);
	mu_assert_int_eq(xerr_code(rc), EAI_BADFLAGS);
	mu_assert_int_eq(xerr_is_sys(rc), 0);
	mu_assert_int_eq(xerr_is_addr(rc), 1);
	mu_assert_int_eq(xerr_is_http(rc), 0);
}

static void
test_http(void)
{
	int rc = xerr_http(TOOSHORT);
	mu_assert_int_lt(rc, 0);
	mu_assert_int_eq(xerr_type(rc), XERR_HTTP);
	mu_assert_int_eq(xerr_code(rc), XERR_HTTP_TOOSHORT);
	mu_assert_int_eq(xerr_is_sys(rc), 0);
	mu_assert_int_eq(xerr_is_addr(rc), 0);
	mu_assert_int_eq(xerr_is_http(rc), 1);
}

int
main(void)
{
	mu_init("err");

	test_sys();
	test_addr();
	test_http();
}

