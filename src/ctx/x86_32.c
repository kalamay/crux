#define XCTX_REG_COUNT 7

#define EBX 0
#define ESI 1
#define EDI 2
#define EBP 3
#define EIP 4
#define ESP 5
#define ECX 6

static uintptr_t *
start (uint8_t *stack, size_t len)
{
	uintptr_t *s = (uintptr_t *)(void *)(stack + len - sizeof (uintptr_t)*2);
	s = (void *)((uintptr_t)s - (uintptr_t)s%16);
	return s - 1;
}

void
xctx_init (uintptr_t *ctx, void *stack, size_t len,
		uintptr_t ip, uintptr_t a1, uintptr_t a2)
{
	uintptr_t *s = start (stack, len);
	*s = 0;

	s[1] = a1;
	s[2] = a2;
	ctx[EIP] = ip;
	ctx[ESP] = (uintptr_t)s;
}

size_t
xctx_stack_size (const uintptr_t *ctx, void *stack, size_t len, bool current)
{
	register uintptr_t *s = start (stack, len);
	register uintptr_t sp;
	if (current) {
		__asm__ ("movl %%esp, %0" : "=r" (sp));
	}
	else {
		sp = ctx[ESP];
	}
	return (uintptr_t)s - sp;
}

void
xctx_print (const uintptr_t *ctx, FILE *out)
{
	fprintf (out,
		"\tebx: 0x%08" PRIxPTR "\n"
		"\tesi: 0x%08" PRIxPTR "\n"
		"\tedi: 0x%08" PRIxPTR "\n"
		"\tebp: 0x%08" PRIxPTR "\n"
		"\teip: 0x%08" PRIxPTR "\n"
		"\tesp: 0x%08" PRIxPTR "\n"
		"\tecx: 0x%08" PRIxPTR "\n",
		ctx[EBX], ctx[ESI], ctx[EDI], ctx[EBP], ctx[EIP], ctx[ESP], ctx[ECX]);
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

