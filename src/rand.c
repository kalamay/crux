#include "../include/crux/rand.h"
#include "../include/crux/err.h"
#include "config.h"

#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

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

void xinit_rand (void) {}

#else

#include <unistd.h>
#include <sys/stat.h>

static int fd = -1;

void
xinit_rand (void)
{
	while (1) {
		fd = open ("/dev/urandom", O_RDONLY);
		if (fd >= 0) {
			break;
		}
		if (errno != EINTR) {
			fprintf (stderr, "*** failed to open /dev/urandom: %s", xerr_str (errno));
			exit (1);
		}
	}

	// stat the fd so it can be verified
	struct stat sbuf;
	if (fstat (fd, &sbuf) < 0) {
		fprintf (stderr, "*** failed to stat /dev/urandom: %s", xerr_str (errno));
		close (fd);
		fd = -1;
		exit (1);
	}

	// check that it is a char special
	if (!S_ISCHR (sbuf.st_mode) ||
			// verify that the device is /dev/random or /dev/urandom (linux only)
			(sbuf.st_rdev != makedev (1, 8) && sbuf.st_rdev != makedev (1, 9))) {
		fprintf (stderr, "*** /dev/urandom is an invalid device");
		close (fd);
		fd = -1;
		exit (1);
	}
}

int
xrand (void *const restrict dst, size_t len)
{
	size_t amt = 0;
	while (amt < len) {
		ssize_t r = read (fd, (char *)dst+amt, len-amt);
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

