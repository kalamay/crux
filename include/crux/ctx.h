#ifndef CRUX_CTX_H
#define CRUX_CTX_H

#include "def.h"

#include <stdio.h>

struct xctx;

/**
 * @brief  Configures the context to invoke a function with 2 arguments
 *
 * @param  ctx    context pointer
 * @param  stack  lowest address of the stack
 * @param  len    byte length of the stack
 * @param  ip     function to execute
 * @param  a1     first argument to `ip`
 * @param  a2     second argument to `ip`
 */
XEXTERN void
xctx_init (struct xctx *ctx, void *stack, size_t len,
		uintptr_t ip, uintptr_t a1, uintptr_t a2);

/**
 * @brief  Swaps execution contexts
 *
 * @param  save  destination to save current context
 * @param  load  context to activate
 */
XEXTERN void
xctx_swap (struct xctx *save, const struct xctx *load);

#endif

