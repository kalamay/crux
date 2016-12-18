#ifndef CRUX_TASK_H
#define CRUX_TASK_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "value.h"

#define XTASK_FPROTECT   (UINT32_C(1) << 0) /** protect the end of the stack */
#define XTASK_FENTRY     (UINT32_C(1) << 1) /** capture entry point name */

/**
 * @brief  Minimum allowed stack size
 */
#define XTASK_STACK_MIN 16384

/**
 * @brief  Maximum allowed stack size
 */
#define XTASK_STACK_MAX (1024 * XTASK_STACK_MIN)

/**
 * @brief  Stack size just large enough to call all of glibc functions
 */
#define XTASK_STACK_DEFAULT (8 * XTASK_STACK_MIN)

/**
 * @brief  Flag combination ideal for general use
 */
#define XTASK_FDEFAULT (XTASK_FPROTECT)

/**
 * @brief  Flag combination ideal for debugging purposed
 */
#define XTASK_FDEBUG (XTASK_FPROTECT|XTASK_FENTRY)

/**
 * @brief  Opaque type for task manager instance
 */
struct xmgr;

/**
 * @brief  Creates a new task manager for constructing and using tasks
 *
 * @param[out]  mgrp   reference to the manager pointer to create
 * @param  stack  minimum amount of stack space for each task
 * @param  tls    task local storage space for each task
 * @param  flags  flags for task flags
 * @return  0 on succes, -errno on error
 */
extern int
xmgr_new (struct xmgr **mgrp, size_t stack, size_t tls, int flags);

/**
 * @brief  Frees a task manager
 *
 * `mgrp` cannot be `NULL`, but `*mgrp` may be.
 *
 * @param  mgrp  reference to the manager pointer to free
 */
extern void
xmgr_free (struct xmgr **mgrp);

extern struct xmgr *
xmgr_self (void);

/**
 * @brief  Opaque type for task instances
 */
struct xtask;

/**
 * @brief  Creates a new task with optional initial local storage
 *
 * This will also capture the file and line where the task was created.
 *
 * @see  `xtask_newf` for more information.
 *
 * @param  tp    reference to the task pointer to create
 * @param  mgr   task manager pointer
 * @param  tls   task local storage reference to copy or `NULL`
 * @param  fn    the function to execute in the new context
 * @return  0 on success, -errno on error
 */
#define xtask_new(tp, mgr, tls, fn) \
	xtask_newf (tp, mgr, tls, __FILE__, __LINE__, fn)

/**
 * @brief  Creates a new task with optional initial local storage
 *
 * The newly created task will be in a suspended state. Calling
 * `resume` on the returned value will transfer execution context
 * to the function `fn`.
 *
 * Task local storage may optionally be included. This size of the
 * the local storage area is specified by the task mananger. The
 * region is guaranteed to be 16-byte aligned. If a `tls` value is
 * provided during task construction, the value pointed at by `tls`
 * will be copied into this storage. This region may be changed at
 * any time, however.
 *
 * The mapping for the task is a single contiguous region with the
 * task state struct stored towards the beginning fo the stack.
 * If the stack is protected, the last page will be locked.
 *
 * For downward growing stacks, the mapping layout looks like:
 *
 *     ┏━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━┳━━━━━┳━━━━━━┓
 *     ┃ lock ┃                stack ┃ tls ┃ task ┃
 *     ┗━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━┻━━━━━┻━━━━━━┛
 *
 * For upward growing stack, the mapping looks like:
 *
 *     ┏━━━━━━┳━━━━━┳━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━┓
 *     ┃ task ┃ tls ┃ stack                ┃ lock ┃
 *     ┗━━━━━━┻━━━━━┻━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━┛
 *
 * @param  tp    reference to the task pointer to create
 * @param  mgr   task manager pointer
 * @param  tls   task local storage reference to copy or `NULL`
 * @param  file  source file path
 * @param  line  source file line number
 * @param  fn    the function to execute in the new context
 * @return  0 on success, -errno on error
 */
extern int
xtask_newf (struct xtask **tp, struct xmgr *mgr, void *tls,
		const char *file, int line,
		union xvalue (*fn)(void *tls, union xvalue));

/**
 * @brief  Frees an inactive task
 *
 * This returns the memory allocated for the task. This may not actually
 * return the memory to the OS, and the stack may be reused later.
 *
 * `tp` cannot be `NULL`, but `*tp` may be.
 *
 * @param  tp  reference to the task pointer to free
 */
extern void
xtask_free (struct xtask **tp);

/**
 * @brief  Gets the active task for the current thread
 *
 * @return  task or `NULL` if none are active
 */
extern struct xtask *
xtask_self (void);

/**
 * @brief  Gets the task local storage if it was create with some
 *
 * @param  t  the task access storage for
 * @return  pointer to storage or `NULL`
 */
extern void *
xtask_local (struct xtask *t);

/**
 * @brief  Checks if a task is not dead
 *
 * This returns `true` if the state is suspended, current, or active. In
 * other words, this is similar to:
 *
 *     xtask_state (t) != KD_TASK_DEAD
 *
 * The one difference is that a `NULL` task is not considered alive,
 * whereas `xtask_state` expects a non-NULL task.
 *
 * @param  t  the task to test or `NULL` for the current task
 * @return  `true` if alive, `false` if dead or exited
 */
extern bool
xtask_alive (const struct xtask *t);

/**
 * @brief  Gets the exit code of the task
 *
 * @param  t  the task to test or `NULL` for the current task
 * @return  exit code or -1 if not exited
 */
extern int
xtask_exitcode (const struct xtask *t);

/**
 * @brief  Exits a task
 *
 * @param  t   the task to exit or `NULL` for the current task
 * @param  ec  exit code
 * @return  0 on succes, -errno of failure
 */
extern int
xtask_exit (struct xtask *t, int ec);

/**
 * @brief  Prints a representation of the task
 *
 * @param  t    the task to print or `NULL` for the current task
 * @param  out  `FILE *` handle to write to or `NULL`
 */
extern void
xtask_print (const struct xtask *t, FILE *out);

/**
 * @brief  Gives up context from the current task
 *
 * This will return context to the parent task that called `resume`.
 * `val` will become the return value to `resume`, allowing a value to be
 * communicated back to the calling context.
 *
 * The returned value is the value passed into the context to `resume`.
 *
 * @param  val  value to send to the context
 * @return  value passed into `resume`
 */
extern union xvalue
xyield (union xvalue val);

/**
 * @brief  Gives context to en explicit task
 *
 * If this is the first activation of the task, `val` will be the input
 * parameter to the task's function. If this is a secondary  activation,
 * `val` will be returned from the `yield` call within the the task's
 * function.
 *
 * The returned value is either the value passed into `yield` or the
 * final returned value from the task function.
 *
 * @param  t    task to activate
 * @param  val  value to pass to the task
 */
extern union xvalue
xresume (struct xtask *t, union xvalue val);

#endif

