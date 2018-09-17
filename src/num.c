#include "../include/crux/num.h"
#include "../include/crux/err.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

const uint64_t XPOWER2_PRIMES[64] = {
	1ULL, 2ULL, 3ULL,
	7ULL, 13ULL, 31ULL,
	61ULL, 127ULL, 251ULL,
	509ULL, 1021ULL, 2039ULL,
	4093ULL, 8191ULL, 16381ULL,
	32749ULL, 65521ULL, 131071ULL,
	262139ULL, 524287ULL, 1048573ULL,
	2097143ULL, 4194301ULL, 8388593ULL,
	16777213ULL, 33554393ULL, 67108859ULL,
	134217689ULL, 268435399ULL, 536870909ULL,
	1073741789ULL, 2147483647ULL, 4294967291ULL,
	8589934583ULL, 17179869143ULL, 34359738337ULL,
	68719476731ULL, 137438953447ULL, 274877906899ULL,
	549755813881ULL, 1099511627689ULL, 2199023255531ULL,
	4398046511093ULL, 8796093022151ULL, 17592186044399ULL,
	35184372088777ULL, 70368744177643ULL, 140737488355213ULL,
	281474976710597ULL, 562949953421231ULL, 1125899906842597ULL,
	2251799813685119ULL, 4503599627370449ULL, 9007199254740881ULL,
	18014398509481951ULL, 36028797018963913ULL, 72057594037927931ULL,
	144115188075855859ULL, 288230376151711717ULL, 576460752303423433ULL,
	1152921504606846883ULL, 2305843009213693951ULL, 4611686018427387847ULL,
	9223372036854775783ULL,
};

int
xto_bool(const char *restrict s, size_t len, bool *restrict val)
{
	assert(s != NULL);

	switch (len) {
	case 1:
		switch (*s) {
		case 't': case 'T': case 'y': case 'Y': case '1': *val = true; return 0;
		case 'f': case 'F': case 'n': case 'N': case '0': *val = false; return 0;
		}
		break;
	case 2:
		if (!strncasecmp(s, "on", 2)) { *val = true; return 0; }
		if (!strncasecmp(s, "no", 2)) { *val = false; return 0; }
		break;
	case 3:
		if (!strncasecmp(s, "yes", 3)) { *val = true; return 0; }
		if (!strncasecmp(s, "off", 3)) { *val = false; return 0; }
		break;
	case 4:
		if (!strncasecmp(s, "true", 4)) { *val = true; return 0; }
		break;
	case 5:
		if (!strncasecmp(s, "false", 5)) { *val = false; return 0; }
		break;
	}
	return xerr_sys(EINVAL);
}

int
xto_i64(const char *restrict s, size_t len, uint8_t base, int64_t *restrict val)
{
	assert (s != NULL);

	// detect and skip over '+' or '-'
	int64_t m = 1;
	if (len) {
		switch (*s) {
		case '-':
			m = -1;
			// fallthrough
		case '+':
			s++;
			len--;
			break;
		}
	}

	// parse as uint64 and the verify if it fits within an int64
	uint64_t p;
	int rc = xto_u64(s, len, base, &p);
	if (rc == 0) {
		// special case: INT64_MIN can't be turned into a positive int64
		if (m < 0 && p == ((uint64_t)INT64_MAX)+1) {
			*val = INT64_MIN;
		}
		else if (p > (uint64_t)INT64_MAX) {
			rc = xerr_sys(EOVERFLOW);
		}
		else {
			*val = m * (int64_t)p;
		}
	}
	return rc;
}

int
xto_u64(const char *restrict s, size_t len, uint8_t base, uint64_t *restrict val)
{
	assert(s != NULL);
	assert(base == 0 || (base >= 2 && base <= 36));

	if (base == 0) {
		if (len > 1 && *s == '0') {
			switch (s[1]) {
			case 'x': case 'X': s += 2; len -= 2; base = 16; break;
			case 'b': case 'B': s += 2; len -= 2; base = 2; break;
			default: s += 1; len -= 1; base = 8; break;
			}
		}
		else {
			base = 10;
		}
	}

	static const char map[] = {
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, // '0' - '9'
		99, 99, 99, 99, 99, 99, 99,             // invalid
		10, 11, 12, 13, 14, 15, 16, 17, 18, 19, // 'A' - 'J'
		20, 21, 22, 23, 24, 25, 26, 27, 28, 29, // 'K' - 'T'
		30, 31, 32, 33, 34, 35,                 // 'U' - 'Z'
		99, 99, 99, 99, 99, 99,                 // invalid
		10, 11, 12, 13, 14, 15, 16, 17, 18, 19, // 'a' - 'j'
		20, 21, 22, 23, 24, 25, 26, 27, 28, 29, // 'k' - 't'
		30, 31, 32, 33, 34, 35                  // 'u' - 'z'
	};

	uint64_t n = 0, bmax = UINT64_MAX/base;
	uint8_t i;
	while (len-- > 0) {
		// get next value and verify if its valid for the base
		i = *s++ - '0';
		if (i > 'z' - '0' || (i = map[i]) >= base) {
			return xerr_sys(EINVAL);
		}

		// check if next base multiplication would overflow
		if (n > bmax) {
			return xerr_sys(EOVERFLOW);
		}
		n *= base;

		// check if adding next value would overflow
		if (i > (UINT64_MAX-n)) {
			return xerr_sys(EOVERFLOW);
		}
		n += i;
	}
	*val = n;
	return 0;
}

#define to_conv(s, len, base, val, type, cmp, fulltype, conv) __extension__({ \
	fulltype p; \
	int rc = conv(s, len, base, &p); \
	if (rc == 0) { \
		if (cmp) { \
			*val = (type)p; \
		} \
		else { \
			rc = xerr_sys(EOVERFLOW); \
		} \
	} \
	rc; \
})

#define to_i(s, len, base, val, type, min, max) \
	to_conv(s, len, base, val, type, p >= min && p <= max, int64_t, xto_i64)

#define to_u(s, len, base, val, type, max) \
	to_conv(s, len, base, val, type, p <= max, uint64_t, xto_u64)

int
xto_int(const char *restrict s, size_t len, uint8_t base, int *restrict val)
{
	return to_i(s, len, base, val, int, INT_MIN, INT_MAX);
}

int
xto_i8(const char *restrict s, size_t len, uint8_t base, int8_t *restrict val)
{
	return to_i(s, len, base, val, int8_t, INT8_MIN, INT8_MAX);
}

int
xto_i16(const char *restrict s, size_t len, uint8_t base, int16_t *restrict val)
{
	return to_i(s, len, base, val, int16_t, INT16_MIN, INT16_MAX);
}

int
xto_i32(const char *restrict s, size_t len, uint8_t base, int32_t *restrict val)
{
	return to_i(s, len, base, val, int32_t, INT32_MIN, INT32_MAX);
}

int
xto_uint(const char *restrict s, size_t len, uint8_t base, unsigned *restrict val)
{
	return to_u(s, len, base, val, unsigned, UINT_MAX);
}

int
xto_u8(const char *restrict s, size_t len, uint8_t base, uint8_t *restrict val)
{
	return to_u(s, len, base, val, uint8_t, UINT8_MAX);
}

int
xto_u16(const char *restrict s, size_t len, uint8_t base, uint16_t *restrict val)
{
	return to_u(s, len, base, val, uint16_t, UINT16_MAX);
}

int
xto_u32(const char *restrict s, size_t len, uint8_t base, uint32_t *restrict val)
{
	return to_u(s, len, base, val, uint32_t, UINT32_MAX);
}

int
xto_double(const char *restrict s, size_t len, double *restrict val)
{
	char *end;
	double r = strtod(s, &end);
	if (end != s + len) {
		return xerr_sys(EINVAL);
	}
	*val = r;
	return 0;
}

int
xto_float(const char *restrict s, size_t len, float *restrict val)
{
	char *end;
	float r = strtof(s, &end);
	if (end != s + len) {
		return xerr_sys(EINVAL);
	}
	*val = r;
	return 0;
}

