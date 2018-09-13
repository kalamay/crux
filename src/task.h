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

extern int
xmgr_init(struct xmgr *mgr, size_t tls, size_t stack, int flags);

extern void
xmgr_final(struct xmgr *mgr);

extern void
xtask_record_entry(struct xtask *t, void *fn);

extern void
xtask_print_val(const struct xtask *t, FILE *out, int indent);

