#ifndef CRUX_DEF_H
#define CRUX_DEF_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <errno.h>

#define XEXTERN __attribute__ ((visibility("default"))) extern
#define XLOCAL  __attribute__ ((visibility("hidden")))
#define XSTATIC __attribute__ ((unused)) static

#ifndef thread_local
# ifdef __declspec
#  define thread_local __declspec(thread)
# else
#  define thread_local __thread
# endif
#endif

#define xcontainer(ptr, type, member) __extension__ ({ \
	const __typeof(((type *)0)->member) *__mptr = (ptr); \
	(type *)(void *)((char *)__mptr - offsetof(type,member)); \
})

#define xlen(arr) \
	(sizeof(arr) / sizeof((arr)[0]))

#define xlikely(x) __builtin_expect(!!(x), 1)
#define xunlikely(x) __builtin_expect(!!(x), 0)

#define xsym_concat(x, y) x ## y
#define xsym_make(x, y) xsym_concat(x, y)
#define xsym(n) xsym_make(sym__##n##_, __LINE__)

#ifdef TEMP_FAILURE_RETRY
# define xretry TEMP_FAILURE_RETRY
#else
# define xretry(exp) __extension__ ({ \
	int __code; \
	do { __code = (exp); } \
	while (__code == -1 && errno == EINTR); \
	__code; \
})
#endif

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

XEXTERN size_t xpagesize;

#define xpagetrunc(n) ((n) & (~(xpagesize - 1)))
#define xpageround(n) xpagetrunc((n) + (xpagesize - 1))

#define xnew(init, ptr, ...) __extension__ ({ \
	void *p = malloc(sizeof(**ptr)); \
	int rc = p ? init(p, ##__VA_ARGS__) : xerrno; \
	if (rc < 0) { free(p); } \
	else { *ptr = p; } \
	rc; \
})

#define xfree(fini, ptr) do { \
	void *p = *ptr; \
	if (p != NULL) { \
		*ptr = NULL; \
		fini(p); \
		free(p); \
	} \
} while (0)

#endif

