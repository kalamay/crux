#include "../include/crux/heap.h"

size_t xheap_pagecount;
size_t xheap_pagemask;
size_t xheap_pageshift;

struct xheap
{
	uint32_t rows;
	uint32_t capacity;
	uint32_t next;
	struct xheap_entry ***entries;
};

XLOCAL int
xheap_init(struct xheap *heap);

XLOCAL void
xheap_final(struct xheap *heap);

