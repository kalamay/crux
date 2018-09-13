#include "../include/crux.h"
#include "../include/crux/task.h"

static union xvalue
fn(void *tls, union xvalue val)
{
	(void)tls;
	for (uint64_t i = val.u64; i > 0; i--) {
		xyield(val);
	}
	return val;
}

int
main(void)
{
	struct timespec start, end;
	struct xmgr *mgr;
	struct xtask *t;

	xcheck(xmgr_new(&mgr, 0, XSTACK_DEFAULT, XTASK_FDEFAULT));
	xcheck(xtask_new(&t, mgr, NULL, fn));

	union xvalue val = XU64(1<<24);
	xclock_mono(&start);
	while (xtask_alive(t)) { xresume(t, val); }
	xclock_mono(&end);

	xtask_free(&t);
	xmgr_free(&mgr);

	intmax_t diff = XCLOCK_NSEC(&end) - XCLOCK_NSEC(&start);
	printf("total time: %jdms\n"
	       "context switch: %jdns (%.2fM/sec)\n",
			(intmax_t)X_NSEC_TO_MSEC(diff),
			diff/(1<<25),
			(((double)(1<<25)/1000000.0) * ((double)X_NSEC_PER_SEC/(double)diff)));
	return 0;
}

