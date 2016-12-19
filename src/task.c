#include "task.h"
#include "../include/crux/ctx.h"
#include "../include/crux/err.h"
#include "../include/crux/def.h"
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <dlfcn.h>
#include <assert.h>
#include <errno.h>

#if HAS_X86_64
# include "ctx/x86_64.c"
#elif HAS_X86_32
# include "ctx/x86_32.c"
#endif

#ifdef MAP_STACK
# define MAP_FLAGS (MAP_ANON|MAP_PRIVATE|MAP_STACK)
#else
# define MAP_FLAGS (MAP_ANON|MAP_PRIVATE)
#endif


/**
 * @brief  Stack size in bytes
 *
 * This is the stack size including the padding added for page boundary
 * rounding. This will also include the protected page if it is uses,
 * but it does not include the task local storage.
 *
 * @param  t  task pointer
 * @param  msz  map size
 * @param  tsz  tls size
 * @return  total size of the stack
 */
#define STACK_SIZE(t, msz, tsz) \
	((msz) - sizeof (struct xtask) - (tsz))

/**
 * @brief  Gets the task local storage
 *
 * @param  t  task pointer
 * @param  tsz  tls size
 * @return  local storage pointer
 */
#if STACK_GROWS_UP
# define TLS(t, tsz) \
	((void *t)((t) + 1))
#else
# define TLS(t, tsz) \
	((void *)((uint8_t *)(t) - (tsz)))
#endif


/**
 * @brief  Get the mapped address from a task
 *
 * @param  t    task pointer
 * @param  msz  map size
 * @return  mapped address
 */
#if STACK_GROWS_UP
# define MAP_BEGIN(t, msz) \
	((uint8_t *)(t))
#else
# define MAP_BEGIN(t, msz) \
	((uint8_t *)(t) + sizeof (struct xtask) - (msz))
#endif

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
	if (__builtin_expect (!(exp), 0)) { \
		struct xtask *t_tmp = (t); \
		fprintf (stderr, msg ":\n"); \
		xtask_print (t_tmp, stderr); \
		abort (); \
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
} __attribute__ ((aligned (16)));

static __thread struct xtask *current = NULL;
static __thread struct xtask top = {
#if HAS_DLADDR
	.entry = "main",
#endif
	.state = CURRENT,
	.istop = true
};

int
xmgr_new (struct xmgr **mgrp, size_t tls, size_t stack, int flags)
{
	struct xmgr *mgr = calloc (1, sizeof *mgr);
	if (mgr == NULL) { return -XERRNO; }
	int rc = xmgr_init (mgr, tls, stack, flags);
	if (rc < 0) { free (mgr); }
	else { *mgrp = mgr; }
	return rc;
}

int
xmgr_init (struct xmgr *mgr, size_t tls, size_t stack, int flags)
{
	if (stack < XTASK_STACK_MIN || stack > XTASK_STACK_MAX) {
		return -EINVAL;
	}
	if (tls > XTASK_TLS_MAX) {
		return -EINVAL;
	}

	size_t map_size;

	// round task size up to nearest multiple of 16
	tls = (tls + 15) & ~15;
	// exact size would be the stack, local storage, and task object
	map_size = stack + tls + sizeof (struct xtask);
	// round up to nearest page size
	map_size = (((map_size - 1) / PAGESIZE) + 1) * PAGESIZE;
	// add extra locked page if requested
	if (flags & XTASK_FPROTECT) { map_size += PAGESIZE; }

	mgr->map_size = map_size;
	mgr->stack_size = stack;
	mgr->tls_size = tls;
	mgr->flags = flags;

	return 0;
}

void
xmgr_free (struct xmgr **mgrp)
{
	assert (mgrp != NULL);

	struct xmgr *mgr = *mgrp;
	if (mgr == NULL) { return; }
	*mgrp = NULL;
	xmgr_final (mgr);
	free (mgr);
}

void
xmgr_final (struct xmgr *mgr)
{
	struct xdefer *free_defer = mgr->free_defer;
	while (free_defer != NULL) {
		struct xdefer *next = free_defer->next;
		free (free_defer);
		free_defer = next;
	}

	struct xtask *free_task = mgr->free_task;
	while (free_task != NULL) {
		struct xtask *next = free_task->parent;
		munmap (free_task, mgr->map_size);
		free_task = next;
	}
}

struct xmgr *
xmgr_self (void)
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
eol (struct xtask *t, union xvalue val, int ec)
{
	struct xmgr *mgr = t->mgr;
	struct xdefer *def = t->defer;
	t->defer = NULL;

	// prevent task from being resumed explicity
	t->exitcode = ec;
	t->state = EXIT;

	while (def != NULL) {
		struct xdefer *next = def->next;
		def->fn (def->data);
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
entry (struct xtask *t, union xvalue (*fn)(void *, union xvalue))
{
	struct xtask *p = t->parent;
	union xvalue val = fn (xtask_local (t), t->value);

	eol (t, val, 0);
	current = p;
	p->state = CURRENT;
	xctx_swap (&t->ctx, &p->ctx);
}

int
xtask_newf (struct xtask **tp, struct xmgr *mgr, void *tls,
		const char *file, int line,
		union xvalue (*fn)(void *tls, union xvalue))
{
	assert (tp != NULL);
	assert (mgr != NULL);
	assert (fn != NULL);

	struct xtask *t = mgr->free_task;
	size_t map_size = mgr->map_size;
	size_t tls_size = mgr->tls_size;
	uint8_t *map;

	if (t != NULL) {
		mgr->free_task = t->parent;
		map = MAP_BEGIN (t, map_size);
	}
	else {
		map = mmap (NULL, map_size, PROT_READ|PROT_WRITE, MAP_FLAGS, -1, 0);
		if (map == MAP_FAILED) { return -XERRNO; }
		if (mgr->flags & XTASK_FPROTECT) {
#if STACK_GROWS_UP
			int rc = mprotect (map+map_size-PAGESIZE, PAGESIZE, PROT_NONE);
#else
			int rc = mprotect (map, PAGESIZE, PROT_NONE);
#endif
			if (rc < 0) {
				rc = XERRNO;
				munmap (map, map_size);
				return rc;
			}
		}
#if STACK_GROWS_UP
		t = (struct xtask *)map;
#else
		t = (struct xtask *)(void *)(map + map_size - sizeof (*t));
#endif
	}

	t->value = XZERO;
	t->parent = NULL;
	t->mgr = mgr;
	t->defer = NULL;
	t->file = file;
	t->line = line;
	t->exitcode = -1;
	t->state = SUSPENDED;
	t->istop = false;

#if STACK_GROWS_UP
	map += sizeof (*t) + tls_size;
#endif

	xctx_init (&t->ctx, map, STACK_SIZE (t, map_size, tls_size),
			(uintptr_t)entry, (uintptr_t)t, (uintptr_t)fn);

#if HAS_DLADDR
	if (mgr->flags & XTASK_FENTRY) {
		Dl_info info;
		if (dladdr ((void *)fn, &info) > 0) {
			t->entry = info.dli_sname;
		}
	}
#endif

	if (tls_size > 0) {
		if (tls) { memcpy (TLS (t, tls_size), tls, tls_size); }
		else     { memset (TLS (t, tls_size), 0, tls_size); }
	}

	*tp = t;
	return 0;
}

void
xtask_free (struct xtask **tp)
{
	assert (tp != NULL);

	struct xtask *t = *tp;
	if (t == NULL) { return; }

	ensure (t, t->state != CURRENT, "attempting to free current task");
	ensure (t, t->state != ACTIVE, "attempting to free an active task");
	ensure (t, !t->istop, "attempting to free main task");

	*tp = NULL;

	eol (t, XZERO, t->exitcode);

	t->parent = t->mgr->free_task;
	t->mgr->free_task = t;
}

struct xtask *
xtask_self (void)
{
	return current;
}

void *
xtask_local (struct xtask *t)
{
	size_t tls_size = t->mgr->tls_size;
	return tls_size ? TLS (t, tls_size) : NULL;
}

bool
xtask_alive (const struct xtask *t)
{
	assert (t != NULL);

	return t->state != EXIT;
}

int
xtask_exitcode (const struct xtask *t)
{
	assert (t != NULL);

	return t->exitcode;
}

int
xtask_exit (struct xtask *t, int ec)
{
	bool yield;
	if (t == NULL) {
		t = current;
		if (t == NULL) { return -EPERM; }
		yield = true;
	}
	else {
		yield = t == current;
	}

	if (t->istop) { return -EPERM; }
	if (t->state == EXIT) { return -EALREADY; }

	struct xtask *p = t->parent;
	eol (t, XZERO, ec);
	if (yield) {
		current = p;
		p->state = CURRENT;
		xctx_swap (&t->ctx, &p->ctx);
	}

	return 0;
}

void
print_head (const struct xtask *t, FILE *out)
{
	if (out == NULL) { out = stdout; }
	if (t == NULL) {
		fprintf (out, 
				"<crux:task:(null)>");
		return;
	}

	fprintf (out, "<crux:task:%p", (void *)t);
	if (t->file != NULL) {
		fprintf (out, " %s:%d", t->file, t->line);
	}
#if HAS_DLADDR
	if (t->entry != NULL) {
		fprintf (out, " %s", t->entry);
	}
#endif
	fprintf (out, " %s tls=%u",
			state_names[t->state],
			t->istop ? 0 : t->mgr->tls_size);
	if (t->state == EXIT) {
		fprintf (out, " exitcode=%d>", t->exitcode);
	}
	else {
		fprintf (out, ">");
	}
}

extern int 
print_tree (const struct xtask *t, FILE *out)
{
	static const char line[] = "└── ";
	static const char space[] = "    ";

	int depth = 0;
	if (t->parent) {
		depth = print_tree (t->parent, out);
	}

	if (depth > 0) { fputc ('\n', out); }
	for (int i=1; i<depth; i++) { fwrite (space, 1, xlen (space) - 1, out); }
	if (depth > 0) { fwrite (line, 1, xlen (line) - 1, out); }
	print_head (t, out);

	return depth + 1;
}

void
xtask_print (const struct xtask *t, FILE *out)
{
	if (out == NULL) { out = stdout; }
	if (t == NULL) { print_head (t, out); }
	else           { print_tree (t, out); }
	fputc ('\n', out);
	fflush (out);
}

union xvalue
xyield (union xvalue val)
{
	struct xtask *t = current;
	ensure (t, t != NULL, "yield attempted outside of task");
	ensure (t, t->state != EXIT, "attempting to yield from exiting task");

	struct xtask *p = t->parent;
	ensure (t, p != NULL, "yield attempted outside of task");

	current = p;

	t->parent = NULL;
	t->value = val;
	t->state = SUSPENDED;
	if (p->state != EXIT) {
		p->state = CURRENT;
	}
	xctx_swap (&t->ctx, &p->ctx);
	return t->value;
}

union xvalue
xresume (struct xtask *t, union xvalue val)
{
	ensure (t, t != NULL, "attempting to resume a null task");
	ensure (t, t->state != CURRENT, "attempting to resume the current task");
	ensure (t, t->state != ACTIVE, "attempting to resume an active task");
	ensure (t, t->state < EXIT, "attempting to resume an exited task");

	struct xtask *p = current;
	if (p == NULL) { p = &top; }

	current = t;

	t->parent = p;
	t->value = val;
	t->state = CURRENT;
	if (p->state != EXIT) {
		p->state = ACTIVE;
	}
	xctx_swap (&p->ctx, &t->ctx);

	return t->value;
}

int
xdefer (void (*fn) (void *), void *data)
{
	assert (fn != NULL);

	struct xtask *t = current;
	ensure (NULL, t != NULL && !t->istop, "defer attempted outside of task");

	struct xmgr *mgr = t->mgr;
	struct xdefer *def = mgr->free_defer;
	if (def != NULL) {
		mgr->free_defer = def->next;
	}
	else {
		def = malloc (sizeof (*def));
		if (def == NULL) { return XERRNO; }
	}

	def->next = t->defer;
	def->fn = fn;
	def->data = data;
	t->defer = def;

	return 0;
}

static void *
defer_free (void *val)
{
	if (val != NULL && xdefer (free, val) < 0) {
		free (val);
		val = NULL;
	}
	return val;
}

void *
xmalloc (size_t size)
{
	return defer_free (malloc (size));
}

void *
xcalloc (size_t count, size_t size)
{
	return defer_free (calloc (count, size));
}

