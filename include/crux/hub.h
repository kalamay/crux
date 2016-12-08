#ifndef CRUX_HUB_H
#define CRUX_HUB_H

/**
 * @brief  Opaque type for hub instances
 */
struct xhub;

/**
 * @brief  Allocates a new hub object.
 *
 * @param[out]  hubp  indirect hub object pointer
 * @return  0 on success, -errno on error
 */
extern int
xhub_new (struct xhub **hubp);

/**
 * @brief  Deallocates a hub object.
 *
 * @param[in,out]  hubp  indirect hub object pointer
 */
extern void
xhub_free (struct xhub **hupb);

/**
 * @brief  Runs 
 */
extern int
xhub_run (struct xhub *hub);

extern void
xhub_stop (struct xhub *hub);

extern int
xspawn (struct xhub *hub, 
		void (*fn)(struct xhub *, void *), void *data);

#if defined (__BLOCKS__)

extern int
xspawn_b (struct xhub *hub, void (^block)(void));

#endif

#endif

