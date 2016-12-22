#ifndef CRUX_ERR_H
#define CRUX_ERR_H

#include "def.h"

#include <stdio.h>

#define XERRNO (-errno)

/**
 * @brief  Gets a string representation of an error code
 *
 * @param  code  error code in either positive or negative form
 * @return  string error message
 */
XEXTERN const char *
xerr_str (int code);

/**
 * @brief  Prints an error message and aborts
 *
 * This will print a stack trace if execinfo is available when compiled.
 *
 * @param  code  error code in either positive or negative form
 */
XEXTERN void
xerr_abort (int code);

/**
 * @brief  Prints a custom message followed by an  error message and aborts
 *
 * The user-supplied message will be printed with a trailing ": ", the error
 * message, and a newline. A stack trace will also be printed if execinfo is
 * available when compiled.
 *
 * @param  code  error code in either positive or negative form
 */
XEXTERN void __attribute__ ((format (printf, 2, 3)))
xerr_fabort (int code, const char *fmt, ...);

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
		xerr_fabort (__code, "%s:%d: '%s' failed", __FILE__, __LINE__, #f); \
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
	if (__code < 0) { \
		xerr_fabort (errno, "%s:%d: '%s' failed", __FILE__, __LINE__, #f); \
	} \
	__code; \
})

#endif

