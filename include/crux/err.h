#ifndef CRUX_ERR_H
#define CRUX_ERR_H

#include "def.h"

#include <stdio.h>
#include <netdb.h>

#define xerr_make_type(n) ((int32_t)((((uint32_t)(n))&0x7fff)<<16))
#define xerr_make(type, code) (-((int32_t)(type | (((uint16_t)code) & 0xffff))))

enum xerr_type
{
	XERR_SYS  = xerr_make_type(0),
	XERR_ADDR = xerr_make_type(1),
	XERR_KERN = xerr_make_type(2),
	XERR_IO   = xerr_make_type(3),
	XERR_HTTP = xerr_make_type(4),
};

#define xerr_type(n) ((enum xerr_type)(((uint32_t)-((int32_t)n)) & 0x7fff0000))
#define xerr_code(n) ((int16_t)(((uint32_t)-((int32_t)n)) & 0xffff))
#define xerr_is(type, n) (xerr_type(n) == type)

#define xerr_sys(n)  xerr_make(XERR_SYS, n)
#define xerr_addr(n) xerr_make(XERR_ADDR, n)
#define xerr_kern(n) xerr_make(XERR_KERN, n)
#define xerr_io(n)   xerr_make(XERR_IO, XERR_IO_##n)
#define xerr_http(n) xerr_make(XERR_HTTP, XERR_HTTP_##n)

#define xerr_is_sys(n)  xerr_is(XERR_SYS, n)
#define xerr_is_addr(n) xerr_is(XERR_ADDR, n)
#define xerr_is_kern(n) xerr_is(XERR_KERN, n)
#define xerr_is_io(n)   xerr_is(XERR_IO, n)
#define xerr_is_http(n) xerr_is(XERR_HTTP, n)

#define xerrno xerr_sys(errno)

enum xerr_io_code
{
	XERR_IO_CLOSE,
};

enum xerr_http_code
{
	XERR_HTTP_SYNTAX,
	XERR_HTTP_SIZE,
	XERR_HTTP_STATE,
	XERR_HTTP_TOOSHORT,
};

#ifdef NDEBUG
# define xassert(e) ((void)0)
#else
# define xassert(e) \
	((void)((e) ? ((void)0) : xfail(#e, __FILE__, __LINE__)))
#endif

XEXTERN const char *
xerr_str_type(int code);

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
 * @brief  Converts a -1 return value into an `xerrno` result
 *
 * @param  f  expressoint that result in a return code
 * @return  the result from `f` or `xerrno` if -1
 */
#define xerr(f) __extension__ ({ \
	int __code = (f); \
	if (__code == -1) { __code = xerrno; } \
	__code; \
})

#endif

