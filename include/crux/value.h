#ifndef CRUX_VALUE_H
#define CRUX_VALUE_H

#include <stdint.h>

union xvalue
{
	uint64_t u64;
	int64_t i64;
	double dbl;
	void *ptr;
	const void *cptr;
	int i;
};

#define xptr(v)  ((union xvalue){ .ptr = (v) })
#define xcptr(v) ((union xvalue){ .cptr = (v) })
#define xu64(v)  ((union xvalue){ .u64 = (v) })
#define xi64(v)  ((union xvalue){ .i64 = (v) })
#define xdbl(v)  ((union xvalue){ .dbl = (v) })
#define xint(v)  ((union xvalue){ .i = (v) })
#define xzero xu64(0)
#define xnil xptr(NULL)

#endif

