#ifndef CRUX_HUB_H
#define CRUX_HUB_H

/**
 * @brief  Opaque type for hub instances
 */
struct xhub;

/**
 * @brief  Allocates a new hub object
 *
 * @param[out]  hubp  indirect hub object pointer
 * @return  0 on success, -errno on error
 *
 * Errors:
 *   `-EMFILE`: the per-process descriptor table is full
 *   `-ENFILE`: the system file table is full
 *   `-ENOMEM`: insufficient memory was available
 */
extern int
xhub_new (struct xhub **hubp);

/**
 * @brief  Deallocates a hub object
 *
 * @param[in,out]  hubp  indirect hub object pointer
 */
extern void
xhub_free (struct xhub **hupb);

/**
 * @brief  Runs the hub until all task are complete
 *
 * @param  hub  hub pointer
 * @return  0 on succes, -errno on error
 */
extern int
xhub_run (struct xhub *hub);

/**
 * @brief  Forces the hub to stop
 *
 * @param  hub  hub pointer
 */
extern void
xhub_stop (struct xhub *hub);

/**
 * @brief  Spawns a task managed by the hub
 *
 * @param  hub   hub pointer
 * @param  fn    function to invoke as the entry point of the task
 * @param  data  user data pointer to pass to `fn`
 * @return  0 on success, -errno on error
 */
extern int
xspawn (struct xhub *hub, 
		void (*fn)(struct xhub *, void *), void *data);

#if defined (__BLOCKS__)

/**
 * @brief  Spawns a block-based task managed by the hub
 *
 * @param  hub    hub pointer
 * @param  block  block to invoke as the entry point of the task
 * @return  0 on success, -errno on error
 */
extern int
xspawn_b (struct xhub *hub, void (^block)(void));

#endif

#endif

