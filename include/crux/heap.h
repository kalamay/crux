#ifndef CRUX_HEAP_H
#define CRUX_HEAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "def.h"

/**
 * @brief  Opaque type for a binary heap
 *
 * The heap maintains a collection of object in ascending order based on a
 * user-defined priority value.
 */
struct xheap;

/**
 * @brief  Transparent type for entries into the heap
 *
 * This is used as an intrusive member of a separately defined struct.
 *
 * For example:
 *     struct thing {
 *         struct xheap_entry handle;
 *     };
 *
 *     struct thing *t = malloc (sizeof (*t));
 *     t->prio = 100;
 *     xheap_add (heap, &t->handle);
 *     ...
 *     struct xheap_entry *entry = xheap_get (heap, XHEAP_ROOT);
 *     t = xcontainer (entry, struct thing, handle);
 *     xheap_remove (heap, entry);
 *     free (t);
 *
 * The priority field must be set before adding an entry to the heap.
 *
 * The key* will be updated heap as item positions are changed. Do not keep a
 * copy of the key value directly. Instead, store either a pointer to the key
 * or the entry object itself.
 *
 * The heap does not manage the memory of the entries. These must be proprly
 * dealt with when removing. The `xheap_clear` function is a more efficient way
 * to handle deallocating all entries in the heap.
 */
struct xheap_entry {
	int64_t prio;  /** sort priority (lowest to highest) */
	uint32_t key;  /** key into the heap */
	uint32_t user; /** any custom user value */
};

/**
 * @brief  Callback function used for clearing the heap
 */
typedef void (*xheap_fn)(struct xheap_entry *ent, void *data);

/**
 * @brief  Value representing an invalid key
 */
#define XHEAP_NONE 0

/**
 * @brief  Value representing the lowest item in the heap
 */
#define XHEAP_ROOT 1

/**
 * @brief  Allocates and initializes a new empty heap
 * 
 * @param[out]  heapp   indirect heap object pointer to own the new heap
 * @return  0 on success, -errno on error
 *
 * Errors:
 *   `-ENOMEM`: an allocation is required and the system is out of memory
 */
extern int
xheap_new (struct xheap **heapp);

/**
 * @brief  Finalizes and deallocates an idirectly referenced heap object
 * 
 * The objected pointed to by `*heapp` may be NULL, but `heapp` must point
 * to a valid address. The `*heapp` address will be set to NULL.
 *
 * @param  heapp  indirect heap object pointer
 */
extern void
xheap_free (struct xheap **heapp);

/**
 * @brief  Gets the number of entries in the heap
 *
 * @param  heap  heap pointer
 * @return  number of entries
 */
extern uint32_t
xheap_count (const struct xheap *heap);

/**
 * @brief  Gets an entry using an explicit key
 *
 * Use the value `XHEAP_ROOT` will retrieve the lowest item.
 *
 * @param  heap  heap pointer
 * @param  key   entry key >= `XHEAP_ROOT`
 * @return  entry pointer or `NULL`
 */
extern struct xheap_entry *
xheap_get (const struct xheap *heap, uint32_t key);

/**
 * @brief  Adds an entry to the heap
 *
 * The `prio` field must be set prior to adding to the heap. The `key` field
 * will be updated with the proper value when the entry is successfully added.
 *
 * @param  heap  heap pointer
 * @param  e     entry pointer to add
 * @return  0 on succes, -errno on error
 *
 * Errors:
 *   `-ENOMEM`: an allocation is required and the system is out of memory
 */
extern int
xheap_add (struct xheap *heap, struct xheap_entry *e);

/**
 * @brief  Removes an entry from the heap
 *
 * This expects the `key` value of the entry to properly reflect the position
 * in the heap.
 *
 * Note, the heap does not manage the memory of the entries.
 *
 * @param  heap  heap pointer
 * @param  e     entry currenty in heap
 * @return  0 on succes, -errno on error
 *
 * Errors:
 *   `-ENOENT`: the key for the entry is not in the heap
 */
extern int
xheap_remove (struct xheap *heap, struct xheap_entry *e);

/**
 * @brief  Updates the priority of an entry
 *
 * Any time the priority of an entry changes, this function must be called to
 * reposition it properly. The `key` value of the entry is expected to properly
 * reflect the position in the heap.
 *
 * @param  heap  heap pointer
 * @param  e     entry currenty in heap
 * @return  0 on succes, -errno on error
 *
 * Errors:
 *   `-ENOENT`: the key for the entry is not in the heap
 */
extern int
xheap_update (const struct xheap *heap, struct xheap_entry *e);

/**
 * @brief  Removes all entries from the heap
 *
 * This is generally the most efficient way to remove all the entries from the
 * heap. The `fn` function will be passed each entry in order to properly
 * manage the memory.
 *
 * @param  heap  heap pointer
 * @param  fn    callback function for each entry
 * @param  data  user pointer to pass to `fn`
 */
extern void
xheap_clear (struct xheap *heap, xheap_fn fn, void *data);

#endif

