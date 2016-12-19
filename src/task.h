#include "../include/crux/task.h"

struct xdefer {
	struct xdefer *next;
	void (*fn) (void *);
	void *data;
};

struct xmgr {
	uint32_t map_size;
	uint32_t stack_size;
	uint32_t tls_size;
	int flags;
	struct xdefer *free_defer;
	struct xtask *free_task;
};

extern int
xmgr_init (struct xmgr *mgr, size_t tls, size_t stack, int flags);

extern void
xmgr_final (struct xmgr *mgr);

