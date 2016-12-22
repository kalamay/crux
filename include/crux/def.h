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
	uintmax_t tmp = (n); \
	if (tmp > 0) { \
		tmp--; \
		tmp |= tmp >> 1; \
		tmp |= tmp >> 2; \
		tmp |= tmp >> 4; \
		tmp |= tmp >> 8; \
		tmp |= tmp >> 16; \
		tmp |= tmp >> 32; \
		tmp++; \
	} \
	tmp; \
})

XEXTERN const uint64_t XPOWER2_PRIMES[64];

#define xpower2_prime(n) __extension__ ({ \
	__typeof (n) tmp = (n); \
	tmp > 0 ? XPOWER2_PRIMES[63- __builtin_clzll(tmp)] : (uint64_t)0; \
})

#endif

