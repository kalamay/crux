#include "../include/crux/heap.h"

struct xheap {
	uint32_t rows;
	uint32_t capacity;
	uint32_t next;
	struct xheap_entry ***entries;
};

int
xheap_init(struct xheap *heap);

void
xheap_final(struct xheap *heap);

