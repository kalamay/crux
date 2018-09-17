#ifndef CRUX_NUM_H
#define CRUX_NUM_H

#include "def.h"

#define xpower2(n) __extension__ ({ \
	uintmax_t __p2 = (n); \
	if (__p2 > 0) { \
		__p2--; \
		__p2 |= __p2 >> 1; \
		__p2 |= __p2 >> 2; \
		__p2 |= __p2 >> 4; \
		__p2 |= __p2 >> 8; \
		__p2 |= __p2 >> 16; \
		__p2 |= __p2 >> 32; \
		__p2++; \
	} \
	__p2; \
})

XEXTERN const uint64_t XPOWER2_PRIMES[64];

#define xpower2_prime(n) __extension__ ({ \
	__typeof(n) __p2 = (n); \
	__p2 > 0 ? XPOWER2_PRIMES[63- __builtin_clzll(__p2)] : (uint64_t)0; \
})

#define xquantum(n, quant) __extension__ ({ \
	__typeof(n) __quant = (quant); \
	(((((n) - 1) / __quant) + 1) * __quant); \
})

XEXTERN int
xto_bool(const char *restrict s, size_t len, bool *restrict val);

XEXTERN int
xto_i64(const char *restrict s, size_t len, uint8_t base, int64_t *restrict val);

XEXTERN int
xto_u64(const char *restrict s, size_t len, uint8_t base, uint64_t *restrict val);

XEXTERN int
xto_int(const char *restrict s, size_t len, uint8_t base, int *restrict val);

XEXTERN int
xto_i8(const char *restrict s, size_t len, uint8_t base, int8_t *restrict val);

XEXTERN int
xto_i16(const char *restrict s, size_t len, uint8_t base, int16_t *restrict val);

XEXTERN int
xto_i32(const char *restrict s, size_t len, uint8_t base, int32_t *restrict val);

XEXTERN int
xto_uint(const char *restrict s, size_t len, uint8_t base, unsigned *restrict val);

XEXTERN int
xto_u8(const char *restrict s, size_t len, uint8_t base, uint8_t *restrict val);

XEXTERN int
xto_u16(const char *restrict s, size_t len, uint8_t base, uint16_t *restrict val);

XEXTERN int
xto_u32(const char *restrict s, size_t len, uint8_t base, uint32_t *restrict val);

XEXTERN int
xto_double(const char *restrict s, size_t len, double *restrict val);

XEXTERN int
xto_float(const char *restrict s, size_t len, float *restrict val);

#define xto(s, len, val) \
	_Generic((val),\
		    bool *: xto__bool, \
		  double *: xto__double, \
		   float *: xto__float, \
		  int8_t *: xto_i8, \
		 int16_t *: xto_i16, \
		 int32_t *: xto_i32, \
		 int64_t *: xto_i64, \
		 uint8_t *: xto_u8, \
		uint16_t *: xto_u16, \
		uint32_t *: xto_u32, \
		uint64_t *: xto_u64 \
	)(s, len, 0, val)

XSTATIC inline int
xto__bool(const char *restrict s, size_t len, uint8_t base, bool *restrict val)
{
	(void)base;
	return xto_bool(s, len, val);
}

XSTATIC inline int
xto__double(const char *restrict s, size_t len, uint8_t base, double *restrict val)
{
	(void)base;
	return xto_double(s, len, val);
}

XSTATIC inline int
xto__float(const char *restrict s, size_t len, uint8_t base, float *restrict val)
{
	(void)base;
	return xto_float(s, len, val);
}

#endif

