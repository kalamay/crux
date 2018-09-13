#ifndef CRUX_VALUE_H
#define CRUX_VALUE_H

#include <stdint.h>

union xvalue {
	uint64_t u64;
	int64_t i64;
	double dbl;
	void *ptr;
	const void *cptr;
	int i;
};

#define XPTR(v)  ((union xvalue){ .ptr = (v) })
#define XCPTR(v) ((union xvalue){ .cptr = (v) })
#define XU64(v)  ((union xvalue){ .u64 = (v) })
#define XI64(v)  ((union xvalue){ .i64 = (v) })
#define XDBL(v)  ((union xvalue){ .dbl = (v) })
#define XINT(v)  ((union xvalue){ .i = (v) })
#define XZERO XU64(0)
#define XNULL XPTR(NULL)

#endif

