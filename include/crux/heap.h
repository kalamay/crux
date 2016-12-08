#ifndef CRUX_HEAP_H
#define CRUX_HEAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct xheap;

struct xheap_entry {
	int64_t prio;  /** sort priority (lowest to highest) */
	uint32_t key;  /** key into the heap */
	uint32_t user; /** unused user value */
};

typedef void (*xheap_cb)(struct xheap_entry *ent, void *data);

#define XHEAP_NONE 0
#define XHEAP_ROOT 1

/**
 * Creates a new empty heap
 * 
 * @param  heapp   indirect heap object pointer to own the new heap
 * @return  0 on success or <0 on error
 */
extern int
xheap_new (struct xheap **heapp);

/**
 * Finalizes and deallocates an idirectly referenced heap object
 * 
 * The objected pointed to by `*selfp` may be NULL, but `selfp` must point
 * to a valid address. The `*selfp` address will be set to NULL.
 *
 * @param  heapp  indirect heap object pointer
 */
extern void
xheap_free (struct xheap **heapp);

extern uint32_t
xheap_count (const struct xheap *heap);

extern struct xheap_entry *
xheap_get (const struct xheap *heap, uint32_t key);

extern int
xheap_add (struct xheap *heap, struct xheap_entry *e);

extern int
xheap_remove (struct xheap *heap, struct xheap_entry *e);

extern int
xheap_update (const struct xheap *heap, struct xheap_entry *e);

extern void
xheap_clear (struct xheap *heap, xheap_cb func, void *data);

#endif

