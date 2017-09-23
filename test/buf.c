#include "mu.h"
#include "../include/crux/buf.h"

#define add(buf, str) xbuf_add(buf, str, sizeof(str)-1)

int
main(void)
{
	mu_init("buf");

	struct xbuf *buf;
	mu_assert_int_eq(xbuf_new(&buf, 0), 0);

	add(buf, "test value with some more text to help with the byte printing");
	xbuf_print(buf, stdout);
	xbuf_trim(buf, 5);
	xbuf_print(buf, stdout);
	xbuf_trim(buf, 11);
	xbuf_print(buf, stdout);
	xbuf_trim(buf, 18);
	xbuf_print(buf, stdout);

	xbuf_free(&buf);
}

