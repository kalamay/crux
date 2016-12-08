#include "../include/crux/task.h"
#include "../include/crux/ctx.h"
#include "../include/crux/err.h"
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <dlfcn.h>
#include <assert.h>
#include <errno.h>

#if HAS_EXECINFO
# include <execinfo.h>
#endif

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
 * The stack size in bytes including the extra space below the corotoutine
 * and the protected page
 *
 * @param  t  task pointer
 * @return  total size of the stack
 */
#define STACK_SIZE(t) \
	((t)->map_size - sizeof (struct xtask) - (t)->tls)

/**
 * Gets the task local storage
 *
 * @param  t  task pointer
 * @return  local storage pointer
 */
#if STACK_GROWS_UP
# define TLS(t) \
	((void *t)((t) + 1))
#else
# define TLS(t) \
	((void *)((uint8_t *)(t) - (t)->tls))
#endif


/**
 * Get the mapped address from a task
 *
 * @param  t  task pointer
 * @return  mapped address
 */
#if STACK_GROWS_UP
# define MAP_BEGIN(t) \
	((uint8_t *)(t))
#else
# define MAP_BEGIN(t) \
	((uint8_t *)(t) + sizeof (struct xtask) - (t)->map_size)
#endif

#define SUSPENDED 0  /** new created or yielded */
#define CURRENT   1  /** currently has context */
#define ACTIVE    2  /** is in the parent list of the current */
#define DEAD      3  /** function has returned */

/** Maps state integers to name strings */
static const char *state_names[] = {
	[SUSPENDED] = "SUSPENDED",
	[CURRENT]   = "CURRENT",
	[ACTIVE]    = "ACTIVE",
	[DEAD]      = "DEAD",
};

/**
 * Runtime assert that prints stack and error information before aborting
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

/**
 * Test if the task has debugging enabled
 *
 * @param  t  task pointer
 * @return  if the task is in debug mode
 */
#define debug(t) \
	__builtin_expect ((t)->flags & X_FDEBUG, 0)

struct xdefer {
	struct xdefer *next;
	void (*fn) (void *);
	void *data;
};

struct xtask {
	struct xtask *parent;          /** task the resumed this task */
	void *data;                    /** user data */
#if HAS_DLADDR
	char *name;                    /** entry function name */
#endif
	size_t tls;                    /** task locak storage size */
	union xvalue value;            /** the resume or yield value */
	struct xdefer *defer;          /** defered execution linked list */
	char **backtrace;              /** backtrace captured during task creationg */
	uint8_t nbacktrace;            /** number of backtrace frames */
	uint8_t state;                 /** SUSPENDED, CURRENT, ACTIVE, or DEAD */
	uintptr_t ctx[XCTX_REG_COUNT]; /** execution registers */
	uint32_t flags;                /** flags used to create the task */
	uint32_t map_size;             /** size of the mapped memory in bytes */
} __attribute__ ((aligned (16)));

union xtask_config {
	int64_t value;
	struct {
		uint32_t stack_size;
		uint32_t flags;
	} cfg;
};


static __thread struct xtask *current = NULL;
static __thread struct xtask top = { .state = CURRENT };
static __thread struct xtask *dead = NULL;
static __thread struct xdefer *pool = NULL;

static union xtask_config config = {
	.cfg = {
		.stack_size = X_STACK_DEFAULT,
		.flags = X_FDEFAULT
	}
};

/**
 * Populates a config option with valid values
 *
 * @param  stack_size  desired stack size
 * @param  flags       flags for the task
 * @return  valid configuration value
 */
static union xtask_config
config_make (uint32_t stack_size, uint32_t flags)
{
	if (stack_size > X_STACK_MAX) {
		stack_size = X_STACK_MAX;
	}
	else if (stack_size < X_STACK_MIN) {
		stack_size = X_STACK_MIN;
	}

	return (union xtask_config) {
		.cfg = {
			.stack_size = stack_size,
			.flags = flags & 0x7fffffff
		}
	};
}

/**
 * Reclaims a "freed" mapping
 *
 * If the map size doesn't meet needs it is unmapped and `NULL` is
 * returned. This will not iterate through the dead list as that could be
 * too costly. Perhaps trying a few at a time would be preferrable though?
 *
 * @param  map_size  minimum size requirement for the entire mapping
 * @return  pointer to mapped region or `NULL` if nothing to revive
 */
static uint8_t *
map_revive (uint32_t map_size)
{
	struct xtask *t = dead;
	if (t == NULL) {
		return NULL;
	}

	uint8_t *map = MAP_BEGIN (t);

	dead = t->parent;
	if (t->map_size < map_size) {
		munmap (map, t->map_size);
		map = NULL;
	}

	return map;
}

/**
 * Reclaims or creates a new mapping
 *
 * @param  map_size  minimum size requirement for the entire mapping
 * @return  pointer to mapped region or `NULL` on error
 */
static uint8_t *
map_alloc (uint32_t map_size)
{
	uint8_t *map = map_revive (map_size);
	if (map == NULL) {
		map = mmap (NULL, map_size, PROT_READ|PROT_WRITE, MAP_FLAGS, -1, 0);
		if (map == MAP_FAILED) {
			map = NULL;
		}
	}
	return map;
}

static void
defer_run (struct xdefer **d)
{
	assert (d != NULL);

	struct xdefer *def = *d;
	*d = NULL;

	while (def) {
		struct xdefer *next = def->next;
		def->fn (def->data);
		def->next = pool;
		pool = def;
		def = next;
	}
}

/**
 * Entry point for a new task
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
	struct xtask *parent = t->parent;
	union xvalue val = fn (t->data, t->value);

	current = parent;

	t->parent = NULL;
	t->value = val;
	t->state = DEAD;
	parent->state = CURRENT;
	defer_run (&t->defer);
	xctx_swap (t->ctx, parent->ctx);
}

void
xtask_configure (uint32_t stack_size, uint32_t flags)
{
	union xtask_config c = config_make (stack_size, flags);
	while (!__sync_bool_compare_and_swap (&config.value, config.value, c.value));
}

int
xtask_new (struct xtask **tp, union xvalue (*fn)(void *, union xvalue), void *data)
{
	assert (tp != NULL);
	assert (fn != NULL);

	union xtask_config c;
	c.value = config.value;
	return xtask_new_opt (tp, c.cfg.stack_size, c.cfg.flags, 0, fn, data);
}

int
xtask_new_opt (struct xtask **tp,
		uint32_t stack_size, uint32_t flags, size_t tls,
		union xvalue (*fn)(void *, union xvalue), void *data)
{
	assert (tp != NULL);
	assert (fn != NULL);

	tls = (tls + 15) & ~15;

	uint8_t *map = NULL, *stack = NULL;
	struct xtask *t = NULL;

	// exact size would be the stack, local storage, and task object
	uint32_t min_size = stack_size + tls + sizeof (*t);
	// round up to nearest page size
	uint32_t map_size = (((min_size - 1) / PAGESIZE) + 1) * PAGESIZE;

	if (flags & X_FPROTECT) {
		map_size += PAGESIZE;
	}
	
	map = map_alloc (map_size);
	if (map == NULL) {
		return XERRNO;
	}

#if STACK_GROWS_UP
	stack = map + sizeof (*t) + tls;
	t = (struct xtask *)map;
#else
	stack = map;
	t = (struct xtask *)(void *)(map + map_size - sizeof (*t));
#endif

	if ((flags & X_FPROTECT) && !(t->flags & X_FPROTECT)) {
#if STACK_GROWS_UP
		int rc = mprotect (map+map_size-PAGESIZE, PAGESIZE, PROT_NONE);
#else
		int rc = mprotect (map, PAGESIZE, PROT_NONE);
#endif
		if (rc < 0) {
			int rc = XERRNO;
			munmap (map, map_size);
			return rc;
		}
	}

	t->parent = NULL;
	t->data = data;
	t->tls = tls;
	t->value = XZERO;
	t->defer = NULL;
	t->backtrace = NULL;
	t->nbacktrace = 0;
	t->state = SUSPENDED;
	t->flags = flags;
	t->map_size = map_size;

	xctx_init (t->ctx, stack, STACK_SIZE (t),
			(uintptr_t)entry, (uintptr_t)t, (uintptr_t)fn);

#if HAS_DLADDR
	if (flags & X_FENTRY) {
		Dl_info info;
		if (dladdr ((void *)fn, &info) > 0) {
			t->name = strdup (info.dli_sname);
		}
	}
#endif

#if HAS_EXECINFO
	if (flags & X_FBACKTRACE) {
		void *calls[32];
		int frames = backtrace (calls, sizeof calls / sizeof calls[0]);
		if (frames > 0) {
			t->backtrace = backtrace_symbols (calls, frames);
			t->nbacktrace = frames;
		}
	}
#endif

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

	*tp = NULL;

	defer_run (&t->defer);
	free (t->name);
	free (t->backtrace);

	t->parent = dead;
	dead = t;
}

struct xtask *
xtask_self (void)
{
	return current;
}

void *
xtask_local (struct xtask *t)
{
	return TLS (t);
}

bool
xtask_alive (const struct xtask *t)
{
	assert (t != NULL);

	return t->state != DEAD;
}

size_t
xtask_stack_used (const struct xtask *t)
{
	if (t == NULL || t == &top) {
		return 0;
	}
	return xctx_stack_size (t->ctx, MAP_BEGIN (t), STACK_SIZE (t), t == current);
}

void
xtask_print (const struct xtask *t, FILE *out)
{
	if (out == NULL) {
		out = stdout;
	}

	if (t == NULL) {
		t = current;
		if (t == NULL) {
			fprintf (out, "#<crux:task:(null)>\n");
			return;
		}
	}

	if (t->state == DEAD) {
		fprintf (out,
				"#<crux:task:%p state=DEAD> {\n",
				(void *)t);
	}
	else {
		fprintf (out,
				"#<crux:task:%p state=%s, stack=%zd> {\n",
				(void *)t, state_names[t->state], xtask_stack_used (t));
	}
	xctx_print (t->ctx, out);
#if HAS_DLADDR
	if (t->name != NULL) {
		fprintf (out, "\tentry: %s\n", t->name);
	}
#endif
	fprintf (out, "\tdata: %p\n", t->data);
	if (t->nbacktrace > 0) {
		fprintf (out, "\tbacktrace:\n");
		for (int i = 0; i < t->nbacktrace; i++) {
			fprintf (stderr, "\t\t%s\n", t->backtrace[i]);
		}
	}
	fprintf (out, "}\n");
	fflush (out);
}

union xvalue
xyield (union xvalue val)
{
	struct xtask *t = current, *p = t->parent;

	ensure (t, p != NULL, "yield attempted outside of task");

	current = p;

	t->parent = NULL;
	t->value = val;
	t->state = SUSPENDED;
	p->state = CURRENT;
	xctx_swap (t->ctx, p->ctx);
	return t->value;
}

union xvalue
xresume (struct xtask *t, union xvalue val)
{
	ensure (t, t != NULL, "attempting to resume a null task");
	ensure (t, t->state != CURRENT, "attempting to resume the current task");
	ensure (t, t->state != ACTIVE, "attempting to resume an active task");
	ensure (t, t->state != DEAD, "attempting to resume a dead task");

	struct xtask *p = current;
	if (p == NULL) {
		p = &top;
	}

	current = t;

	t->parent = p;
	t->value = val;
	t->state = CURRENT;
	p->state = ACTIVE;
	xctx_swap (p->ctx, t->ctx);

	return t->value;
}

int
xdefer (void (*fn) (void *), void *data)
{
	assert (fn != NULL);

	struct xdefer *def = pool;
	if (def != NULL) {
		pool = def->next;
	}
	else {
		def = malloc (sizeof (*def));
		if (def == NULL) {
			return XERRNO;
		}
	}

	def->next = current->defer;
	def->fn = fn;
	def->data = data;
	current->defer = def;

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

