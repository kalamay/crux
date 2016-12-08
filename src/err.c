#include "../include/crux/err.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#if HAS_EXECINFO
# include <execinfo.h>
#endif

const char *
xerr_str (int code)
{
	return strerror (code < 0 ? -code : code);
}

static void
stack_abort (FILE *out)
{
	fflush (out);
#if HAS_EXECINFO
	void *calls[32];
	int frames = backtrace (calls, sizeof calls / sizeof calls[0]);
	backtrace_symbols_fd (calls, frames, fileno (out));
#endif
	abort ();
}

void
xerr_abort (int code)
{
	fprintf (stderr, "%s\n", xerr_str (code));
	stack_abort (stderr);
}

void
xerr_fabort (int code, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	va_end (ap);

	if (code != 0) {
		fprintf (stderr, ": %s", xerr_str (code));
	}
	fputc ('\n', stderr);

	stack_abort (stderr);
}

