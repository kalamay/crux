#include "../include/crux/task.h"

struct xmgr {
	uint32_t map_size;
	uint32_t stack_size;
	uint32_t tls_size;
	int flags;
	struct xdefer *pool;
	struct xtask *dead;
};

extern int
xmgr_init (struct xmgr *mgr, size_t stack, size_t tls, int flags);

extern void
xmgr_final (struct xmgr *mgr);

