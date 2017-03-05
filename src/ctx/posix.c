#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define _XOPEN_SOURCE 1
#define _XOPEN_SOURCE_EXTENDED 1
#include <ucontext.h>

struct xctx {
	ucontext_t uctx;
};

void
xctx_init(struct xctx *ctx, void *stack, size_t len,
		uintptr_t ip, uintptr_t a1, uintptr_t a2)
{
	getcontext(&ctx->uctx);
	// ctx->uctx.uc_link = ...
	ctx->uctx.uc_stack.ss_sp = stack;
	ctx->uctx.uc_stack.ss_size = len;
	makecontext(&ctx->uctx, (void (*) (void))ip, 2, a1, a2);
}

void
xctx_swap(struct xctx *save, const struct xctx *load)
{
	swapcontext(&save->uctx, &load->uctx);
}

#pragma GCC diagnostic pop

