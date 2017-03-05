#ifndef CRUX_TASK_H
#define CRUX_TASK_H

#include "def.h"

#include <stdio.h>

union xvalue {
	uint64_t u64;
	int64_t i64;
	double dbl;
	void *ptr;
	int i;
};

#define XPTR(v) ((union xvalue){ .ptr = (v) })
#define XU64(v) ((union xvalue){ .u64 = (v) })
#define XI64(v) ((union xvalue){ .i64 = (v) })
#define XDBL(v) ((union xvalue){ .dbl = (v) })
#define XINT(v) ((union xvalue){ .i = (v) })
#define XZERO XU64(0)


#define XTASK_FPROTECT   (UINT32_C(1) << 0)
#define XTASK_FENTRY     (UINT32_C(1) << 1)

#define XTASK_FDEFAULT (XTASK_FPROTECT)
#define XTASK_FDEBUG (XTASK_FPROTECT|XTASK_FENTRY)

#define XSTACK_MIN 16384
#define XSTACK_MAX (1024 * XSTACK_MIN)
#define XSTACK_DEFAULT (8 * XSTACK_MIN)

#define XTASK_TLS_MAX 16384


struct xmgr;

XEXTERN int
xmgr_new(struct xmgr **mgrp, size_t tls, size_t stack, int flags);

XEXTERN void
xmgr_free(struct xmgr **mgrp);

XEXTERN struct xmgr *
xmgr_self(void);


struct xtask;

#define xtask_new(tp, mgr, tls, fn) \
	xtask_newf(tp, mgr, tls, __FILE__, __LINE__, fn)

XEXTERN int
xtask_newf(struct xtask **tp, struct xmgr *mgr, void *tls,
		const char *file, int line,
		union xvalue (*fn)(void *tls, union xvalue));

XEXTERN void
xtask_free(struct xtask **tp);

XEXTERN struct xtask *
xtask_self(void);

XEXTERN void *
xtask_local(struct xtask *t);

XEXTERN bool
xtask_alive(const struct xtask *t);

XEXTERN int
xtask_exitcode(const struct xtask *t);

XEXTERN int
xtask_exit(struct xtask *t, int ec);

XEXTERN void
xtask_print(const struct xtask *t, FILE *out);

XEXTERN union xvalue
xyield(union xvalue val);

XEXTERN union xvalue
xresume(struct xtask *t, union xvalue val);

XEXTERN int
xdefer(void (*fn) (void *), void *data);

XEXTERN void *
xmalloc(size_t size);

XEXTERN void *
xcalloc(size_t count, size_t size);

#endif

