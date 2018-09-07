#include "../include/crux.h"
#include "../include/crux/buf.h"

#define XBUF_REDZONE 64
#define XBUF_MAX_COMPACT (2*xpagesize)
#define XBUF_MIN_TRIM (2*xpagesize)

#define XBUF_ROUND(sz) ((((sz) + xpagesize - 1) / xpagesize) * xpagesize)
#define XBUF_HINT(h) XBUF_ROUND((h) + XBUF_REDZONE)

struct xbuf {
	uint8_t *wr;                   /**< Write pointer offset from #map **/
	uint8_t *rd;                   /**< Read pointer offset from #map **/
	uint8_t *map;                  /**< Base buffer address **/
	size_t cap;                    /**< Usable byte size of #map **/
};

#define XBUF_INIT \
	(struct xbuf){ NULL, NULL, NULL, 0 }

#define XBUF_MSIZE(b) ((b)->cap + XBUF_REDZONE)

#define XBUF_RSIZE(b) ((b)->wr - (b)->rd)
#define XBUF_RDATA(b) ((b)->rd)
#define XBUF_ROFFSET(b) ((b)->rd - (b)->map)
#define XBUF_RSET(b, off) ((b)->rd = (b)->map + (off))
#define XBUF_RBUMP(b, len) ((b)->rd += (len))

#define XBUF_WSIZE(b) ((b)->cap - XBUF_WOFFSET(b))
#define XBUF_WDATA(b) ((b)->wr)
#define XBUF_WOFFSET(b) ((b)->wr - (b)->map)
#define XBUF_WSET(b, off) ((b)->wr = (b)->map + (off))
#define XBUF_WBUMP(b, len) ((b)->wr += (len))

#define XBUF_COMPACT(b, len) do { \
	(b)->wr = (b)->map + (len); \
	(b)->rd = (b)->map; \
} while (0)

XEXTERN int
xbuf_init(struct xbuf *buf, size_t hint);

