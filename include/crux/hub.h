#ifndef CRUX_HUB_H
#define CRUX_HUB_H

struct xhub;

extern int
xhub_new (struct xhub **hubp);

extern void
xhub_free (struct xhub **hupb);

extern int
xhub_run (struct xhub *hub);

extern void
xhub_stop (struct xhub *hub);

#define xspawn(hub, fn, data) \
	xspawnf (hub, __FILE__, __LINE__, fn, data)

extern int
xspawnf (struct xhub *hub, const char *file, int line,
		void (*fn)(struct xhub *, void *), void *data);

#if defined (__BLOCKS__)

extern int
xspawn_b (struct xhub *hub, void (^block)(void));

#endif

#endif

