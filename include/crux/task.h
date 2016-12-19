#ifndef CRUX_TASK_H
#define CRUX_TASK_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>


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

#define XTASK_STACK_MIN 16384
#define XTASK_STACK_MAX (1024 * XTASK_STACK_MIN)
#define XTASK_STACK_DEFAULT (8 * XTASK_STACK_MIN)

#define XTASK_TLS_MAX 16384


struct xmgr;

extern int
xmgr_new (struct xmgr **mgrp, size_t tls, size_t stack, int flags);

extern void
xmgr_free (struct xmgr **mgrp);

extern struct xmgr *
xmgr_self (void);


struct xtask;

#define xtask_new(tp, mgr, tls, fn) \
	xtask_newf (tp, mgr, tls, __FILE__, __LINE__, fn)

extern int
xtask_newf (struct xtask **tp, struct xmgr *mgr, void *tls,
		const char *file, int line,
		union xvalue (*fn)(void *tls, union xvalue));

extern void
xtask_free (struct xtask **tp);

extern struct xtask *
xtask_self (void);

extern void *
xtask_local (struct xtask *t);

extern bool
xtask_alive (const struct xtask *t);

extern int
xtask_exitcode (const struct xtask *t);

extern int
xtask_exit (struct xtask *t, int ec);

extern void
xtask_print (const struct xtask *t, FILE *out);

extern union xvalue
xyield (union xvalue val);

extern union xvalue
xresume (struct xtask *t, union xvalue val);

#endif

