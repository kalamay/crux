#ifndef CRUX_DEF_H
#define CRUX_DEF_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <errno.h>

#define XEXTERN __attribute__ ((visibility ("default"))) extern
#define XLOCAL  __attribute__ ((visibility ("hidden")))
#define XSTATIC __attribute__ ((unused)) static

#define xcontainer(ptr, type, member) __extension__ ({ \
	const __typeof (((type *)0)->member) *__mptr = (ptr); \
	(type *)(void *)((char *)__mptr - offsetof(type,member)); \
})

#define xlen(arr) \
	(sizeof (arr) / sizeof ((arr)[0]))

#define xlikely(x) __builtin_expect(!!(x), 1)
#define xunlikely(x) __builtin_expect(!!(x), 0)

#define xsym_concat(x, y) x ## y
#define xsym_make(x, y) xsym_concat(x, y)
#define xsym(n) xsym_make(sym__##n##_, __LINE__)

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
	__typeof (n) __p2 = (n); \
	__p2 > 0 ? XPOWER2_PRIMES[63- __builtin_clzll(__p2)] : (uint64_t)0; \
})

#define xquantum(n, quant) __extension__ ({ \
	__typeof (n) __quant = (quant); \
	(((((n) - 1) / __quant) + 1) * __quant); \
})

#endif

