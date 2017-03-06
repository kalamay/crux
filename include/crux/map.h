#ifndef CRUX_MAP_H
#define CRUX_MAP_H

#include <stdio.h>

#include "hash.h"

#define XMAP_NEW_ENTRY    0
#define XMAP_UPDATE_ENTRY 1

struct xmap;

XEXTERN int
xmap_new(struct xmap **mp, double loadf, size_t hint);

XEXTERN void
xmap_free(struct xmap **mp);

XEXTERN size_t
xmap_count(const struct xmap *m);

XEXTERN size_t
xmap_max(const struct xmap *m);

XEXTERN void
xmap_set_print(struct xmap *m, void (*)(void *, FILE *));

XEXTERN int
xmap_resize(struct xmap *m, size_t hint);

XEXTERN size_t
xmap_condense(struct xmap *m, size_t limit);

XEXTERN bool
xmap_has(struct xmap *m, const void *k, size_t kn);

XEXTERN void *
xmap_get(struct xmap *m, const void *k, size_t kn);

XEXTERN int
xmap_put(struct xmap *m, const void *k, size_t kn, void **v);

XEXTERN bool
xmap_del(struct xmap *m, const void *k, size_t kn, void **v);

XEXTERN int
xmap_reserve(struct xmap *m, const void *k, size_t kn, void ***v);

XEXTERN void
xmap_clear(struct xmap *m);

XEXTERN void
xmap_print(const struct xmap *m, FILE *out);

#endif

