#ifndef CRUX_ERR_H
#define CRUX_ERR_H

#include <stdio.h>
#include <errno.h>

#define XERRNO (-errno)

extern const char *
xerr_str (int code);

extern void
xerr_abort (int code);

extern void __attribute__ ((format (printf, 2, 3)))
xerr_fabort (int code, const char *fmt, ...);

#define xerr_check(f) __extension__ ({ \
	int __code = (f); \
	if (__code < 0) { \
		xerr_fabort (__code, "%s:%d: '%s' failed", __FILE__, __LINE__, #f); \
	} \
	__code; \
})

#define xerrno_check(f) __extension__ ({ \
	int __code = (f); \
	if (__code < 0) { \
		xerr_fabort (errno, "%s:%d: '%s' failed", __FILE__, __LINE__, #f); \
	} \
	__code; \
})

#endif

