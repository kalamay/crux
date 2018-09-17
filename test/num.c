#include "mu.h"
#include "../include/crux/num.h"
#include "../include/crux/err.h"

#define parse(s, out) xto(s, sizeof (s)-1, out)

void
test_fail_int(void)
{
	int rc;
	int64_t val;

	rc = parse("123.456", &val);
	mu_assert_int_eq(rc, xerr_sys(EINVAL));
}

void
test_double(void)
{
	int rc;
	double val;

	rc = parse("123.456", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_flt_eq(val, 123.456);
}

void
test_int64(void)
{
	int rc;
	int64_t val;

	rc = parse("-9223372036854775808", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, INT64_MIN);

	rc = parse("9223372036854775807", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, INT64_MAX);

	rc = parse("-9223372036854775809", &val);
	mu_assert_int_eq(rc, xerr_sys(EOVERFLOW));

	rc = parse("9223372036854775808", &val);
	mu_assert_int_eq(rc, xerr_sys(EOVERFLOW));
}

void
test_uint64(void)
{
	int rc;
	uint64_t val;

	rc = parse("0", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, 0);

	rc = parse("18446744073709551615", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, UINT64_MAX);

	rc = parse("18446744073709551616", &val);
	mu_assert_int_eq(rc, xerr_sys(EOVERFLOW));

	rc = parse("-1", &val);
	mu_assert_int_eq(rc, xerr_sys(EINVAL));
}

void
test_base(void)
{
	int rc;
	uint32_t val;

	rc = parse("0xFF", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, 0xFF);

	rc = parse("0XABCD", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, 0xABCD);

	rc = parse("0664", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, 0664);

	rc = parse("0b1101", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, 13);

	rc = parse("0B11011110101011011011111011101111", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, 0xDEADBEEF);
}

void
test_bool(void)
{
	int rc;
	bool val;

	rc = parse("true", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, true);

	rc = parse("false", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, false);

	rc = parse("yes", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, true);

	rc = parse("no", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, false);

	rc = parse("on", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, true);

	rc = parse("off", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, false);

	rc = parse("T", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, true);

	rc = parse("F", &val);
	mu_assert_int_eq(rc, 0);
	mu_assert_int_eq(val, false);
}

int
main (void)
{
	mu_init("num");

	mu_run(test_fail_int);
	mu_run(test_double);
	mu_run(test_int64);
	mu_run(test_uint64);
	mu_run(test_base);
	mu_run(test_bool);
}
