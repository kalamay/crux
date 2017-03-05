#include "../include/crux/err.h"

#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

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

#define XERR_SYS_MAP(XX) \
	XX(EPERM,              "operation not permitted") \
	XX(ENOENT,             "no such file or directory") \
	XX(ESRCH,              "no such process") \
	XX(EINTR,              "interrupted system call") \
	XX(EIO,                "i/o error") \
	XX(ENXIO,              "no such device or address") \
	XX(E2BIG,              "argument list too long") \
	XX(EBADF,              "bad file descriptor") \
	XX(ENOMEM,             "not enough memory") \
	XX(EACCES,             "permission denied") \
	XX(EFAULT,             "bad address in system call argument") \
	XX(EBUSY,              "resource busy or locked") \
	XX(EEXIST,             "file already exists") \
	XX(EXDEV,              "cross-device link not permitted") \
	XX(ENODEV,             "no such device") \
	XX(ENOTDIR,            "not a directory") \
	XX(EISDIR,             "illegal operation on a directory") \
	XX(EINVAL,             "invalid argument") \
	XX(ENFILE,             "file table overflow") \
	XX(EMFILE,             "too many open files") \
	XX(ETXTBSY,            "text file is busy") \
	XX(EFBIG,              "file too large") \
	XX(ENOSPC,             "no space left on device") \
	XX(ESPIPE,             "invalid seek") \
	XX(EROFS,              "read-only file system") \
	XX(EMLINK,             "too many links") \
	XX(EPIPE,              "broken pipe") \
	XX(ERANGE,             "result too large") \
	XX(EAGAIN,             "resource temporarily unavailable") \
	XX(EALREADY,           "connection already in progress") \
	XX(ENOTSOCK,           "socket operation on non-socket") \
	XX(EDESTADDRREQ,       "destination address required") \
	XX(EMSGSIZE,           "message too long") \
	XX(EPROTOTYPE,         "protocol wrong type for socket") \
	XX(ENOPROTOOPT,        "protocol not available") \
	XX(EPROTONOSUPPORT,    "protocol not supported") \
	XX(ENOTSUP,            "operation not supported on socket") \
	XX(EAFNOSUPPORT,       "address family not supported") \
	XX(EADDRINUSE,         "address already in use") \
	XX(EADDRNOTAVAIL,      "address not available") \
	XX(ENETDOWN,           "network is down") \
	XX(ENETUNREACH,        "network is unreachable") \
	XX(ECONNABORTED,       "software caused connection abort") \
	XX(ECONNRESET,         "connection reset by peer") \
	XX(ENOBUFS,            "no buffer space available") \
	XX(EISCONN,            "socket is already connected") \
	XX(ENOTCONN,           "socket is not connected") \
	XX(ESHUTDOWN,          "cannot send after transport endpoint shutdown") \
	XX(ETIMEDOUT,          "connection timed out") \
	XX(ECONNREFUSED,       "connection refused") \
	XX(ELOOP,              "too many symbolic links encountered") \
	XX(ENAMETOOLONG,       "name too long") \
	XX(EHOSTDOWN,          "host is down") \
	XX(EHOSTUNREACH,       "host is unreachable") \
	XX(ENOTEMPTY,          "directory not empty") \
	XX(ENOSYS,             "function not implemented") \
	XX(ECANCELED,          "operation canceled") \
	XX(EPROTO,             "protocol error") \

#define XERR_ADDR_MAP(XX) \
	XX(ADDRFAMILY,         "address family not supported") \
	XX(AGAIN,              "temporary failure") \
	XX(BADFLAGS,           "bad ai_flags value") \
	XX(CANCELED,           "request canceled") \
	XX(FAIL,               "permanent failure") \
	XX(FAMILY,             "ai_family not supported") \
	XX(MEMORY,             "out of memory") \
	XX(NODATA,             "no address") \
	XX(NONAME,             "unknown node or service") \
	XX(OVERFLOW,           "argument buffer overflow") \
	XX(SERVICE,            "service not available for socket type") \
	XX(SOCKTYPE,           "socket type not supported") \
	XX(BADHINTS,           "invalid value for hints") \
	XX(PROTOCOL,           "resolved protocol is unknown") \

#define XERR_HTTP_MAP(XX) \
	XX(SYNTAX,             "invalid syntax") \
	XX(SIZE,               "size of value exceeded maximum allowed") \
	XX(STATE,              "parser state is invalid") \
	XX(TOOSHORT,           "input is too short") \
	XX(BUFS,               "not enough buffer space available") \

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

	case XERR_SYS:
		switch (XECODE(code)) {
#define XX(sym, msg) \
			case sym: return msg;
	XERR_SYS_MAP(XX)
#undef XX
		}
		return strerror(XECODE(code));;

	case XERR_ADDR:
		switch (XECODE(code)) {
#define XX(sym, msg) \
			case EAI_##sym: return msg;
	XERR_ADDR_MAP(XX)
#undef XX
		}
		break;

	case XERR_HTTP:
		switch (code) {
#define XX(sym, msg) \
			case XEHTTP##sym: return msg;
	XERR_HTTP_MAP(XX)
#undef XX
		}
		break;
	}

	return "undefined error";
}

static void
stack_abort(FILE *out)
{
	fflush(out);
#if HAS_EXECINFO
	void *calls[32];
	int frames = backtrace(calls, sizeof calls / sizeof calls[0]);
	backtrace_symbols_fd(calls, frames, fileno(out));
#endif
	abort();
}

void
xerr_abort(int code)
{
	fprintf(stderr, "%s\n", xerr_str(code));
	stack_abort(stderr);
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
	stack_abort(stderr);
}
