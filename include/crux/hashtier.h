#ifndef CRUX_HASHTIER_H
#define CRUX_HASHTIER_H

#include "def.h"
#include "err.h"
#include "hash.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/**
 * Hash base functionality
 *
 * These macros implement the underlying robin hood search, insert, and
 * remove behavior on a single-allocation, fixed size tier. Higher-level
 * data structures manage resizing across tiers.
 */

/**
 * Swaps the values at two indices.
 *
 * This uses a buffer and memcpy calls. This will get optimized into
 * register assignments if the element size is appropriate.
 *
 * @param  arr  const: array reference
 * @param  a    const: first array index
 * @param  b    const: second array index
 */
#define XHASHTIER_SWAP(arr, a, b) do { \
	uint8_t xsym(tmp)[sizeof((arr)[0])]; \
	memcpy(xsym(tmp), &(arr)[(a)], sizeof(xsym(tmp))); \
	(arr)[(a)] = (arr)[(b)]; \
	memcpy(&(arr)[(b)], xsym(tmp), sizeof(xsym(tmp))); \
} while (0)

/**
 * Determines the allocation count
 *
 * @param  n  desired count
 * @return  allocation count
 */
#define XHASHTIER_SIZE(n) __extension__ ({ \
	size_t __size = (size_t)(n); \
	__size < 8 ? 8 : xpower2(__size); \
})

/**
 * Determines the starting index for a hash value
 *
 * @param  hash  hash value of the key
 * @param  mod   mod value, typically prime
 * @return  starting probe index
 */
#define XHASHTIER_START(hash, mod) \
	(ssize_t)((uint64_t)(hash) % (size_t)(mod))
/**
 * Wraps an index for a new probe position
 * 
 * @param  idx   index position to wrap
 * @param  mask  bit mask value (should be equal to `size-1`)
 * @return  index position within the array
 */
#define XHASHTIER_WRAP(idx, mask) \
	(ssize_t)((size_t)(idx) & (size_t)(mask))

/**
 * Probes an open addressed array for a valid index
 *
 * @param  idx   index position to wrap
 * @param  size  number of buckets in the array
 * @param  hash  hash value of the key
 * @param  mod   mod value, typically prime
 * @param  mask  bit mask value (should be equal to `size-1`)
 * @return  index position within the array
 */
#define XHASHTIER_STEP(idx, size, hash, mod, mask) \
	XHASHTIER_WRAP((idx) + (size) - XHASHTIER_START(hash, mod), mask)

/**
 * Declares a tier structure for a given entry type
 *
 * Example:
 *     struct thing {
 *         int key;
 *         const char *value;
 *     };
 *
 *     struct thing_tier {
 *         XHASHTIER(struct thing);
 *     };
 *     ...
 *     printf("size: %zu\n", sizeof(struct thing_tier));
 *
 * @param  name  struct name
 * @param  TEnt  entry type
 * @return  struct definition
 */
#define XHASHTIER(TEnt) \
	size_t size; \
	size_t mod; \
	size_t count; \
	size_t remap; \
	struct { \
		TEnt entry; \
		uint64_t h; \
	} arr[]

#define XHASHTIER_EACH(tier, entp) \
	for (ssize_t xsym(i) = (ssize_t)(tier)->size - 1; \
			xsym(i) >= 0; \
			xsym(i)--) \
		if ((tier)->arr[xsym(i)].h && \
				(entp = &(tier)->arr[xsym(i)].entry)) \

/**
 * Generates extern function prototypes for the tier
 *
 * @param  pref   function name prefix
 * @param  TTier  tier structure type
 * @param  TKey   key type
 */
#define XHASHTIER_EXTERN(pref, TTier, TKey) \
	XHASHTIER_PROTO(XEXTERN, pref, TTier, TKey)

/**
 * Generates functions for the tier
 *
 * @param  pref     function name prefix
 * @param  TTier    tier structure name
 * @param  TKey     key type
 */
#define XHASHTIER_GEN(pref, TTier, TKey) \
	XHASHTIER_GEN_PRIV(pref, TTier, TKey, pref##_has_key, pref##_hash)

/**
 * Generates functions for the tier with an int-like key
 *
 * This generates a hash function so a user-defined function should be omitted.
 *
 * @param  pref     function name prefix
 * @param  TTier    tier structure name
 * @param  TKey     key type
 */
#define XHASHTIER_INT_GEN(pref, TTier, TKey) \
	XHASH_INT_GEN(XSTATIC, pref##_hash, TKey) \
	XHASHTIER_GEN(pref, TTier, TKey)

/**
 * Generates static functions for the tier
 *
 * @param  pref   function name prefix
 * @param  TTier  tier structure type
 * @param  TKey   key type
 */
#define	XHASHTIER_STATIC(pref, TTier, TKey) \
	XHASHTIER_PROTO(XSTATIC, pref, TTier, TKey) \
	XHASHTIER_GEN(pref, TTier, TKey)

/**
 * Generates static functions for the tier using an int-like key.
 *
 * This generates a hash function so a user-defined function should be omitted.
 *
 * @param  pref   function name prefix
 * @param  TTier  tier structure type
 * @param  TKey   key type
 */
#define	XHASHTIER_INT_STATIC(pref, TTier, TKey) \
	XHASHTIER_PROTO(XSTATIC, pref, TTier, TKey) \
	XHASHTIER_INT_GEN(pref, TTier, TKey)

/**
 * Generates attributed function prototypes for the tier 
 *
 * @param  TTier  tier structure type
 * @param  TKey   key type
 * @param  pref   function name prefix
 * @param  attr   attributes to apply to the function prototypes
 */
#define XHASHTIER_PROTO(attr, pref, TTier, TKey) \
	attr double \
	pref##_load(const TTier *tier); \
	attr ssize_t \
	pref##_get(TTier *tier, TKey k, size_t kn); \
	attr ssize_t \
	pref##_hget(TTier *tier, TKey k, size_t kn, uint64_t h); \
	attr ssize_t \
	pref##_reserve(TTier *tier, TKey k, size_t kn, int *full); \
	attr ssize_t \
	pref##_hreserve(TTier *tier, TKey k, size_t kn, uint64_t h, int *full); \
	attr size_t \
	pref##_force(TTier *tier, const TTier *src, size_t idx); \
	attr int \
	pref##_del(TTier *tier, size_t idx); \
	attr void \
	pref##_clear(TTier *tier); \
	attr size_t \
	pref##_remap(TTier *tier, TTier *dst); \
	attr size_t \
	pref##_nremap(TTier *tier, TTier *dst, size_t limit); \
	attr int \
	pref##_new(TTier **tierp, size_t n); \
	attr int \
	pref##_new_size(TTier **tierp, size_t n); \
	attr int \
	pref##_renew(TTier **tierp, size_t n); \
	attr int \
	pref##_renew_size(TTier **tierp, size_t n); \

/**
 * Generates functions for the tier with a named key verification function
 *
 * @param  pref     function name prefix
 * @param  TTier    tier structure name
 * @param  TKey     key type
 * @param  has_key  function to test if a key matches an entry
 * @param  hash     function to create hash from key
 */
#define XHASHTIER_GEN_PRIV(pref, TTier, TKey, has_key, hash) \
	double \
	pref##_load(const TTier *tier) \
	{ \
		return (double)tier->count / (double)tier->size; \
	} \
	ssize_t \
	pref##_get(TTier *tier, TKey k, size_t kn) \
	{ \
		return pref##_hget(tier, k, kn, hash(k, kn)); \
	} \
	ssize_t \
	pref##_hget(TTier *tier, TKey k, size_t kn, uint64_t h) \
	{ \
		if (tier->count == 0) { return XESYS(ENOENT); } \
		const size_t size = tier->size; \
		const size_t mod = tier->mod; \
		const size_t mask = size - 1; \
		ssize_t dist, i = XHASHTIER_START(h, mod); \
		for (dist = 0; 1; dist++, i = XHASHTIER_WRAP(i+1, mask)) { \
			uint64_t eh = tier->arr[i].h; \
			if (eh == 0 || XHASHTIER_STEP(i, size, eh, mod, mask) < dist) { \
				return XESYS(ENOENT); \
			} \
			if (eh == h && has_key(&tier->arr[i].entry, k, kn)) { \
				return i; \
			} \
		} \
	} \
	ssize_t \
	pref##_reserve(TTier *tier, TKey k, size_t kn, int *full) \
	{ \
		return pref##_hreserve(tier, k, kn, hash(k, kn), full); \
	} \
	ssize_t \
	pref##_hreserve(TTier *tier, TKey k, size_t kn, uint64_t h, int *full) \
	{ \
		assert(tier->remap == tier->size); \
		assert(full != NULL); \
		if (tier->count == tier->size) { return XESYS(ENOBUFS); } \
		const size_t size = tier->size; \
		const size_t mod = tier->mod; \
		const size_t mask = size - 1; \
		ssize_t dist, rc = XESYS(ENOENT), i = XHASHTIER_START(h, mod); \
		for (dist = 0; 1; dist++, i = XHASHTIER_WRAP(i+1, mask)) { \
			uint64_t eh = tier->arr[i].h; \
			if (eh == 0 || (eh == h && has_key(&tier->arr[i].entry, k, kn))) { \
				if (rc == XESYS(ENOENT)) { rc = i; } \
				else { \
					tier->arr[i] = tier->arr[rc]; \
					memset(&tier->arr[rc], 0, sizeof(tier->arr[rc])); \
				} \
				tier->arr[rc].h = h; \
				if (eh == 0) { \
					*full = 0; \
					tier->count++; \
				} \
				else { \
					*full = 1; \
				} \
				return rc; \
			} \
			ssize_t next = XHASHTIER_STEP(i, size, eh, mod, mask); \
			if (next < dist) { \
				if (xlikely(rc < 0)) { rc = i; } \
				else { XHASHTIER_SWAP(tier->arr, rc, i); } \
				dist = next; \
			} \
		} \
	} \
	size_t \
	pref##_force(TTier *tier, const TTier *src, size_t idx) \
	{ \
		const size_t size = tier->size; \
		const size_t mod = tier->mod; \
		const size_t mask = size - 1; \
		ssize_t dist, rc = -1, i = XHASHTIER_START(src->arr[idx].h, mod); \
		for (dist = 0; 1; dist++, i = XHASHTIER_WRAP(i+1, mask)) { \
			uint64_t eh = tier->arr[i].h; \
			if (eh == 0) { \
				if (rc < 0) { rc = i; } \
				else { tier->arr[i] = tier->arr[rc]; } \
				tier->arr[rc] = src->arr[idx]; \
				tier->count++; \
				return (size_t)rc; \
			} \
			ssize_t next = XHASHTIER_STEP(i, size, eh, mod, mask); \
			if (next < dist) { \
				if (xlikely(rc < 0)) { rc = i; } \
				else { XHASHTIER_SWAP(tier->arr, rc, i); } \
				dist = next; \
			} \
		} \
	} \
	int \
	pref##_del(TTier *tier, size_t idx) \
	{ \
		if (idx >= tier->size) { return XESYS(ERANGE); } \
		if (tier->arr[idx].h == 0) { return XESYS(ENOENT); } \
		const size_t size = tier->size; \
		const size_t mod = tier->mod; \
		const size_t mask = size - 1; \
		do { \
			memset(&tier->arr[idx], 0, sizeof(tier->arr[idx])); \
			ssize_t p = idx; \
			idx = XHASHTIER_WRAP(idx+1, mask); \
			uint64_t eh = tier->arr[idx].h; \
			if (eh == 0 || XHASHTIER_STEP(idx, size, eh, mod, mask) == 0) { \
				tier->count--; \
				return 0; \
			} \
			tier->arr[p] = tier->arr[idx]; \
		} while (1); \
	} \
	void \
	pref##_clear(TTier *tier) \
	{ \
		tier->count = 0; \
		tier->remap = tier->size; \
		memset(tier->arr, 0, sizeof(tier->arr[0]) * tier->size); \
	} \
	size_t \
	pref##_remap(TTier *tier, TTier *dst) \
	{ \
		size_t n = 0; \
		for (size_t i = tier->remap; i > 0; i--) { \
			if (tier->arr[i-1].h != 0) { \
				pref##_force(dst, tier, i-1); \
				tier->arr[i-1].h = 0; \
				n++; \
			} \
		} \
		tier->remap = 0; \
		tier->count = 0; \
		return n; \
	} \
	size_t \
	pref##_nremap(TTier *tier, TTier *dst, size_t limit) \
	{ \
		size_t n = 0, i = tier->remap; \
		for (; i > 0 && n < limit; i--) { \
			if (tier->arr[i-1].h != 0) { \
				pref##_force(dst, tier, i-1); \
				if (xunlikely(i == tier->size)) { \
					pref##_del(tier, i-1); \
					i++; \
				} \
				else { \
					tier->arr[i-1].h = 0; \
					tier->count--; \
				} \
				n++; \
			} \
		} \
		tier->remap = i; \
		return n; \
	} \
	int \
	pref##_new(TTier **tierp, size_t n) \
	{ \
		return pref##_new_size(tierp, XHASHTIER_SIZE(n)); \
	} \
	int \
	pref##_new_size(TTier **tierp, size_t n) \
	{ \
		TTier *tier = calloc(1, sizeof(*tier) + sizeof(tier->arr[0]) * n); \
		if (tier == NULL) { return XERRNO; } \
		tier->size = n; \
		tier->mod = xpower2_prime(n); \
		tier->remap = n; \
		*tierp = tier; \
		return 1; \
	} \
	int \
	pref##_renew(TTier **tierp, size_t n) \
	{ \
		return pref##_renew_size(tierp, XHASHTIER_SIZE(n)); \
	} \
	int \
	pref##_renew_size(TTier **tierp, size_t n) \
	{ \
		TTier *tier = *tierp; \
		if (tier != NULL) { \
			if (n == tier->size) { \
				tier->remap = n; \
				return 0; \
			} \
			if (n < tier->count) { return XESYS(EPERM); } \
		} \
		int rc = pref##_new_size(tierp, n); \
		if (rc < 0) { return rc; } \
		if (tier != NULL) { \
			pref##_remap(tier, *tierp); \
			free(tier); \
		} \
		return 1; \
	} \

#define XHASH_INT_GEN(attr, name, TKey) \
	attr uint64_t \
	name(TKey k, size_t kn) \
	{ \
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

