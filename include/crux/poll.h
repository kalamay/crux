#ifndef CRUX_POLL_H
#define CRUX_POLL_H

#include "def.h"
#include "clock.h"

#include <signal.h>

#define XPOLL_ADD 0x0001 /** Operation to add an event */
#define XPOLL_DEL 0x0002 /** Operation to remove an event */

#define XPOLL_IN  0x0001 /** Type for file descriptor readability */
#define XPOLL_OUT 0x0002 /** Type for file descriptor writeability */
#define XPOLL_SIG 0x0006 /** Type for signal readyness */

#define XPOLL_ERR 0x0100 /** Flag to indicate an error with the event */
#define XPOLL_EOF 0x0200 /** Flag ti indicate an end-of-file state */

#define XPOLL_TYPE(n)  (int)((n) & 0x000F)
#define XPOLL_ISERR(n) (!!((n) & XPOLL_ERR))
#define XPOLL_ISEOF(n) (!!((n) & XPOLL_EOF))

#define XEVENT_TYPE(ev)  XPOLL_TYPE((ev)->type)
#define XEVENT_ISERR(ev) XPOLL_ISERR((ev)->type)
#define XEVENT_ISEOF(ev) XPOLL_ISEOF((ev)->type)

/**
 * @brief  Opaque poll type
 */
struct xpoll;

/**
 * @brief  Transparent event type
 */
struct xevent {
	struct xpoll *poll;
	void *ptr;
	int id;
	int type;
	int errcode;
};

/**
 * @brief  Allocates and initializes a new poll
 *
 * @param[out]  pollp   indirect poll object pointer to own the new poll
 * @return  0 on success, -errno on error
 *
 * Errors:
 *   `-EMFILE`: the per-process descriptor table is full
 *   `-ENFILE`: the system file table is full
 *   `-ENOMEM`: insufficient memory was available
 */
XEXTERN int
xpoll_new (struct xpoll **pollp);

/**
 * @brief  Finalizes and deallocates an idirectly referenced poll object
 * 
 * The objected pointed to by `*pollp` may be NULL, but `pollp` must point
 * to a valid address. The `*pollp` address will be set to NULL.
 *
 * @param  pollp  indirect poll object pointer
 */
XEXTERN void
xpoll_free (struct xpoll **pollp);

/**
 * @brief  Performs control operations on the poll
 *
 * This is used to add and remove events of interest from the poll.
 *
 * @param  poll  poll pointer
 * @param  op    operation to perform (XPOLL_ADD or XPOLL_DEL)
 * @param  type  event type (XPOLL_IN, XPOLL_OUT, or XPOLL_SIG)
 * @param  id    file descriptor or signal number
 * @param  ptr   user value to assign to the event
 * @return  0 on sucess, -errno on error
 *
 * Errors:
 *   `-EINVAL`: the operation, type, or signal number are invalid
 *   `-EBADF`: the specified descriptor is invalid
 *   `-ENOENT`: the event could not be found to be modified or deleted
 *   `-ENOMEM`: no memory was available to register the event
 */
XEXTERN int
xpoll_ctl (struct xpoll *poll, int op, int type, int id, void *ptr);

/**
 * @brief  Reads the next event from the poller
 *
 * The poller maintains an internal event list. When empty, more events will
 * be requested from the kernel.
 *
 * @param  poll  poll pointer
 * @param  ms    milliseconds until timeout, or -1 for infinite
 * @param[out]  ev  event information if successful
 * @return  1 on sucess, 0 on timeout, -errno on error
 */
XEXTERN int
xpoll_wait (struct xpoll *poll, int64_t ms, struct xevent *ev);

/**
 * @brief  Gets the monotonic clock associated with the poll
 *
 * This clock gets updated just prior to yielding an event from `xpoll_wait`.
 *
 * @param  poll  poll pointer
 * @return  clock pointer
 */
XEXTERN const struct xclock *
xpoll_clock (const struct xpoll *poll);

#endif

