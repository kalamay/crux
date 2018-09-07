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
	size_t cap;                    /**< Byte size of #map **/
};

#define XBUF_INIT \
	(struct xbuf){ NULL, NULL, NULL, 0 }

#define XBUF_CAPACITY(b) ((b)->cap - XBUF_WRITE_OFFSET(b))
#define XBUF_MAPSIZE(b) ((b)->cap + XBUF_REDZONE)
#define XBUF_LENGTH(b) ((b)->wr - (b)->rd)

#define XBUF_READ_OFFSET(b) ((b)->rd - (b)->map)
#define XBUF_HEAD(b) ((b)->rd)
#define XBUF_SET_HEAD(b, off) ((b)->rd = (b)->map + (off))
#define XBUF_BUMP_HEAD(b, len) ((b)->rd += (len))

#define XBUF_WRITE_OFFSET(b) ((b)->wr - (b)->map)
#define XBUF_TAIL(b) ((b)->wr)
#define XBUF_SET_TAIL(b, off) ((b)->wr = (b)->map + (off))
#define XBUF_BUMP_TAIL(b, len) ((b)->wr += (len))

#define XBUF_COMPACT(b, len) do { \
	(b)->wr = (b)->map + (len); \
	(b)->rd = (b)->map; \
} while (0)


XEXTERN int
xbuf_init(struct xbuf *buf, size_t cap);

