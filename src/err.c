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
	XX(HTTP,              "http") \
	XX(KERN,              "kern") \

#define XERR_IO_MAP(XX) \
	XX(CLOSE,             "file descriptor closed") \

#define XERR_PARSER_MAP(type, XX) \
	XX(SYNTAX,             type "syntax invalid") \
	XX(SIZE,               type "value size exceeded maximum allowed") \
	XX(STATE,              type "parser state is invalid") \
	XX(TOOSHORT,           type "input is too short") \

#define XERR_HTTP_MAP(XX) \
	XERR_PARSER_MAP("http", XX)

#if HAS_EXECINFO
# include <execinfo.h>
#endif


const char *
xerr_type(int code)
{
	switch (XETYPE(code)) {
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
	if (code > 0) { code = -code; }
	switch (XETYPE(code)) {
	case XERR_SYS: return strerror(XECODE(code));;
	case XERR_ADDR: return gai_strerror(XECODE(code));
	case XERR_KERN:
#if __APPLE__
		return mach_error_string(XECODE(code));
#endif
		break;
	case XERR_IO:
		switch (code) {
#define XX(sym, msg) \
			case XE##sym: return msg;
	XERR_IO_MAP(XX)
#undef XX
		}
		break;
	case XERR_HTTP:
		switch (code) {
#define XX(sym, msg) \
			case XE##sym: return msg;
	XERR_HTTP_MAP(XX)
#undef XX
		}
		break;
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

