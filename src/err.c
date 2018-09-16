#include "../include/crux/err.h"

#if WITH_HUB
# include "../include/crux/hub.h"
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if __APPLE__
# include <mach/mach_error.h>
#endif

/* Fallback definitions to simplify compiling.
 * These won't ever be used because they aren't defined (and thus not used)
 * in linux. These definitions just allow the x-macro to be much simpler.
 */
#if __linux
# ifndef EAI_BADHINTS
#  define EAI_BADHINTS -34
# endif
# ifndef EAI_PROTOCOL
#  define EAI_PROTOCOL -35
# endif
#else
# ifndef EAI_CANCELED
#  define EAI_CANCELED 33
# endif
#endif

#define XERR_TYPE_MAP(XX) \
	XX(SYS,               "system") \
	XX(ADDR,              "address information") \
	XX(KERN,              "kern") \
	XX(IO,                "io") \
	XX(HTTP,              "http") \

#define XERR_IO_MAP(XX) \
	XX(IO_CLOSE,          "file descriptor closed") \

#define XERR_HTTP_MAP(XX) \
	XX(HTTP_SYNTAX,       "http syntax invalid") \
	XX(HTTP_SIZE,         "http value size exceeded maximum allowed") \
	XX(HTTP_STATE,        "http parser state is invalid") \
	XX(HTTP_TOOSHORT,     "http input is too short") \

#if HAS_EXECINFO
# include <execinfo.h>
#endif


const char *
xerr_str_type(int code)
{
	switch (xerr_type(code)) {
#define XX(sym, msg) \
		case XERR_##sym: return msg;
	XERR_TYPE_MAP(XX)
#undef XX
	}
	return "invalid type";
}

const char *
xerr_str(int code)
{
	switch (xerr_type(code)) {
	case XERR_SYS: return strerror(xerr_code(code));;
	case XERR_ADDR: return gai_strerror(xerr_code(code));
	case XERR_KERN:
#if __APPLE__
		return mach_error_string(xerr_code(code));
#endif
		break;

#define XX(sym, msg) case XERR_##sym: return msg;
	case XERR_IO:
		switch (xerr_code(code)) {
			XERR_IO_MAP(XX)
		}
		break;
	case XERR_HTTP:
		switch (xerr_code(code)) {
			XERR_HTTP_MAP(XX)
		}
		break;
#undef XX
	}

	return "undefined error";
}

static void
stack_abort(void)
{
#if WITH_HUB
	xabort();
#else
#if HAS_EXECINFO
	void *calls[32];
	int frames = backtrace(calls, xlen(calls));
	backtrace_symbols_fd(calls, frames, STDERR_FILENO);
#endif
	fflush(stderr);
	abort();
#endif
}

void
xerr_abort(int code)
{
	fprintf(stderr, "%s\n", xerr_str(code));
	stack_abort();
}

void
xerr_fabort(int code, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (code != 0) {
		fprintf(stderr, ": %s", xerr_str(code));
	}
	fputc('\n', stderr);
	stack_abort();
}

void
xfail(const char *exp, const char *file, int line)
{
    fprintf(stderr, "%s:%u: failed assertion '%s'\n", file, line, exp);
	stack_abort();
}

