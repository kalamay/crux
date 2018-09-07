#include "mu.h"
#include "../include/crux/buf.h"

static const char str[] = 
	"test value with some more text to help with the byte filling";

#define add(buf, arr) xbuf_add(buf, arr, sizeof(arr))

static void
test_grow(void)
{
	struct xbuf *buf;
	mu_assert_int_eq(xbuf_new(&buf, 0), 0);

	ssize_t rsize = 0;

	mu_assert_int_eq(xbuf_length(buf), rsize);

	for (int i = 0; i < 1000; i++) {
		mu_assert_int_eq(add(buf, str), 0);
		rsize += sizeof(str);
		mu_assert_int_eq(xbuf_length(buf), rsize);

		mu_assert_int_eq(xbuf_trim(buf, 10), 0);
		rsize -= 10;
		mu_assert_int_eq(xbuf_length(buf), rsize);
	}

	mu_assert_int_eq(xbuf_trim(buf, 5000), 0);
	rsize -= 5000;
	mu_assert_int_eq(xbuf_length(buf), rsize);

	xbuf_free(&buf);
}

static void
test_unused(void)
{
	struct xbuf *buf;
	mu_assert_int_eq(xbuf_new(&buf, 8000), 0);

	ssize_t unused = xbuf_unused(buf);
	ssize_t rsize = 0, wsize = unused;

	mu_assert_int_eq(rsize, xbuf_length(buf));
	mu_assert_int_gt(wsize, 0);

	mu_assert_int_eq(add(buf, str), 0);
	rsize += sizeof(str);
	wsize -= sizeof(str);

	mu_assert_int_eq(xbuf_length(buf), rsize);
	mu_assert_int_eq(xbuf_unused(buf), wsize);

	mu_assert_int_eq(xbuf_trim(buf, 10), 0);
	rsize -= 10;

	mu_assert_int_eq(xbuf_length(buf), rsize);
	mu_assert_int_eq(xbuf_unused(buf), wsize);

	mu_assert_int_eq(xbuf_trim(buf, rsize), 0);

	mu_assert_int_eq(xbuf_length(buf), 0);
	mu_assert_int_eq(xbuf_unused(buf), unused);

	xbuf_free(&buf);
}

int
main(void)
{
	mu_init("buf");

	mu_run(test_grow);
	mu_run(test_unused);
}

