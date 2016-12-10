#define XCTX_REG_COUNT 10

#define RBX 0
#define RBP 1
#define R12 2
#define R13 3
#define R14 4
#define R15 5
#define RDI 6
#define RSI 7
#define RIP 8
#define RSP 9

static uintptr_t *
start (uint8_t *stack, size_t len)
{
	uintptr_t *s = (uintptr_t *)(void *)(stack + len);
	s = (void *)((uintptr_t)s - (uintptr_t)s%16);
	return s - 1;
}

void
xctx_init (uintptr_t *ctx, void *stack, size_t len,
		uintptr_t ip, uintptr_t a1, uintptr_t a2)
{
	uintptr_t *s = start (stack, len);
	*s = 0;

	ctx[RDI] = a1;
	ctx[RSI] = a2;
	ctx[RIP] = ip;
	ctx[RSP] = (uintptr_t)s;
}

size_t
xctx_stack_size (const uintptr_t *ctx, void *stack, size_t len, bool current)
{
	register uintptr_t *s = start (stack, len);
	register uintptr_t sp;
	if (current) {
		__asm__ ("movq %%rsp, %0" : "=r" (sp));
	}
	else {
		sp = ctx[RSP];
	}
	return (uintptr_t)s - sp;
}

void
xctx_print (const uintptr_t *ctx, FILE *out)
{
	fprintf (out,
		"\trbx: 0x%016" PRIxPTR "\n"
		"\trbp: 0x%016" PRIxPTR "\n"
		"\tr12: 0x%016" PRIxPTR "\n"
		"\tr13: 0x%016" PRIxPTR "\n"
		"\tr14: 0x%016" PRIxPTR "\n"
		"\tr15: 0x%016" PRIxPTR "\n"
		"\trdi: 0x%016" PRIxPTR "\n"
		"\trsi: 0x%016" PRIxPTR "\n"
		"\trip: 0x%016" PRIxPTR "\n"
		"\trsp: 0x%016" PRIxPTR "\n",
		ctx[RBX], ctx[RBP], ctx[R12], ctx[R13], ctx[R14],
		ctx[R15], ctx[RDI], ctx[RSI], ctx[RIP], ctx[RSP]);
}

__asm__ (
	".text             \n"
#if defined (__APPLE__)
	".globl _xctx_swap \n"
	"_xctx_swap:       \n"
#else
	".globl xctx_swap  \n"
	"xctx_swap:        \n"
#endif
		"movq      %rbx,    0(%rdi) \n"
		"movq      %rbp,    8(%rdi) \n"
		"movq      %r12,   16(%rdi) \n"
		"movq      %r13,   24(%rdi) \n"
		"movq      %r14,   32(%rdi) \n"
		"movq      %r15,   40(%rdi) \n"
		"movq      %rdi,   48(%rdi) \n"
		"movq      %rsi,   56(%rdi) \n"
		"movq     (%rsp),     %rcx  \n"
		"movq      %rcx,   64(%rdi) \n"
		"leaq    8(%rsp),     %rcx  \n"
		"movq      %rcx,   72(%rdi) \n"
		"movq   72(%rsi),     %rsp  \n"
		"movq    0(%rsi),     %rbx  \n"
		"movq    8(%rsi),     %rbp  \n"
		"movq   16(%rsi),     %r12  \n"
		"movq   24(%rsi),     %r13  \n"
		"movq   32(%rsi),     %r14  \n"
		"movq   40(%rsi),     %r15  \n"
		"movq   48(%rsi),     %rdi  \n"
		"movq   64(%rsi),     %rcx  \n"
		"movq   56(%rsi),     %rsi  \n"
		"jmp      *%rcx             \n"
);

