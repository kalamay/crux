#include "../include/crux/rand.h"
#include "../include/crux/err.h"
#include "../include/crux/seed.h"

#include <math.h>

#if !HAS_GETRANDOM
# include <unistd.h>
# include <sys/stat.h>
# include <fcntl.h>
static int randfd = -1;
#endif

static thread_local struct xrand32 rng32;
static thread_local struct xrand64 rng64[2];

static const uint64_t MULT32 = UINT64_C(6364136223846793005);
static const xuint128 MULT64 = XUINT128_C(2549297995355413924, 4865540595714422341);

void
xseed_random(union xseed *seed)
{
	seed->u128.low = xrand64(&rng64[0]);
	seed->u128.high = xrand64(&rng64[1]);
}

XEXTERN int
xseed_random_secure(union xseed *seed)
{
	return xrand_secure(seed, sizeof(*seed));
}

#if !HAS_GETRANDOM

# ifdef S_ISNAM
#  define IS_RAND_MODE(mode) (S_ISNAM(mode) || S_ISCHR(mode))
# else
#  define IS_RAND_MODE(mode) (S_ISCHR(mode))
# endif

# if defined(__linux__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(1, 9))
# elif defined(__APPLE__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(14, 1))
# elif defined(__FreeBSD__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(0, 10))
# elif defined(__DragonFly__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(8, 4))
# elif defined(__NetBSD__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(46, 1))
# elif defined(__OpenBSD__)
#  define IS_RAND_DEVICE(dev) ((dev) == makedev(45, 2))
# else
#  define IS_RAND_DEVICE(dev) 1
# endif

# define IS_RAND(st) (IS_RAND_MODE((st).st_mode) && IS_RAND_DEVICE((st).st_rdev))

static int
random_verify(int fd)
{
	struct stat st;
	if (fstat(fd, &st) < 0) { return xerrno; }
	if (!IS_RAND(st)) { return xerr_sys(EBADF); }
	return 0;
}

static int
random_open(void)
{
	int fd, err;
	for (;;) {
		fd = open("/dev/urandom", O_RDONLY|O_CLOEXEC);
		if (fd >= 0) {
			err = random_verify(fd);
			if (err == 0) { return fd; }
			close(fd);
		}
		else {
			err = xerrno;
		}
		if (err != xerr_sys(EINTR)) {
			return err;
		}
	}
}
#endif

int
xrand_init(void)
{
#if !HAS_GETRANDOM
	int fd = random_open();
	if (fd < 0) { return fd; }
	if (randfd >= 0) { close(randfd); }
	randfd = fd;
#endif
	return 0;
}

int
xrand_init_thread(void)
{
	struct {
		xuint128 s64[4];
		uint64_t s32[2];
	} seeds;

	ssize_t n = xrand_secure(&seeds, sizeof(seeds));
	if (n < 0) {
		return (int)n;
	}

	xrand32_seed(&rng32, seeds.s32[0], seeds.s32[1]);
	xrand64_seed(&rng64[0], seeds.s64[0], seeds.s64[1]);
	xrand64_seed(&rng64[1], seeds.s64[2], seeds.s64[3]);

	return 0;
}

#if HAS_GETRANDOM

#include <sys/random.h>

ssize_t
xrand_secure(void *dst, size_t len)
{
	void *end = (char *)dst + len;
	while (dst < end) {
		ssize_t r = getrandom(dst, len, 0);
		if (r > 0) {
			len -= r;
			dst += r;
		}
		else if (r == 0 || errno != EINTR) {
			return xerrno;
		}
	}
	return 0;
}

#else

ssize_t
xrand_secure(void *dst, size_t len)
{
	void *end = (char *)dst + len;
	while (dst < end) {
		ssize_t r = read(randfd, dst, len);
		if (r > 0) {
			len -= r;
			dst += r;
		}
		else if (r == 0 || errno != EINTR) {
			return xerrno;
		}
	}
	return 0;
}

#endif

static inline uint32_t
rotr32(uint32_t v, unsigned int k)
{
	return (v >> k) | (v << ((- k) & 31));
}

inline static uint64_t
rotr64(uint64_t v, unsigned k)
{
    return (v >> k) | (v << (64 - k));
}

int
xrand32_seed_secure(struct xrand32 *rng)
{
	uint64_t seed[2];
	ssize_t n = xrand_secure(&seed, sizeof(seed));
	if (n < 0) { return (int)n; }
	xrand32_seed(rng, seed[0], seed[1]);
	return 0;
}

int
xrand64_seed_secure(struct xrand64 *rng)
{
	xuint128 seed[2];
	ssize_t n = xrand_secure(&seed, sizeof(seed));
	if (n < 0) { return (int)n; }
	xrand64_seed(rng, seed[0], seed[1]);
	return 0;
}

uint32_t
xrand32_global(void)
{
	return xrand32(&rng32);
}

uint64_t
xrand64_global(void)
{
	return xrand64(&rng64[0]);
}

double
xrand_real_global(void)
{
	return xrand_real(&rng64[0]);
}

/*
 * (c) 2014 M.E. O'Neill / pcg-random.org
 * Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
 */

void
xrand32_seed(struct xrand32 *rng, uint64_t state, uint64_t seq)
{
	rng->inc = seq = (seq << 1u) | 1u;
	rng->state = (seq + state) * MULT32 + seq;
}

void
xrand64_seed(struct xrand64 *rng, xuint128 state, xuint128 seq)
{
	rng->inc = seq = (seq << 1u) | 1u;
	rng->state = (seq + state) * MULT64 + seq;
}

uint32_t
xrand32(struct xrand32 *rng)
{
	uint64_t os = rng->state;
    rng->state = os * MULT32 + rng->inc;
	return rotr32(((os >> 18u) ^ os) >> 27u, os >> 59u);
}

uint64_t
xrand64(struct xrand64 *rng)
{
	xuint128 ns = rng->state = rng->state * MULT64 + rng->inc;
    return rotr64(((uint64_t)(ns >> 64u)) ^ (uint64_t)ns, ns >> 122u);
}

uint32_t
xrand32_bound(struct xrand32 *rng, uint32_t bound)
{
	uint32_t threshold = -bound % bound;
	for (;;) {
		uint32_t r = xrand32(rng);
		if (r >= threshold) {
			return r % bound;
		}
	}
}

uint64_t
xrand64_bound(struct xrand64 *rng, uint64_t bound)
{
	uint64_t threshold = -bound % bound;
	for (;;) {
		uint64_t r = xrand64(rng);
		if (r >= threshold) {
			return r % bound;
		}
	}
}

/*
 * Copyright (c) 2014, Taylor R Campbell
 *
 * Verbatim copying and distribution of this entire article are
 * permitted worldwide, without royalty, in any medium, provided
 * this notice, and the copyright notice, are preserved.
 *
 * http://mumble.net/~campbell/tmp/random_real.c
 */
double
xrand_real(struct xrand64 *rng)
{
	int exponent = -64;
	uint64_t significand;
	unsigned shift;

	/*
	 * Read zeros into the exponent until we hit a one; the rest
	 * will go into the significand.
	 */
	while (xunlikely((significand = xrand64(rng)) == 0)) {
		exponent -= 64;
		/*
		 * If the exponent falls below -1074 = emin + 1 - p,
		 * the exponent of the smallest subnormal, we are
		 * guaranteed the result will be rounded to zero.  This
		 * case is so unlikely it will happen in realistic
		 * terms only if xrand64 is broken.
		 */
		if (xunlikely(exponent < -1074))
			return 0;
	}

	/*
	 * There is a 1 somewhere in significand, not necessarily in
	 * the most significant position.  If there are leading zeros,
	 * shift them into the exponent and refill the less-significant
	 * bits of the significand.  Can't predict one way or another
	 * whether there are leading zeros: there's a fifty-fifty
	 * chance, if xrand64 is uniformly distributed.
	 */
	shift = __builtin_clzll(significand);
	if (shift != 0) {
		exponent -= shift;
		significand <<= shift;
		significand |= (xrand64(rng) >> (64 - shift));
	}

	/*
	 * Set the sticky bit, since there is almost surely another 1
	 * in the bit stream.  Otherwise, we might round what looks
	 * like a tie to even when, almost surely, were we to look
	 * further in the bit stream, there would be a 1 breaking the
	 * tie.
	 */
	significand |= 1;

	/*
	 * Finally, convert to double (rounding) and scale by
	 * 2^exponent.
	 */
	return ldexp((double)significand, exponent);
}

