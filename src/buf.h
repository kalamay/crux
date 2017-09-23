#include "../include/crux/buf.h"

struct xbuf {
	uint8_t *wr;   /**< Write pointer offset from #map **/
	uint8_t *rd;   /**< Read pointer offset from #map **/
	uint8_t *map;  /**< Base buffer address **/
	size_t cap;    /**< Byte size of #map **/
};

#define XBUF_INIT { NULL, NULL, NULL, 0 }

#define XBUF_READ_OFFSET(b) ((b)->rd - (b)->map)
#define XBUF_WRITE_OFFSET(b) ((b)->wr - (b)->map)
#define XBUF_LENGTH(b) ((b)->wr - (b)->rd)
#define XBUF_CAPACITY(b) ((b)->cap - ((b)->wr - (b)->map))

XEXTERN int
xbuf_init(struct xbuf *buf, size_t cap);

