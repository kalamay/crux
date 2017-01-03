#include "mu.h"
#include "../include/crux/err.h"

static void
test_sys (void)
{
	int rc = XESYS (ENOBUFS);
	mu_assert_int_lt (rc, 0);
	mu_assert_int_eq (XEISSYS (rc), 1);
	mu_assert_int_eq (XEISADDR (rc), 0);
	mu_assert_int_eq (XEISHTTP (rc), 0);
	mu_assert_int_eq (XETYPE (rc), XERR_SYS);
	mu_assert_int_eq (XECODE (rc), ENOBUFS);
}

static void
test_addr (void)
{
	int rc = XEADDR (EAI_BADFLAGS);
	mu_assert_int_lt (rc, 0);
	mu_assert_int_eq (XEISSYS (rc), 0);
	mu_assert_int_eq (XEISADDR (rc), 1);
	mu_assert_int_eq (XEISHTTP (rc), 0);
	mu_assert_int_eq (XETYPE (rc), XERR_ADDR);
	mu_assert_int_eq (XECODE (rc), EAI_BADFLAGS);
}

static void
test_http (void)
{
	int rc = XEHTTPTOOSHORT;
	mu_assert_int_lt (rc, 0);
	mu_assert_int_eq (XEISSYS (rc), 0);
	mu_assert_int_eq (XEISADDR (rc), 0);
	mu_assert_int_eq (XEISHTTP (rc), 1);
	mu_assert_int_eq (XETYPE (rc), XERR_HTTP);
	mu_assert_int_gt (XECODE (rc), 0);
}

int
main (void)
{
	mu_init ("err");

	test_sys ();
	test_addr ();
	test_http ();
}

