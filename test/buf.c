#include "mu.h"
#include "../include/crux/buf.h"

#define add(buf, str) xbuf_add(buf, str, sizeof(str)-1)

static void
test_print(void)
{
	struct xbuf *buf;
	mu_assert_int_eq(xbuf_new(&buf, 0), 0);

	mu_assert_int_eq(add(buf, "test value with some more text to help with the byte printing"), 0);
	xbuf_print(buf, stdout);
	xbuf_trim(buf, 5);
	xbuf_print(buf, stdout);
	xbuf_trim(buf, 11);
	xbuf_print(buf, stdout);
	xbuf_trim(buf, 18);
	xbuf_print(buf, stdout);

	xbuf_free(&buf);
}

static void
test_grow(void)
{
	struct xbuf *buf;
	mu_assert_int_eq(xbuf_new(&buf, 0), 0);

	for (int i = 0; i < 1000; i++) {
		mu_assert_int_eq(add(buf, "test value with some more text to help with the byte printing"), 0);
		mu_assert_int_eq(xbuf_trim(buf, 10), 0);
	}

	mu_assert_int_eq(xbuf_trim(buf, 5000), 0);

	xbuf_free(&buf);
}

int
main(void)
{
	mu_init("buf");

	mu_run(test_print);
	mu_run(test_grow);
}

