#include "../include/crux.h"
#include "../include/crux/buf.h"

#define XBUF_REDZONE 64
#define XBUF_MAX_COMPACT (2*xpagesize)
#define XBUF_MIN_TRIM (2*xpagesize)

#define XBUF_LINE_SIZE(h) xpageround((h) + XBUF_REDZONE)
#define XBUF_FILE_SIZE(h) xpageround((h) + XBUF_REDZONE)
#define XBUF_RING_SIZE(h) xpageround(h)

struct xbuf {
	uint8_t *map;   /**< Base buffer address **/
	uint64_t r;     /**< Read pointer offset from #map **/
	uint64_t w;     /**< Write pointer offset from #map **/
	size_t cap;     /**< Usable byte size of #map **/
	size_t sz;      /**< Map size in bytes **/
	int mode;       /**< Buffer allocation mode **/
};

#define XBUF_LINE 0 /**< Linear memory buffer mode **/
#define XBUF_FILE 1 /**< File-backed buffer mode **/
#define XBUF_RING 2 /**< Circular memory buffer mode **/

#define XBUF_INIT(mode) \
	(struct xbuf){ NULL, 0, 0, 1, 0, mode }

#define XBUF_EMPTY(b) ((b)->r != (b)->w)

#define XBUF_RSIZE(b) ((b)->w - (b)->r)
#define XBUF_RDATA(b) ((b)->map + XBUF_ROFFSET(b))
#define XBUF_ROFFSET(b) ((b)->mode == XBUF_RING ? (b)->r % (b)->cap : (b)->r)
#define XBUF_RBUMP(b, len) ((b)->r += (len))

#define XBUF_WSIZE(b) ((b)->cap - XBUF_WOFFSET(b))
#define XBUF_WDATA(b) ((b)->map + XBUF_WOFFSET(b))
#define XBUF_WOFFSET(b) ((b)->mode == XBUF_RING ? (b)->w % (b)->cap : (b)->w)
#define XBUF_WBUMP(b, len) ((b)->w += (len))

#define XBUF_COMPACT(b, len) do { \
	(b)->r = 0; \
	(b)->w = (len); \
} while (0)

XLOCAL int
xbuf_init(struct xbuf *buf, size_t hint, int mode);

XLOCAL void
xbuf_final(struct xbuf *buf);
