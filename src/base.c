#include "../include/crux/err.h"
#include "../include/crux/version.h"
#include "../include/crux/clock.h"
#include "../include/crux/rand.h"
#include "../include/crux/seed.h"

#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>
#include <assert.h>

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

#if HAS_MACH_TIME
# include <mach/mach_time.h>
static mach_timebase_info_data_t info = { 1, 1 };
#endif

#if !HAS_ARC4
# include <unistd.h>
# include <sys/stat.h>
static int randfd = -1;
#endif

const uint64_t XPOWER2_PRIMES[64] = {
	1ULL, 2ULL, 3ULL,
	7ULL, 13ULL, 31ULL,
	61ULL, 127ULL, 251ULL,
	509ULL, 1021ULL, 2039ULL,
	4093ULL, 8191ULL, 16381ULL,
	32749ULL, 65521ULL, 131071ULL,
	262139ULL, 524287ULL, 1048573ULL,
	2097143ULL, 4194301ULL, 8388593ULL,
	16777213ULL, 33554393ULL, 67108859ULL,
	134217689ULL, 268435399ULL, 536870909ULL,
	1073741789ULL, 2147483647ULL, 4294967291ULL,
	8589934583ULL, 17179869143ULL, 34359738337ULL,
	68719476731ULL, 137438953447ULL, 274877906899ULL,
	549755813881ULL, 1099511627689ULL, 2199023255531ULL,
	4398046511093ULL, 8796093022151ULL, 17592186044399ULL,
	35184372088777ULL, 70368744177643ULL, 140737488355213ULL,
	281474976710597ULL, 562949953421231ULL, 1125899906842597ULL,
	2251799813685119ULL, 4503599627370449ULL, 9007199254740881ULL,
	18014398509481951ULL, 36028797018963913ULL, 72057594037927931ULL,
	144115188075855859ULL, 288230376151711717ULL, 576460752303423433ULL,
	1152921504606846883ULL, 2305843009213693951ULL, 4611686018427387847ULL,
	9223372036854775783ULL,
};

static union xseed SEED_RANDOM;
static union xseed SEED_DEFAULT = {
	.u128 = {
		0x9ae16a3b2f90404fULL,
		0xc3a5c85c97cb3127ULL
	}
};

const union xseed *const XSEED_RANDOM = &SEED_RANDOM;
const union xseed *const XSEED_DEFAULT = &SEED_DEFAULT;

int
xinit (void)
{
#if HAS_MACH_TIME
	(void)mach_timebase_info (&info);
#endif

#if !HAS_ARC4
	while (1) {
		randfd = open ("/dev/urandom", O_RDONLY);
		if (randfd >= 0) {
			break;
		}
		int rc = XERRNO;
		if (rc != XESYS (EINTR)) {
			fprintf (stderr, "[cux:init:error failed to open /dev/urandom: %s", xerr_str (rc));
			return rc;
		}
	}

	// stat the randfd so it can be verified
	struct stat sbuf;
	if (fstat (randfd, &sbuf) < 0) {
		int rc = XERRNO;
		fprintf (stderr, "[crux:init:error] failed to stat /dev/urandom: %s", xerr_str (rc));
		close (randfd);
		randfd = -1;
		return rc;
	}

	// check that it is a char special
	if (!S_ISCHR (sbuf.st_mode) ||
			// verify that the device is /dev/random or /dev/urandom (linux only)
			(sbuf.st_rdev != makedev (1, 8) && sbuf.st_rdev != makedev (1, 9))) {
		int rc = XESYS (ENODEV);
		fprintf (stderr, "[crux:init:error] /dev/urandom is an invalid device");
		close (randfd);
		randfd = -1;
		return rc;
	}
#endif

	return xrand (&SEED_RANDOM, sizeof SEED_RANDOM);
}

static void __attribute__((constructor))
auto_init (void)
{
	int rc = xinit ();
	if (rc < 0) { exit (1); }
}



struct xversion
xversion (void)
{
	return (struct xversion) { VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH };
}


const char *
xerr_type (int code)
{
	switch (XETYPE (code)) {
#define XX(sym, msg) \
		case XERR_##sym: return msg;
	XERR_TYPE_MAP (XX)
#undef XX
	}
	return "invalid type";
}

const char *
xerr_str (int code)
{
	if (code > 0) { code = -code; }
	switch (XETYPE (code)) {

	case XERR_SYS:
		switch (XECODE (code)) {
#define XX(sym, msg) \
			case sym: return msg;
	XERR_SYS_MAP (XX)
#undef XX
		}
		return strerror (XECODE (code));;

	case XERR_ADDR:
		switch (XECODE (code)) {
#define XX(sym, msg) \
			case EAI_##sym: return msg;
	XERR_ADDR_MAP (XX)
#undef XX
		}
		break;

	case XERR_HTTP:
		switch (code) {
#define XX(sym, msg) \
			case XEHTTP##sym: return msg;
	XERR_HTTP_MAP (XX)
#undef XX
		}
		break;
	}

	return "undefined error";
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



int
xclock_real (struct xclock *c)
{
	assert (c != NULL);

#if HAS_CLOCK_GETTIME
	if (clock_gettime (CLOCK_REALTIME, &c->ts) < 0) {
		return XERRNO;
	}
#else
	struct timeval now;
    if (gettimeofday (&now, NULL) < 0) {
		return XERRNO;
	}
    c->ts.tv_sec = now.tv_sec;
    c->ts.tv_nsec = now.tv_usec * 1000;
#endif
	return 0;
}

int
xclock_mono (struct xclock *c)
{
	assert (c != NULL);

#if HAS_CLOCK_GETTIME
	if (clock_gettime (CLOCK_MONOTONIC, &c->ts) < 0) {
		return XERRNO;
	}
#elif HAS_MACH_TIME
	XCLOCK_SET_NSEC (c, (mach_absolute_time () * info.numer) / info.denom);
#else
	XCLOCK_SET_NSEC (c, clock () * (X_NSEC_PER_SEC / CLOCKS_PER_SEC));
#endif
	return 0;
}

double
xclock_diff (struct xclock *c)
{
	assert (c != NULL);

	struct xclock now;
	if (xclock_mono (&now) < 0) {
		return NAN;
	}
	XCLOCK_SUB (&now, c);
	return XCLOCK_TIME (&now);
}

double
xclock_step (struct xclock *c)
{
	assert (c != NULL);

	struct xclock now;
	if (xclock_mono (&now) < 0) {
		return NAN;
	}
	double time = XCLOCK_TIME (&now) - XCLOCK_TIME (c);
	*c = now;
	return time;
}

void
xclock_print (const struct xclock *c, FILE *out)
{
	if (out == NULL) {
		out = stdout;
	}

	if (c == NULL) {
		fprintf (out, "<crux:clock:(null)>\n");
	}
	else {
		fprintf (out, "<crux:clock:%p time=%f>\n", (void *)c, XCLOCK_TIME (c));
	}
}



#if HAS_ARC4

int
xrand (void *const restrict dst, size_t len)
{
	arc4random_buf (dst, len);
	return 0;
}

int
xrand_u32 (uint32_t bound, uint32_t *out)
{
	*out = bound ? arc4random_uniform (bound) : arc4random ();
	return 0;
}

#else

int
xrand (void *const restrict dst, size_t len)
{
	size_t amt = 0;
	while (amt < len) {
		ssize_t r = read (randfd, (char *)dst+amt, len-amt);
		if (r > 0) {
			amt += (size_t)r;
		}
		else if (r == 0 || errno != EINTR) {
			return XERRNO;
		}
	}
	return 0;
}

int
xrand_u32 (uint32_t bound, uint32_t *out)
{
	uint32_t val;
	int rc = xrand (&val, sizeof val);
	if (rc < 0) { return rc; }
	if (bound) {
		val = ((double)val / (double)UINT32_MAX) * bound;
	}
	*out = val;
	return 0;
}

#endif

int
xrand_u64 (uint64_t bound, uint64_t *out)
{
	uint64_t val;
	int rc = xrand (&val, sizeof val);
	if (rc < 0) { return rc; }
	if (bound) {
		val = ((double)val / (double)UINT64_MAX) * bound;
	}
	*out = val;
	return 0;
}

int
xrand_num (double *out)
{
	uint64_t val;
	int rc = xrand (&val, sizeof val);
	if (rc < 0) { return rc; }
	*out = (double)val / (double)UINT64_MAX;
	return 0;
}

