#ifndef CRUX_POLL_H
#define CRUX_POLL_H

#include "def.h"
#include "clock.h"

#include <signal.h>

#define XPOLL_NONE 0
#define XPOLL_IN  (1<<0)  /** Type for file descriptor readability */
#define XPOLL_OUT (1<<1)  /** Type for file descriptor writeability */
#define XPOLL_SIG (1<<2)  /** Type for signal readyness */
#define XPOLL_ERR (1<<16) /** Flag to indicate an error with the event */
#define XPOLL_EOF (1<<17) /** Flag to indicate an end-of-file state */

#define XPOLL_INOUT (XPOLL_IN|XPOLL_OUT)
#define XPOLL_ANY (XPOLL_INOUT|XPOLL_SIG)

#define XPOLL_TYPE(n)  (int)((n) & 0xFFFF)
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
xpoll_new(struct xpoll **pollp);

/**
 * @brief  Finalizes and deallocates an idirectly referenced poll object
 * 
 * The objected pointed to by `*pollp` may be NULL, but `pollp` must point
 * to a valid address. The `*pollp` address will be set to NULL.
 *
 * @param  pollp  indirect poll object pointer
 */
XEXTERN void
xpoll_free(struct xpoll **pollp);

/**
 * @brief  Performs control operations on the poll
 *
 * This is used to add and remove events of interest from the poll.
 *
 * @param  poll     poll pointer
 * @param  id       file descriptor or signal number
 * @param  oldtype  current event type (XPOLL_IN, XPOLL_OUT, or XPOLL_SIG)
 * @param  newtype  desired event type (XPOLL_IN, XPOLL_OUT, or XPOLL_SIG)
 * @return  0 on sucess, -errno on error
 *
 * Errors:
 *   `-EINVAL`: the operation, type, or signal number are invalid
 *   `-EBADF`: the specified descriptor is invalid
 *   `-EEXIST`: the event already exists
 *   `-ENOENT`: the event could not be found to be modified or deleted
 *   `-ENOMEM`: no memory was available to register the event
 */
XEXTERN int
xpoll_ctl(struct xpoll *poll, int id, int oldtype, int newtype);

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
xpoll_wait(struct xpoll *poll, int64_t ms, struct xevent *ev);

/**
 * @brief  Gets the monotonic clock associated with the poll
 *
 * This clock gets updated just prior to yielding an event from `xpoll_wait`.
 *
 * @param  poll  poll pointer
 * @return  clock pointer
 */
XEXTERN const struct timespec *
xpoll_clock(const struct xpoll *poll);

#endif

