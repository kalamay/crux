#ifndef CRUX_BUF_H
#define CRUX_BUF_H

#include "def.h"

#include <stdio.h>

struct xbuf;

XEXTERN int
xbuf_new(struct xbuf **bufp, size_t cap);

XEXTERN void
xbuf_free(struct xbuf **bufp);

XEXTERN const void *
xbuf_value(const struct xbuf *buf);

XEXTERN size_t
xbuf_length(const struct xbuf *buf);

XEXTERN void *
xbuf_tail(const struct xbuf *buf);

XEXTERN size_t
xbuf_capacity(const struct xbuf *buf);

XEXTERN int
xbuf_ensure(struct xbuf *buf, size_t len);

XEXTERN int
xbuf_add(struct xbuf *buf, const void *ptr, size_t len);

XEXTERN void
xbuf_reset(struct xbuf *buf);

XEXTERN int
xbuf_trim(struct xbuf *buf, size_t len);

XEXTERN int
xbuf_bump(struct xbuf *buf, size_t len);

XEXTERN void
xbuf_compact(struct xbuf *buf);

XEXTERN void
xbuf_print(const struct xbuf *buf, FILE *out);

#endif
