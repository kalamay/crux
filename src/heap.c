#include "heap.h"
#include "../include/crux/err.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "config.h"

#define ROW_SHIFT 9
#define ROW_WIDTH (1 << ROW_SHIFT)

#define ROW(h, n) ((h)->entries[(n) >> ROW_SHIFT])
#define ENTRY(h, n) ROW(h, n)[(n) & (ROW_WIDTH - 1)]

static uint32_t
parent (uint32_t key)
{
	assert (key != UINT32_MAX);

	uint32_t mask, pidx;

	mask = key & XHEAP_PAGEMASK;
	if (key < XHEAP_PAGECOUNT || mask > 3) {
		pidx = (key & ~XHEAP_PAGEMASK) | (mask >> 1);
	}
	else if (mask < 2) {
		pidx = (key - XHEAP_PAGECOUNT) >> XHEAP_PAGESHIFT;
		pidx += pidx & ~(XHEAP_PAGEMASK >> 1);
		pidx |= XHEAP_PAGECOUNT / 2;
	}
	else {
		pidx = key - 2;
	}
	return pidx;
}

static void
child (uint32_t key, uint32_t *a, uint32_t *b)
{
	uint32_t shift;

	if (key > XHEAP_PAGEMASK && (key & (XHEAP_PAGEMASK - 1)) == 0) {
		*a = *b = key + 2;
	}
	else if (key & (XHEAP_PAGECOUNT >> 1)) {
		*a = (((key & ~XHEAP_PAGEMASK) >> 1) | (key & (XHEAP_PAGEMASK >> 1))) + 1;
		shift = (uint32_t)*a << XHEAP_PAGESHIFT;
		*a = (uint32_t)shift; // truncat back to 32 bits
		if (*a == shift) { // check for overflow
			*b = *a + 1;
		}
		else {
			*a = UINT32_MAX;
			*b = UINT32_MAX;
		}
	}
	else {
		*a = key + (key & XHEAP_PAGEMASK);
		*b = *a + 1;
	}
}

static int
add_row (struct xheap *heap)
{
	// check if we need to resize the array of rows
	if (&ROW (heap, heap->capacity) >= heap->entries + heap->rows) {
		struct xheap_entry ***entries =
			realloc (heap->entries, sizeof *entries * heap->rows * 2);
		if (!entries) {
			return XERRNO;
		}
		memset (entries + heap->rows, 0, heap->rows * sizeof *entries);
		heap->entries = entries;
		heap->rows *= 2;
	}
	ROW (heap, heap->capacity) = malloc (sizeof **heap->entries * ROW_WIDTH);
	heap->capacity += ROW_WIDTH;
	return 0;
}

static void
swap (const struct xheap *heap, uint32_t aidx, uint32_t bidx)
{
	assert (heap != NULL);
	assert (aidx < heap->next);
	assert (bidx < heap->next);

	void *tmp;

	tmp = ENTRY (heap, aidx);
	ENTRY (heap, aidx) = ENTRY (heap, bidx);
	ENTRY (heap, bidx) = tmp;
	ENTRY (heap, aidx)->key = aidx;
	ENTRY (heap, bidx)->key = bidx;
}

static uint32_t
move_up (const struct xheap *heap, uint32_t key)
{
	assert (heap != NULL);
	assert (key < heap->next);

	uint32_t pidx;

	while (key > XHEAP_ROOT) {
		pidx = parent (key);
		if (ENTRY (heap, key)->prio >= ENTRY (heap, pidx)->prio) {
			break;
		}
		swap (heap, key, pidx);
		key = pidx;
	}
	return key;
}

static uint32_t
move_down (const struct xheap *heap, uint32_t key)
{
	assert (heap != NULL);
	assert (key < heap->next);

	uint32_t cidx1, cidx2;

	while (1) {
		child (key, &cidx1, &cidx2);
		if (cidx1 >= heap->next) {
			return key;
		}
		if (cidx1 != cidx2 && cidx2 < heap->next) {
			if (ENTRY (heap, cidx2)->prio < ENTRY (heap, cidx1)->prio) {
				cidx1 = cidx2;
			}
		}
		if (ENTRY (heap, key)->prio < ENTRY (heap, cidx1)->prio) {
			return key;
		}
		swap (heap, key, cidx1);
		key = cidx1;
	}
}

int
xheap_new (struct xheap **heapp)
{
	struct xheap *heap = malloc (sizeof *heap);
	if (heap == NULL) {
		return XERRNO;
	}

	int rc = xheap_init (heap);
	if (rc < 0) {
		free (heap);
		return rc;
	}

	*heapp = heap;
	return 0;
}

int
xheap_init (struct xheap *heap)
{
	int rc;

	heap->rows = 16;
	heap->capacity = 0;
	heap->next = XHEAP_ROOT;
	heap->entries = calloc (heap->rows, sizeof *heap->entries);

	if (heap->entries == NULL) {
		return XERRNO;
	}

	rc = add_row (heap);
	if (rc < 0) {
		free (heap->entries);
		return rc;
	}

	return 0;
}

void
xheap_free (struct xheap **heapp)
{
	assert (heapp != NULL);

	struct xheap *heap = *heapp;
	if (heap != NULL) {
		*heapp = NULL;
		xheap_final (heap);
		free (heap);
	}
}

void
xheap_final (struct xheap *heap)
{
	for (uint32_t i = 0; i < (heap->capacity / ROW_WIDTH); i++) {
		free (heap->entries[i]);
	}
	free (heap->entries);
}

uint32_t
xheap_count (const struct xheap *heap)
{
	assert (heap != NULL);

	return heap->next - 1;
}

struct xheap_entry *
xheap_get (const struct xheap *heap, uint32_t key)
{
	assert (heap != NULL);
	assert (key >= XHEAP_ROOT);

	return key < heap->next ? ENTRY (heap, key) : NULL;
}

int
xheap_add (struct xheap *heap, struct xheap_entry *e)
{
	assert (heap != NULL);
	assert (e != NULL);
	assert (heap->capacity >= heap->next);

	if (heap->capacity == heap->next) {
		int rc = add_row (heap);
		if (rc < 0) { return rc; }
	}

	uint32_t key = heap->next++;
	ENTRY (heap, key) = e;
	e->key = key;
	move_up (heap, key);
	return 0;
}

int
xheap_remove (struct xheap *heap, struct xheap_entry *e)
{
	assert (heap != NULL);
	assert (heap->next > XHEAP_ROOT);
	assert (e != NULL);

	uint32_t key = e->key;
	if (key == 0 || key >= heap->next) {
		return -ENOENT;
	}

	e->key = XHEAP_NONE;
	if (key == --heap->next) {
		ENTRY (heap, heap->next) = NULL;
	}
	else {
		ENTRY (heap, key) = ENTRY (heap, heap->next);
		ENTRY (heap, heap->next) = NULL;
		ENTRY (heap, key)->key = key;
		key = move_up (heap, key);
		move_down (heap, key);
	}

	// always keep one extra row when removing
	if ((((heap->next + ROW_WIDTH - 2) / ROW_WIDTH) + 1) * ROW_WIDTH < heap->capacity) {
		free (ROW (heap, heap->capacity - 1));
		ROW (heap, heap->capacity - 1) = NULL;
		heap->capacity -= ROW_WIDTH;
	}

	return 0;
}

int
xheap_update (const struct xheap *heap, struct xheap_entry *e)
{
	assert (heap != NULL);
	assert (heap->next > XHEAP_ROOT);
	assert (e != NULL);

	uint32_t key = e->key;
	if (key == 0 || key >= heap->next) {
		return -ENOENT;
	}

	key = move_up (heap, key);
	move_down (heap, key);

	return 0;
}

void
xheap_clear (struct xheap *heap, xheap_cb each, void *data)
{
	assert (heap != NULL);

	uint32_t i;
	for (i = XHEAP_ROOT; i < heap->next; i++) {
		ENTRY (heap, i)->key = XHEAP_NONE;
		each (ENTRY (heap, i), data);
		ENTRY (heap, i) = NULL;
	}
	for (i = (heap->capacity / ROW_WIDTH) - 1; i > 0; i--) {
		free (heap->entries[i]);
	}
	heap->next = XHEAP_ROOT;
	heap->capacity = ROW_WIDTH;
}

