#include "../include/crux/http.h"
#include "../include/crux/hashmap.h"
#include "../include/crux/vec.h"
#include "../include/crux/seed.h"

#include "buf.h"

struct xhttp_vec
{
	XVEC(struct xhttp_field);
};

struct xhttp_map
{
	XHASHMAP(xhttp_tab, struct xhttp_vec, 2);
	struct xbuf buf;
	union xseed seed;
};

XLOCAL int
xhttp_map_init(struct xhttp_map *map);

XLOCAL void
xhttp_map_final(struct xhttp_map *map);

XLOCAL int
xhttp_map_put(struct xhttp_map *map, const struct xbuf *src, struct xhttp_field fld);

