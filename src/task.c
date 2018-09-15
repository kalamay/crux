#include "../include/crux.h"
#include "../include/crux/ctx.h"
#include "../include/crux/err.h"
#include "task.h"

#if WITH_HUB
# include "../include/crux/hub.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <assert.h>

#if HAS_X86_64
# include "ctx/x86_64.c"
#elif HAS_X86_32
# include "ctx/x86_32.c"
#endif

#define MAP_FLAGS (MAP_ANON|MAP_PRIVATE)

/**
 * @brief  Gets the task local storage
 *
 * @param  t  task pointer
 * @param  tsz  tls size
 * @return  local storage pointer
 */
#define TLS(t, tsz) \
	((void *)((uint8_t *)(t) - (tsz)))

#define SUSPENDED 0  /** new created or yielded */
#define CURRENT   1  /** currently has context */
#define ACTIVE    2  /** is in the parent list of the current */
#define EXIT      3  /** function has returned */

/**
 * @brief  Maps state integers to name strings
 * */
static const char *state_names[] = {
	[SUSPENDED] = "SUSPENDED",
	[CURRENT]   = "CURRENT",
	[ACTIVE]    = "ACTIVE",
	[EXIT]      = "EXIT",
};

/**
 * @brief  Runtime assert that prints error and task information before aborting
 *
 * @param  t    task pointer
 * @param  exp  expression to ensure is `true`
 * @param  msg  error message
 */
#define ensure(t, exp, msg) do { \
	if (__builtin_expect(!(exp), 0)) { \
		struct xtask *t_tmp = (t); \
		fprintf(stderr, msg ":\n"); \
		xtask_print(t_tmp, stderr); \
		abort(); \
	} \
} while (0)

struct xtask {
	union xvalue value;            /** the resume or yield value */
	struct xtask *parent;          /** task that resumed this task */
	struct xmgr *mgr;              /** task manager for this task */
	struct xdefer *defer;          /** defered execution linked list */
	struct xctx ctx;               /** execution registers */
#if HAS_DLADDR
	const char *entry;             /** entry function name */
#endif
	const char *file;              /** allocation source file name */
	int line;                      /** allocation source line number */
	int16_t exitcode;              /** code if in EXIT state */
	uint8_t state;                 /** SUSPENDED, CURRENT, ACTIVE, or EXIT */
	bool istop;
} __attribute__ ((aligned(16)));

static thread_local struct xtask *current = NULL;
static thread_local struct xtask top = {
#if HAS_DLADDR
	.entry = "main",
#endif
	.state = CURRENT,
	.istop = true
};

int
xmgr_new(struct xmgr **mgrp, size_t tls, size_t stack, int flags)
{
	return xnew(xmgr_init, mgrp, tls, stack, flags);
}

int
xmgr_init(struct xmgr *mgr, size_t tls, size_t stack, int flags)
{
	if (stack < XSTACK_MIN || stack > XSTACK_MAX) {
		return xerr_sys(EINVAL);
	}
	if (tls > XTASK_TLS_MAX) {
		return xerr_sys(EINVAL);
	}

	size_t tls_size, map_size;

	// round task size up to nearest multiple of 16
	tls_size = (tls + 15) & ~15;
	// exact size would be the stack, local storage, and task object
	map_size = stack + tls_size + sizeof(struct xtask);
	// round up to nearest page size
	map_size = (((map_size - 1) / xpagesize) + 1) * xpagesize;
	// add extra locked page if requested
	if (flags & XTASK_FPROTECT) { map_size += xpagesize; }

	mgr->map_size = map_size;
	mgr->stack_size = stack;
	mgr->tls_size = tls_size;
	mgr->tls_user = tls;
	mgr->flags = flags;
	mgr->free_defer = NULL;
	mgr->free_task = NULL;

	return 0;
}

void
xmgr_free(struct xmgr **mgrp)
{
	assert(mgrp != NULL);

	xfree(xmgr_final, mgrp);
}

void
xmgr_final(struct xmgr *mgr)
{
	struct xdefer *free_defer = mgr->free_defer;
	while (free_defer != NULL) {
		struct xdefer *next = free_defer->next;
		free(free_defer);
		free_defer = next;
	}

	struct xtask *free_task = mgr->free_task;
	while (free_task != NULL) {
		struct xtask *next = free_task->parent;
		munmap(free_task, mgr->map_size);
		free_task = next;
	}
}

struct xmgr *
xmgr_self(void)
{
	struct xtask *t = current;
	return t ? t->mgr : NULL;
}

/**
 * @brief  Executes end-of-life tasks
 *
 * @param  t    task to finalize
 * @param  val  value if the final yield
 * @param  ec   exit code
 */
static void
eol(struct xtask *t, union xvalue val, int ec)
{
	struct xmgr *mgr = t->mgr;
	struct xdefer *def = t->defer;
	t->defer = NULL;

	// prevent task from being resumed explicity
	t->exitcode = ec;
	t->state = EXIT;

	while (def != NULL) {
		struct xdefer *next = def->next;
		def->fn(def->val);
		def->next = mgr->free_defer;
		mgr->free_defer = def;
		def = next;

		// mark exit status again in case defer resumed a separate task
		t->exitcode = ec & 0xFF;
		t->state = EXIT;
	}

	// disable the ability to yield
	t->parent = NULL;
	t->value = val;
}

/**
 * @brief  Entry point for a new task
 *
 * This runs the user function, kills the task, and restores the
 * parent context.
 *
 * @param  t   task pointer
 * @param  fn  function for the body of the task
 */
static void
entry(struct xtask *t, union xvalue (*fn)(void *, union xvalue))
{
	struct xtask *p = t->parent;
	union xvalue val = fn(xtask_local(t), t->value);

	eol(t, val, 0);
	current = p;
	p->state = CURRENT;
	xctx_swap(&t->ctx, &p->ctx);
}

int
xtask_newf(struct xtask **tp, struct xmgr *mgr, void *tls,
		const char *file, int line,
		union xvalue (*fn)(void *tls, union xvalue))
{
	assert(tp != NULL);
	assert(mgr != NULL);
	assert(fn != NULL);

	struct xtask *t = mgr->free_task;
	size_t map_size = mgr->map_size;
	size_t tls_size = mgr->tls_size;
	uint8_t *stack;

	if (t != NULL) {
		mgr->free_task = t->parent;
		stack = (uint8_t *)(void *)t + sizeof(*t) - map_size;
	}
	else {
		uint8_t *map = mmap(NULL, map_size, PROT_READ|PROT_WRITE, MAP_FLAGS, -1, 0);
		if (map == MAP_FAILED) { return -xerrno; }
		if (mgr->flags & XTASK_FPROTECT) {
			int rc = mprotect(map, xpagesize, PROT_NONE);
			if (rc < 0) {
				rc = xerrno;
				munmap(map, map_size);
				return rc;
			}
		}
		stack = map;
		map += map_size;
		t = (struct xtask *)(void *)(map - sizeof(*t));
	}

	t->value = xzero;
	t->parent = NULL;
	t->mgr = mgr;
	t->defer = NULL;
	t->file = file;
	t->line = line;
	t->exitcode = -1;
	t->state = SUSPENDED;
	t->istop = false;

	xctx_init(&t->ctx, stack, map_size - sizeof(*t) - tls_size,
			(uintptr_t)entry, (uintptr_t)t, (uintptr_t)fn);

	if (mgr->flags & XTASK_FENTRY) {
		xtask_record_entry(t, (void *)fn);
	}

	if (tls_size > 0) {
		memset(TLS(t, tls_size), 0, tls_size);
		if (tls) {
			memcpy(TLS(t, tls_size), tls, mgr->tls_user);
		}
	}

	*tp = t;
	return 0;
}

void
xtask_record_entry(struct xtask *t, void *fn)
{
#if HAS_DLADDR
	Dl_info info;
	if (dladdr(fn, &info) > 0) {
		t->entry = info.dli_sname;
	}
#else
	(void)t;
	(void)fn;
#endif
}

void
xtask_free(struct xtask **tp)
{
	assert(tp != NULL);

	struct xtask *t = *tp;
	if (t == NULL) { return; }

	ensure(t, t->state != CURRENT, "attempting to free current task");
	ensure(t, t->state != ACTIVE, "attempting to free an active task");
	ensure(t, !t->istop, "attempting to free main task");

	*tp = NULL;

	eol(t, xzero, t->exitcode);

	t->parent = t->mgr->free_task;
	t->mgr->free_task = t;
}

struct xtask *
xtask_self(void)
{
	return current;
}

void *
xtask_local(struct xtask *t)
{
	size_t tls_size = t->mgr->tls_size;
	return tls_size ? TLS(t, tls_size) : NULL;
}

bool
xtask_alive(const struct xtask *t)
{
	assert(t != NULL);

	return t->state != EXIT;
}

int
xtask_exitcode(const struct xtask *t)
{
	assert(t != NULL);

	return t->exitcode;
}

int
xtask_exit(struct xtask *t, int ec)
{
	bool yield;
	if (t == NULL) {
		t = current;
		if (t == NULL) { return xerr_sys(EPERM); }
		yield = true;
	}
	else {
		yield = t == current;
	}

	if (t->istop) { return xerr_sys(EPERM); }
	if (t->state == EXIT) { return xerr_sys(EALREADY); }

	struct xtask *p = t->parent;
	eol(t, xzero, ec);
	if (yield) {
		current = p;
		p->state = CURRENT;
		xctx_swap(&t->ctx, &p->ctx);
	}

	return 0;
}


void
print_head(const struct xtask *t, FILE *out, int indent)
{
	static const char space[] = "  ";
	static const char line[] = "└ ";

	for (int i = t->parent ? 1 : 0; i < indent; i++) {
		fwrite(space, 1, sizeof(space) - 1, out);
	}
	if (t->parent) {
		fwrite(line, 1, sizeof(line) - 1, out);
	}

	if (t == NULL) {
		fprintf(out, "<crux:task:(null)>");
		return;
	}

	fprintf(out, "<crux:task:%p", (void *)t);
	if (t->file != NULL) {
		fprintf(out, " %s:%d", t->file, t->line);
	}
#if HAS_DLADDR
	if (t->entry != NULL) {
		fprintf(out, " %s", t->entry);
	}
#endif
	fprintf(out, " %s tls=%u",
			state_names[t->state],
			t->istop ? 0 : t->mgr->tls_user);
	if (t->state == EXIT) {
		fprintf(out, " exitcode=%d>", t->exitcode);
	}
	else {
		fprintf(out, ">");
	}
}

int 
print_tree(const struct xtask *t, FILE *out, int indent)
{
	int depth = indent;
	if (t->parent) {
		depth = print_tree(t->parent, out, indent);
		fputc('\n', out);
	}

	print_head(t, out, depth);

	return depth + 1;
}

void
xtask_print(const struct xtask *t, FILE *out)
{
	if (out == NULL) { out = stdout; }
	xtask_print_val(t, out, 0);
	fputc('\n', out);
	fflush(out);
}

void
xtask_print_val(const struct xtask *t, FILE *out, int indent)
{
	if (t == NULL) {
		print_head(t, out, indent);
	}
	else {
		print_tree(t, out, indent);
	}
}

union xvalue
xyield(union xvalue val)
{
	struct xtask *t = current;
	ensure(t, t != NULL, "yield attempted outside of task");
	ensure(t, t->state != EXIT, "attempting to yield from exiting task");

	struct xtask *p = t->parent;
	ensure(t, p != NULL, "yield attempted outside of task");

	current = p;

	t->parent = NULL;
	t->value = val;
	t->state = SUSPENDED;
	if (p->state != EXIT) {
		p->state = CURRENT;
	}
	xctx_swap(&t->ctx, &p->ctx);
	return t->value;
}

union xvalue
xresume(struct xtask *t, union xvalue val)
{
	ensure(t, t != NULL, "attempting to resume a null task");
	ensure(t, t->state != CURRENT, "attempting to resume the current task");
	ensure(t, t->state != ACTIVE, "attempting to resume an active task");
	ensure(t, t->state < EXIT, "attempting to resume an exited task");

	struct xtask *p = current;
	if (p == NULL) { p = &top; }

	current = t;

	t->parent = p;
	t->value = val;
	t->state = CURRENT;
	if (p->state != EXIT) {
		p->state = ACTIVE;
	}
	xctx_swap(&p->ctx, &t->ctx);

	return t->value;
}

int
xdefer(void (*fn)(union xvalue), union xvalue val)
{
	assert(fn != NULL);

	struct xtask *t = current;
	ensure(NULL, t != NULL && !t->istop, "defer attempted outside of task");

	struct xmgr *mgr = t->mgr;
	struct xdefer *def = mgr->free_defer;
	if (def != NULL) {
		mgr->free_defer = def->next;
	}
	else {
		def = malloc(sizeof(*def));
		if (def == NULL) { return xerrno; }
	}

	def->next = t->defer;
	def->fn = fn;
	def->val = val;
	t->defer = def;

	return 0;
}

#if defined(__BLOCKS__)

#include <Block.h>

static void
defer_block(union xvalue val)
{
	void (^block)(void) = val.ptr;
	block();
	Block_release(block);
}

int
xdefer_b(void (^block)(void))
{
	void (^copy)(void) = Block_copy(block);
	int rc = xdefer(defer_block, xptr(copy));
	if (rc < 0) {
		Block_release(copy);
	}
	return rc;
}

#endif

static void
close_fd(union xvalue val)
{
#if WITH_HUB
	xclose(val.i);
#else
	xretry(close(val.i));
#endif
}

int
xdefer_close(int fd)
{
	return xdefer(close_fd, xint(fd));
}

static void
free_ptr(union xvalue val)
{
	free(val.ptr);
}

int
xdefer_free(void *ptr)
{
	if (ptr == NULL) { return 0; }
	return xdefer(free_ptr, xptr(ptr));
}

static void *
defer_free(void *ptr)
{
	if (ptr == NULL) {
		xerr_abort(xerrno);
	}

	int rc = xdefer(free_ptr, xptr(ptr));
	if (rc < 0) {
		free(ptr);
		xerr_abort(rc);
	}

	return ptr;
}

void *
xmalloc(size_t size)
{
	return defer_free(malloc(size));
}

void *
xcalloc(size_t count, size_t size)
{
	return defer_free(calloc(count, size));
}

static void
free_buf(union xvalue val)
{
	struct xbuf *buf = val.ptr;
	xbuf_free(&buf);
}

struct xbuf *
xbuf(size_t cap, bool ring)
{
	struct xbuf *buf;
	int rc;

	if ((rc = xbuf_new(&buf, cap, ring)) < 0) {
		xerr_abort(rc);
	}

	if ((rc = xdefer(free_buf, xptr(buf))) < 0) {
		xbuf_free(&buf);
		xerr_abort(rc);
	}

	return buf;
}

