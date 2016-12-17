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

void
xctx_init (uintptr_t *ctx, void *stack, size_t len,
		uintptr_t ip, uintptr_t a1, uintptr_t a2)
{
	uintptr_t *s = (uintptr_t *)(void *)((uint8_t *)stack + len);
	s = (uintptr_t *)((uintptr_t)s - (uintptr_t)s%16) - 1;
	*s = 0;

	ctx[RDI] = a1;
	ctx[RSI] = a2;
	ctx[RIP] = ip;
	ctx[RSP] = (uintptr_t)s;
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

