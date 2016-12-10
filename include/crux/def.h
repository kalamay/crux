#ifndef CRUX_DEF_H
#define CRUX_DEF_H

#define xcontainer(ptr, type, member) __extension__ ({ \
	const __typeof (((type *)0)->member) *__mptr = (ptr); \
	(type *)(void *)((char *)__mptr - offsetof(type,member)); \
})

#define xlen(arr) \
	(sizeof (arr) / sizeof ((arr)[0]))

#endif

