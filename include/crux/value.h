#ifndef CRUX_VALUE_H
#define CRUX_VALUE_H

#include <stdint.h>

union xvalue
{
	char ch;
	bool bln;
	uint8_t u8;
	int8_t i8;
	uint16_t u16;
	int16_t i16;
	uint32_t u32;
	int32_t i32;
	uint64_t u64;
	int64_t i64;
	float flt;
	double dbl;
	void *ptr;
	const void *cptr;
	int i;
};

#define xchar(v)   ((union xvalue){ .ch = (v) })
#define xbool(v)   ((union xvalue){ .bln = (v) })
#define xu8(v)     ((union xvalue){ .u8 = (v) })
#define xi8(v)     ((union xvalue){ .i8 = (v) })
#define xu16(v)    ((union xvalue){ .u16 = (v) })
#define xi16(v)    ((union xvalue){ .i16 = (v) })
#define xu32(v)    ((union xvalue){ .u32 = (v) })
#define xi32(v)    ((union xvalue){ .i32 = (v) })
#define xu64(v)    ((union xvalue){ .u64 = (v) })
#define xi64(v)    ((union xvalue){ .i64 = (v) })
#define xfloat(v)  ((union xvalue){ .flt = (v) })
#define xdouble(v) ((union xvalue){ .dbl = (v) })
#define xptr(v)    ((union xvalue){ .ptr = (v) })
#define xcptr(v)   ((union xvalue){ .cptr = (v) })
#define xint(v)    ((union xvalue){ .i = (v) })
#define xzero xu64(0)
#define xnil xptr(NULL)
#define xtrue xbool(true)
#define xfalse xbool(false)

#endif

