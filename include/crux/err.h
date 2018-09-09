#ifndef CRUX_ERR_H
#define CRUX_ERR_H

#include "def.h"

#include <stdio.h>
#include <netdb.h>

enum xerr_type {
	XERR_SYS  = -1,
	XERR_ADDR = -2,
	XERR_HTTP = -3,
	XERR_KERN = -4,
};

union xerr {
	int value;
	struct { short code, type; };
};

#define XETYPE(n) (((union xerr){ .value=(n) }).type)
#define XECODE(n) (((union xerr){ .value=(n) }).code)

#define XEMAKE(T, n) (((union xerr){ .code=(n), .type=XERR_##T }).value)
#define XESYS(n) (XEMAKE(SYS, n))
#define XEADDR(n) (XEMAKE(ADDR, n))
#define XEHTTP(n) (XEMAKE(HTTP, n))
#define XEKERN(n) (XEMAKE(KERN, n))

#define XEIS(T, n) (XETYPE(n) == XERR_##T)
#define XEISSYS(n) (XEIS(SYS, n))
#define XEISADDR(n) (XEIS(ADDR, n))
#define XEISHTTP(n) (XEIS(HTTP, n))
#define XEISKERN(n) (XEIS(KERN, n))

#define XERRNO XESYS(errno)

#define XESYNTAX    1
#define XESIZE      2
#define XESTATE     3
#define XETOOSHORT  4
#define XEBUFS      5

#ifdef NDEBUG
# define xassert(e) ((void)0)
#else
# define xassert(e) \
	((void)((e) ? ((void)0) : xfail(#e, __FILE__, __LINE__)))
#endif

XEXTERN const char *
xerr_type(int code);

/**
 * @brief  Gets a string representation of an error code
 *
 * @param  code  error code in either positive or negative form
 * @return  string error message
 */
XEXTERN const char *
xerr_str(int code);

/**
 * @brief  Prints an error message and aborts
 *
 * This will print a stack trace if execinfo is available when compiled.
 *
 * @param  code  error code in either positive or negative form
 */
XEXTERN void
xerr_abort(int code);

/**
 * @brief  Prints a custom message followed by an  error message and aborts
 *
 * The user-supplied message will be printed with a trailing ": ", the error
 * message, and a newline. A stack trace will also be printed if execinfo is
 * available when compiled.
 *
 * @param  code  error code in either positive or negative form
 */
XEXTERN void __attribute__ ((format(printf, 2, 3)))
xerr_fabort(int code, const char *fmt, ...);

/**
 * @brief  Reports a failed assertion and aborts.
 *
 * @param  exp  failing assertion
 * @param  file  file path string
 * @param  line  file line number
 */
XEXTERN void
xfail(const char *exp, const char *file, int line);

/**
 * @brief  Checks the return code from a function and aborts on error
 *
 * This will print file and line information along with the error message.
 * A stack trace will also be printed if execinfo is available when compiled.
 *
 * @param  f  expression that results in a return code
 * @return  the return code from `f`
 */
#define xcheck(f) __extension__ ({ \
	int __code = (f); \
	if (__code < 0) { \
		xerr_fabort(__code, "%s:%d: '%s' failed", __FILE__, __LINE__, #f); \
	} \
	__code; \
})

/**
 * @brief  Checks the return code from a function and aborts using `errno`
 *
 * This will print file and line information along with the error message.
 * A stack trace will also be printed if execinfo is available when compiled.
 * Unline `xcheck`, this expects the error code to be in `errno`.
 *
 * @param  f  expression that results in a return code
 * @return  the return code from `f`
 */
#define xcheck_errno(f) __extension__ ({ \
	int __code = (f); \
	if (__code == -1) { \
		xerr_fabort(errno, "%s:%d: '%s' failed", __FILE__, __LINE__, #f); \
	} \
	__code; \
})

/**
 * @brief  Converts a -1 return value into an `XERRNO` result
 *
 * @param  f  expressoint that result in a return code
 * @return  the result from `f` or `XERRNO` if -1
 */
#define xerr(f) __extension__ ({ \
	int __code = (f); \
	if (__code == -1) { __code = XERRNO; } \
	__code; \
})

#endif

