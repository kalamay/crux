struct xctx {
	uintptr_t ebx;
	uintptr_t esi;
	uintptr_t edi;
	uintptr_t ebp;
	uintptr_t eip;
	uintptr_t esp;
	uintptr_t ecx;
};

void
xctx_init (struct xctx *ctx, void *stack, size_t len,
		uintptr_t ip, uintptr_t a1, uintptr_t a2)
{
	uintptr_t *s = (uintptr_t *)(void *)((uint8_t *)stack + len - sizeof (uintptr_t)*2);
	s = (uintptr_t *)((uintptr_t)s - (uintptr_t)s%16) - 1;
	s[0] = 0;
	s[1] = a1;
	s[2] = a2;
	ctx->eip = ip;
	ctx->esp = (uintptr_t)s;
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
		"movl    4(%esp),     %eax  \n"
		"movl      %ecx,   24(%eax) \n"
		"movl      %ebx,    0(%eax) \n"
		"movl      %esi,    4(%eax) \n"
		"movl      %edi,    8(%eax) \n"
		"movl      %ebp,   12(%eax) \n"
		"movl     (%esp),     %ecx  \n"
		"movl      %ecx,   16(%eax) \n"
		"leal    4(%esp),     %ecx  \n"
		"movl      %ecx,   20(%eax) \n"
		"movl    8(%esp),     %eax  \n"
		"movl   16(%eax),     %ecx  \n"
		"movl   20(%eax),     %esp  \n"
		"pushl     %ecx             \n"
		"movl    0(%eax),     %ebx  \n"
		"movl    4(%eax),     %esi  \n"
		"movl    8(%eax),     %edi  \n"
		"movl   12(%eax),     %ebp  \n"
		"movl   24(%eax),     %ecx  \n"
		"ret                        \n"
);

