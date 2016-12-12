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
 * @brief  The global stack size for `xtask_new_fn` and `xtask_new_tls`
 */
extern uint32_t XTASK_STACK_SIZE;

/**
 * @brief  The global flags for `xtask_new_fn` and `xtask_new_tls`
 */
extern uint32_t XTASK_FLAGS;

/**
 * @brief  Opaque type for task instances
 */
struct xtask;

/**
 * @brief  Creates a new task with a function for execution context
 *
 * This will also capture the file and line where the task was created.
 *
 * @see  `xtask_new` for more information. This variant uses the global
 * stack size and flags, and the local storage space is not used.
 *
 * @param  tp    reference to the task pointer to create
 * @param  fn    the function to execute in the new context
 * @return  0 on succes, -errno on error
 */
#define xtask_new_fn(tp, fn) \
	xtask_new_tls (tp, 0, 0, fn)

/**
 * @brief  Creates a new task with a function and local storage
 *
 * This will also capture the file and line where the task was created.
 *
 * @see  `xtask_new` for more information. This variant uses the global
 * stack size and flags.
 *
 * @param  tp    reference to the task pointer to create
 * @param  tls   task local storage reference to copy or `NULL`
 * @param  len   length of `tls` in bytes
 * @param  fn    the function to execute in the new context
 * @return  0 on succes, -errno on error
 */
#define xtask_new_tls(tp, tls, len, fn) \
	xtask_new_opt (tp, XTASK_STACK_SIZE, XTASK_FLAGS, tls, len, fn)

/**
 * @brief  Creates a new task using per-task configuration options
 *
 * This will also capture the file and line where the task was created.
 *
 * @see  `xtask_new` for more information.
 *
 * @param  tp     reference to the task pointer to create
 * @param  stack  minimum amount of stack space to allocate
 * @param  flags  task flags
 * @param  tls    task local storage reference to copy or `NULL`
 * @param  len    length of `tls` in bytes
 * @param  fn     the function to execute in the new context
 * @return  0 on success, -errno on error
 */
#define xtask_new_opt(tp, stack, flags, tls, len, fn) __extension__ ({ \
	struct xtask *__t; \
	int __rc = xtask_new (&__t, (stack), (flags), (tls), (len), (fn)); \
	if (__rc == 0) { xtask_set_file (__t, __FILE__, __LINE__); (*tp) = __t; } \
	__rc; \
})

/**
 * @brief  Creates a new task using per-task configuration options
 *
 * The newly created task will be in a suspended state. Calling
 * `resume` on the returned value will transfer execution context
 * to the function `fn`.
 *
 * Task local storage may optionally be included. This allows an
 * arbitrarily sized region to be reserved for storing values
 * associated with the task. The region is guaranteed to be 16-byte
 * aligned. The `len` parameter defines the number of bytes to reserve.
 * If `tls` is not `NULL`, the value pointed to will be copied into
 * this space. This region may be changed at any time, however.
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
 * @param  tp     reference to the task pointer to create
 * @param  stack  minimum amount of stack space to allocate
 * @param  flags  task flags
 * @param  tls    task local storage reference to copy or `NULL`
 * @param  len    length of `tls` in bytes
 * @param  fn     the function to execute in the new context
 * @return  0 on success, -errno on error
 */
extern int
xtask_new (struct xtask **tp,
		size_t stack, int flags,
		void *tls, size_t len,
		union xvalue (*fn)(void *tls, union xvalue));

/**
 * @brief  Sets file and line information for the task
 *
 * This is intended to help with debugging. This information is only used
 * when printing a representation of the task.
 *
 * The `file` parameter is NOT copied.
 *
 * @param  t     task pointer
 * @Param  file  file name string
 * @param  line  file line number
 */
extern void
xtask_set_file (struct xtask *t, const char *file, int line);

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
 * @brief  Gets the current stack space used
 *
 * @param  t  the task to access
 * @return  number of bytes used
 */
extern size_t
xtask_stack_used (const struct xtask *t);

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

