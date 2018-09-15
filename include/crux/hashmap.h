#ifndef CRUX_HASHMAP_H
#define CRUX_HASHMAP_H

#include "hashtier.h"

#include <math.h> 

#define XHASHMAP_RESERVE_NEW 0
#define XHASHMAP_RESERVE_UPD 1

#define XHASHMAP(pref, TEnt, ntiers) \
	struct pref##_tier { \
		XHASHTIER(TEnt); \
	} *tiers[ntiers]; \
	double loadf; \
	size_t count; \
	size_t max

#define xhashmap_each(map, entp) \
	for (size_t xsym(t) = 0; \
			xsym(t) < xlen((map)->tiers) && (map)->tiers[xsym(t)]; \
			xsym(t)++) \
		xhashtier_each((map)->tiers[xsym(t)], entp)

/**
 * Generates extern function prototypes for the map
 *
 * @param  pref  function name prefix
 * @param  TMap  map structure type
 * @param  TKey  key type
 * @param  TEnt  entry type
 */
#define XHASHMAP_EXTERN(pref, TMap, TKey, TEnt) \
	XHASHMAP_PROTO(XEXTERN, pref, TMap, TKey, TEnt)

/**
 * Generates static functions for the map
 *
 * @param  pref  function name prefix
 * @param  TMap  map structure type
 * @param  TKey  key type
 * @param  TEnt  entry type
 */
#define XHASHMAP_STATIC(pref, TMap, TKey, TEnt) \
	XHASHMAP_PROTO(XSTATIC, pref, TMap, TKey, TEnt) \
	XHASHMAP_GEN(pref, TMap, TKey, TEnt)

/**
 * Generates static function prototypes for the map using an int-like key
 *
 * @param  pref  function name prefix
 * @param  TMap  map structure type
 * @param  TKey  key type
 * @param  TEnt  entry type
 */
#define XHASHMAP_INT_STATIC(pref, TMap, TKey, TEnt) \
	XHASHMAP_PROTO(XSTATIC, pref, TMap, TKey, TEnt) \
	XHASHMAP_INT_GEN(pref, TMap, TKey, TEnt)

/**
 * Generates attributed function prototypes for the map
 *
 * @param  attr  attributes to apply to the function prototypes
 * @param  pref  name prefix
 * @param  TMap  structure type
 * @param  TKey  key type
 * @param  TEnt  entry type
 */
#define XHASHMAP_PROTO(attr, pref, TMap, TKey, TEnt) \
	attr int \
	pref##_init(TMap *map, double loadf, size_t hint); \
	attr void \
	pref##_final(TMap *map); \
	attr int \
	pref##_resize(TMap *map, size_t hint); \
	attr size_t \
	pref##_condense(TMap *map, size_t limit); \
	attr bool \
	pref##_has(TMap *map, TKey k, size_t kn); \
	attr TEnt * \
	pref##_get(TMap *map, TKey k, size_t kn); \
	attr int \
	pref##_put(TMap *map, TKey k, size_t kn, TEnt *entry); \
	attr bool \
	pref##_del(TMap *map, TKey k, size_t kn, TEnt *entry); \
	attr int \
	pref##_reserve(TMap *map, TKey k, size_t kn, TEnt **entry); \
	attr bool \
	pref##_remove(TMap *map, TEnt *entry); \
	attr void \
	pref##_clear(TMap *map); \
	attr void \
	pref##_print(const TMap *map, FILE *out, void (*fn)(const TMap *, TEnt *, FILE *)); \

#define XHASHMAP_GEN(pref, TMap, TKey, TEnt) \
	XHASHTIER_PROTO(XSTATIC, pref##_tier, struct pref##_tier, TKey) \
	XHASHTIER_GEN_PRIV(pref##_tier, struct pref##_tier, TKey, pref##_has_key, pref##_hash) \
	int \
	pref##_resize(TMap *map, size_t hint) \
	{ \
		if (hint < map->count) { return xerr_sys(EPERM); } \
		size_t sz = XHASHTIER_SIZE(ceil(hint / map->loadf)); \
		if (map->tiers[0] && sz == map->tiers[0]->size) { return 0; } \
		struct pref##_tier *tmp[xlen(map->tiers) + 1]; \
		tmp[0] = map->tiers[xlen(map->tiers) - 1]; \
		for (size_t s=0, d=1; s < xlen(map->tiers); s++) { \
			struct pref##_tier *n = map->tiers[s]; \
			if (n && n->size == sz && s != d) { tmp[0] = n; } \
			else                              { tmp[d++] = n; } \
		} \
		int rc = pref##_tier_renew_size(tmp, sz); \
		if (rc < 0) { return rc; } \
		memcpy(map->tiers, tmp, sizeof(map->tiers)); \
		map->max = tmp[0]->size * map->loadf; \
		return 1; \
	} \
	int \
	pref##_init(TMap *map, double loadf, size_t hint) \
	{ \
		for (size_t i = 0; i < xlen(map->tiers); i++) { \
			map->tiers[i] = NULL; \
		} \
		if (isnan(loadf) || loadf <= 0.0) { map->loadf = 0.9; } \
		else if (loadf > 0.99) { map->loadf = 0.99; } \
		else { map->loadf = loadf; } \
		map->count = 0; \
		map->max = 0; \
		return pref##_resize(map, hint); \
	} \
	void \
	pref##_final(TMap *map) \
	{ \
		if (map == NULL) { return; } \
		for (size_t i = 0; i < xlen(map->tiers) && map->tiers[i]; i++) { \
			free(map->tiers[i]); \
			map->tiers[i] = NULL; \
		} \
		map->count = 0; \
		map->max = 0; \
	} \
	size_t \
	pref##_condense(TMap *map, size_t limit) \
	{ \
		if (xlen(map->tiers) == 1) { return 0; } \
		size_t total = 0; \
		for (size_t i = xlen(map->tiers) - 1; i > 0 && limit > 0; i--) { \
			if (map->tiers[i] == NULL) { break; } \
			size_t n = pref##_tier_nremap \
				(map->tiers[i], map->tiers[0], limit); \
			limit -= n; \
			total += n; \
			if (map->tiers[i]->count == 0) { \
				free(map->tiers[i]); \
				map->tiers[i] = NULL; \
			} \
		} \
		return total; \
	} \
	XSTATIC int \
	pref##_prune_index(TMap *map, size_t tier, size_t idx) \
	{ \
		int rc = pref##_tier_del(map->tiers[tier], idx); \
		if (rc == 0 && tier > 0 && map->tiers[tier]->count == 0) { \
			free(map->tiers[tier]); \
			map->tiers[tier] = NULL; \
			for (tier++; tier < xlen(map->tiers); tier++) { \
				map->tiers[tier-1] = map->tiers[tier]; \
				map->tiers[tier] = NULL; \
			} \
		} \
		return rc; \
	} \
	bool \
	pref##_has(TMap *map, TKey k, size_t kn) \
	{ \
		uint64_t h = pref##_hash(map, k, kn); \
		for (size_t i = 0; i < xlen(map->tiers) && map->tiers[i]; i++) { \
			if (pref##_tier_get(map->tiers[i], k, kn, h, map) >= 0) { \
				return true; \
			} \
		} \
		return false; \
	} \
	TEnt * \
	pref##_get(TMap *map, TKey k, size_t kn) \
	{ \
		uint64_t h = pref##_hash(map, k, kn); \
		for (size_t i = 0; i < xlen(map->tiers) && map->tiers[i]; i++) { \
			ssize_t idx = pref##_tier_get(map->tiers[i], k, kn, h, map); \
			if (idx >= 0) { \
				if (xlen(map->tiers) > 1 && i > 0 && \
						pref##_tier_load(map->tiers[0]) < map->loadf) { \
					int full; \
					ssize_t res = pref##_tier_reserve(map->tiers[0], k, kn, h, &full, map); \
					if (res >= 0) { \
						map->tiers[0]->arr[res].entry = map->tiers[i]->arr[idx].entry; \
						pref##_prune_index(map, i, idx); \
						i = 0; \
						idx = res; \
					} \
				} \
				return &map->tiers[i]->arr[idx].entry; \
			} \
		} \
		return NULL; \
	} \
	XSTATIC int \
	pref##_hreserve(TMap *map, TKey k, size_t kn, uint64_t h, TEnt **entry) \
	{ \
		assert(h > 0); \
		if (map->count == map->max) { \
			int rc = pref##_resize(map, map->count + 1); \
			if (rc < 0) { return rc; } \
		} \
		int full; \
		size_t idx = pref##_tier_reserve(map->tiers[0], k, kn, h, &full, map); \
		if (!full) { \
			for (size_t i = 1; i < xlen(map->tiers) && map->tiers[i]; i++) { \
				ssize_t sidx = pref##_tier_get(map->tiers[i], k, kn, h, map); \
				if (sidx >= 0) { \
					map->tiers[0]->arr[idx].entry = map->tiers[i]->arr[sidx].entry; \
					pref##_prune_index(map, i, sidx); \
					full = 1; \
					goto out; \
				} \
			} \
			map->count++; \
		} \
	out: \
		*entry = &map->tiers[0]->arr[idx].entry; \
		return full; \
	} \
	int \
	pref##_reserve(TMap *map, TKey k, size_t kn, TEnt **entry) \
	{ \
		return pref##_hreserve(map, k, kn, pref##_hash(map, k, kn), entry); \
	} \
	int \
	pref##_put(TMap *map, TKey k, size_t kn, TEnt *entry) \
	{ \
		assert(entry != NULL); \
		uint64_t h = pref##_hash(map, k, kn); \
		TEnt *e; \
		int rc = pref##_hreserve(map, k, kn, h, &e); \
		if (rc < 0) { return rc; } \
		if (rc) { \
			TEnt tmp = *e; \
			*e = *entry; \
			*entry = tmp; \
		} \
		else {\
			*e = *entry; \
		} \
		return rc; \
	} \
	bool \
	pref##_del(TMap *map, TKey k, size_t kn, TEnt *entry) \
	{ \
		uint64_t h = pref##_hash(map, k, kn); \
		for (size_t i = 0; i < xlen(map->tiers); i++) { \
			if (map->tiers[i] == NULL) { break; } \
			ssize_t idx = pref##_tier_get(map->tiers[i], k, kn, h, map); \
			if (idx >= 0) { \
				if (entry != NULL) { *entry = map->tiers[i]->arr[idx].entry; } \
				if (pref##_prune_index(map, i, idx) == 0 && --map->count < map->max/3) { \
					pref##_resize(map, map->count); \
				} \
				return true; \
			} \
		} \
		return false; \
	} \
	bool \
	pref##_remove(TMap *map, TEnt *entry) \
	{ \
		for (size_t i = 0; i < xlen(map->tiers) && map->tiers[i]; i++) { \
			void *begin = (void *)map->tiers[i]->arr; \
			void *end = (void *)(map->tiers[i]->arr + map->tiers[i]->size); \
			if ((void *)entry >= begin && (void *)entry < end) { \
				size_t idx = ((uint8_t *)entry - (uint8_t *)begin) \
				           / sizeof(map->tiers[i]->arr[0]); \
				if (pref##_prune_index(map, i, idx) < 0) { \
					return false; \
				} \
				if (--map->count < map->max/3) { \
					pref##_resize(map, map->count); \
				} \
				return true; \
			} \
		} \
		return false; \
	} \
	void \
	pref##_clear(TMap *map) \
	{ \
		pref##_tier_clear(map->tiers[0]); \
		for (size_t i = 1; i < xlen(map->tiers) && map->tiers[i]; i++) { \
			free(map->tiers[i]); \
			map->tiers[i] = NULL; \
		} \
		map->count = 0; \
	} \
	void \
	pref##_print(const TMap *map, FILE *out, void (*fn)(const TMap *, TEnt *, FILE *out)) \
	{ \
		if (out == NULL) { out = stdout; } \
		fprintf(out, "<crux:map:" #pref "(" #TKey "," #TEnt "):"); \
		if (map == NULL) { \
			fprintf(out, "(null)>\n"); \
			return; \
		} \
		fprintf(out, "%p count=%zu max=%zu> {\n", (void *)map, map->count, map->max); \
		for (size_t i = 0; i < xlen(map->tiers) && map->tiers[i]; i++) { \
			struct pref##_tier *t = map->tiers[i]; \
			fprintf(out, \
					"  <tier[%zu]:%p-%p count=%zu, size=%zu, remap=%zu>", \
					i, (void *)t->arr, (void *)(t->arr + t->size), \
					t->count, t->size, t->remap); \
			if (fn) { \
				fprintf(out, " {\n"); \
				for (size_t j = 0; j < t->size; j++) { \
					if (t->arr[j].h) { \
						fprintf(out, "    "); \
						fn(map, &t->arr[j].entry, out); \
						fprintf(out, "\n"); \
					} \
				} \
				fprintf(out, "  }\n"); \
			} \
			else { \
				fprintf(out, "\n"); \
			} \
		} \
		fprintf(out, "}\n"); \
	} \

#define XHASHMAP_INT_GEN(pref, TMap, TKey, TEnt) \
	XHASH_INT_GEN(XSTATIC, pref##_hash, TKey) \
	XHASHMAP_GEN(pref, TMap, TKey, TEnt) \

#define XHASH_INT_GEN(attr, name, TKey) \
	attr uint64_t \
	name(const void *map, TKey k, size_t kn) \
	{ \
		(void)map; \
		(void)kn; \
		uint64_t x = ((uint64_t)k << 1) | 1; \
		x ^= x >> 33; \
		x *= 0xff51afd7ed558ccdULL; \
		x ^= x >> 33; \
		x *= 0xc4ceb9fe1a85ec53ULL; \
		x ^= x >> 33; \
		return x; \
	} \

#endif

