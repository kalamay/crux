#include "../include/crux/task.h"

struct xdefer {
	struct xdefer *next;
	void (*fn)(union xvalue);
	union xvalue val;
};

struct xmgr {
	uint32_t map_size;
	uint32_t stack_size;
	uint16_t tls_size, tls_user;
	int flags;
	struct xdefer *free_defer;
	struct xtask *free_task;
};

XLOCAL int
xmgr_init(struct xmgr *mgr, size_t tls, size_t stack, int flags);

XLOCAL void
xmgr_final(struct xmgr *mgr);

XLOCAL void
xtask_record_entry(struct xtask *t, void *fn);

XLOCAL void
xtask_print_val(const struct xtask *t, FILE *out, int indent);

