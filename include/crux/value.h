#ifndef CRUX_VALUE_H
#define CRUX_VALUE_H

union xvalue {
	uint64_t u64;
	int64_t i64;
	void *ptr;
	int i;
};

#define XPTR(v) ((union xvalue){ .ptr = (v) })
#define XU64(v) ((union xvalue){ .u64 = (v) })
#define XI64(v) ((union xvalue){ .i64 = (v) })
#define XINT(v) ((union xvalue){ .i = (v) })
#define XZERO XU64(0)

#endif

