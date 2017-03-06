#include "../include/crux/map.h"
#include "../include/crux/hashmap.h"

#include <string.h>

struct entry {
	void *v;
	const void *k;
	size_t kn;
};

struct xmap {
	XHASHMAP(map, struct entry, 2);
	void (*print)(void *, FILE *);
};

static uint64_t
map_hash(const char *k, size_t kn)
{
	return xhash_xx64(k, kn, XSEED_RANDOM);
}

static bool
map_has_key(const struct entry *e, const void *k, size_t kn)
{
	return kn == e->kn && memcmp(e->k, k, kn) == 0;
}

static void
default_print_value(void *v, FILE *out)
{
	if (v) { fprintf(out, "%p", v); }
	else   { fprintf(out, "(null)"); }
}

XHASHMAP_STATIC(map, struct xmap, const void *, struct entry)

int
xmap_new(struct xmap **mp, double loadf, size_t hint)
{
	struct xmap *m = malloc(sizeof(*m));
	if (m == NULL) { return -errno; }
	int rc = map_init(m, loadf, hint);
	if (rc < 0) {
		free(m);
		return rc;
	}
	m->print = default_print_value;
	*mp = m;
	return 0;
}

void
xmap_free(struct xmap **mp)
{
	struct xmap *m = *mp;
	if (m != NULL) {
		*mp = NULL;
		map_final(m);
		free(m);
	}
}

size_t
xmap_count(const struct xmap *m)
{
	return m->count;
}

size_t
xmap_max(const struct xmap *m)
{
	return m->max;
}

void
xmap_set_print(struct xmap *m, void (*fn)(void *, FILE *))
{
	m->print = fn;
}

int
xmap_resize(struct xmap *m, size_t hint)
{
	return map_resize(m, hint);
}

size_t
xmap_condense(struct xmap *m, size_t limit)
{
	return map_condense(m, limit);
}

bool
xmap_has(struct xmap *m, const void *k, size_t kn)
{
	return map_has(m, k, kn);
}

void *
xmap_get(struct xmap *m, const void *k, size_t kn)
{
	struct entry *e = map_get(m, k, kn);
	return e ? e->v : NULL;
}

int
xmap_put(struct xmap *m, const void *k, size_t kn, void **v)
{
	struct entry e = { *v, k, kn };
	int rc = map_put(m, k, kn, &e);
	if (rc >= 0) { *v = e.v; }
	return rc;
}

bool
xmap_del(struct xmap *m, const void *k, size_t kn, void **v)
{
	struct entry e;
	bool rc = map_del(m, k, kn, &e);
	*v = e.v;
	return rc;
}

int
xmap_reserve(struct xmap *m, const void *k, size_t kn, void ***v)
{
	struct entry *e;
	int rc = map_reserve(m, k, kn, &e);
	if (rc >= 0) { *v = &e->v; }
	return rc;
}

void
xmap_clear(struct xmap *m)
{
	map_clear(m);
}

static void
print_entry(const struct xmap *m, struct entry *e, FILE *out)
{
	m->print(e->v, out);
}

void
xmap_print(const struct xmap *m, FILE *out)
{
	map_print(m, out, print_entry);
}

