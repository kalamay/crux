#include "../include/crux/resolv.h"
#include "../include/crux/list.h"

struct xresolv
{
	const struct xresolv_config *cfg;
	struct xlist *free;
	int fdpool[12];
	unsigned seed;
	unsigned char fdpos;
	unsigned char hostpos;
};

struct xresolv_entry
{
	struct xlist h;
	struct xdns packet;
};

XLOCAL int
xresolv_init(struct xresolv *r, const struct xresolv_config *cfg);

XLOCAL void
xresolv_final(struct xresolv *r);

