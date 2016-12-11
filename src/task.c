#include "../include/crux/task.h"
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
 * @return  total size of the stack
 */
#define STACK_SIZE(t) \
	((t)->map_size - sizeof (struct xtask) - (t)->tls)

/**
 * @brief  Gets the task local storage
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
 * @brief  Get the mapped address from a task
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

struct xdefer {
	struct xdefer *next;
	void (*fn) (void *);
	void *data;
};

struct xtask {
	union xvalue value;            /** the resume or yield value */
	struct xtask *parent;          /** task that resumed this task */
	size_t tls;                    /** task locak storage size */
	void *data;                    /** user data */
	struct xdefer *defer;          /** defered execution linked list */
	uintptr_t ctx[XCTX_REG_COUNT]; /** execution registers */
#if HAS_DLADDR
	const char *name;              /** entry function name */
#endif
	uint32_t flags;                /** flags used to create the task */
	uint32_t map_size;             /** size of the mapped memory in bytes */
	const char *file;              /** allocation source file name */
	int line;                      /** allocation source line number */
	int16_t exitcode;              /** code if in EXIT state */
	uint8_t state;                 /** SUSPENDED, CURRENT, ACTIVE, or EXIT */
} __attribute__ ((aligned (16)));


static __thread struct xdefer *pool = NULL;
static __thread struct xtask *dead = NULL;
static __thread struct xtask *current = NULL;
static __thread struct xtask top = {
#if HAS_DLADDR
	.name = "main",
#endif
	.state = CURRENT
};

union xtask_config {
	int64_t value;
	struct xtask_opt opt;
};

static union xtask_config config = {
	.opt = {
		.stack_size = XTASK_STACK_DEFAULT,
		.flags = XTASK_FDEFAULT,
		.tls = 0,
		.file = NULL,
		.line = 0
	}
};

/**
 * @brief  Reclaims a "freed" mapping
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
 * @brief  Reclaims or creates a new mapping
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
	struct xdefer *def = t->defer;
	t->defer = NULL;

	// prevent task from being resumed explicity
	t->exitcode = ec;
	t->state = EXIT;

	while (def) {
		struct xdefer *next = def->next;
		def->fn (def->data);
		def->next = pool;
		pool = def;
		def = next;

		// mark exit status again in case defer resumed a separate task
		t->exitcode = ec;
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
	struct xtask *parent = t->parent;
	union xvalue val = fn (t->data, t->value);

	eol (t, val, 0);
	current = parent;
	parent->state = CURRENT;
	xctx_swap (t->ctx, parent->ctx);
}

void
xtask_configure (uint32_t stack_size, uint32_t flags)
{
	if (stack_size > XTASK_STACK_MAX) { stack_size = XTASK_STACK_MAX; }
	else if (stack_size < XTASK_STACK_MIN) { stack_size = XTASK_STACK_MIN; }

	union xtask_config c = { .opt = { .stack_size = stack_size, .flags = flags } };
	while (!__sync_bool_compare_and_swap (&config.value, config.value, c.value));
}

void
xtask_get_config (struct xtask_opt *opt)
{
	union xtask_config c;

	__sync_synchronize ();
	c.value = config.value;
	opt->stack_size = c.opt.stack_size;
	opt->flags = c.opt.flags;
}

int
xtask_new_loc (struct xtask **tp, const char *file, int line,
		union xvalue (*fn)(void *, union xvalue), void *data)
{
	assert (tp != NULL);
	assert (fn != NULL);

	struct xtask_opt opt = { .tls = 0, .file = file, .line = line };
	xtask_get_config (&opt);

	return xtask_new_opt (tp, &opt, fn, data);
}

int
xtask_new_opt (struct xtask **tp, const struct xtask_opt *opt,
		union xvalue (*fn)(void *, union xvalue), void *data)
{
	assert (tp != NULL);
	assert (fn != NULL);

	uint32_t stack_size = opt->stack_size;
	uint32_t flags = opt->flags;
	size_t tls = (opt->tls + 15) & ~15;
	uint8_t *map = NULL, *stack = NULL;
	struct xtask *t = NULL;

	// exact size would be the stack, local storage, and task object
	uint32_t min_size = stack_size + tls + sizeof (*t);
	// round up to nearest page size
	uint32_t map_size = (((min_size - 1) / PAGESIZE) + 1) * PAGESIZE;

	if (flags & XTASK_FPROTECT) {
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

	if ((flags & XTASK_FPROTECT) && !(t->flags & XTASK_FPROTECT)) {
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

	t->value = XZERO;
	t->parent = NULL;
	t->tls = tls;
	t->data = data;
	t->defer = NULL;
	t->flags = flags;
	t->map_size = map_size;
	t->file = opt->file;
	t->line = opt->line;
	t->exitcode = -1;
	t->state = SUSPENDED;

	xctx_init (t->ctx, stack, STACK_SIZE (t),
			(uintptr_t)entry, (uintptr_t)t, (uintptr_t)fn);

#if HAS_DLADDR
	if (flags & XTASK_FENTRY) {
		Dl_info info;
		if (dladdr ((void *)fn, &info) > 0) {
			t->name = info.dli_sname;
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

	eol (t, XZERO, t->exitcode);

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

size_t
xtask_stack_used (const struct xtask *t)
{
	if (t == NULL || t == &top) {
		return 0;
	}
	return xctx_stack_size (t->ctx, MAP_BEGIN (t), STACK_SIZE (t), t == current);
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
		if (t == NULL) { return -EINVAL; }
		yield = true;
	}
	else {
		yield = t == current;
	}

	if (t->state == EXIT) { return -EALREADY; }

	struct xtask *parent = t->parent;
	eol (t, XZERO, ec);
	if (yield) {
		current = parent;
		parent->state = CURRENT;
		xctx_swap (t->ctx, parent->ctx);
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
	if (t->name != NULL) {
		fprintf (out, " %s", t->name);
	}
#endif
	fprintf (out, " %s", state_names[t->state]);
	if (t->state == EXIT) {
		fprintf (out, " exitcode=%d>", t->exitcode);
	}
	else {
		fprintf (out, " stack=%zd>", xtask_stack_used (t));
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
	if (t == NULL) {
		print_head (t, out);
		fputc ('\n', out);
		return;
	}

	print_tree (t, out);

	fprintf (out, " {\n");
	xctx_print (t->ctx, out);
	fprintf (out, "\tdata: %p\n}\n", t->data);
	fflush (out);
}

union xvalue
xyield (union xvalue val)
{
	struct xtask *t = current, *p = t->parent;

	ensure (t, p != NULL, "yield attempted outside of task");
	ensure (t, t->state != EXIT, "attempting to yield from exiting task");

	current = p;

	t->parent = NULL;
	t->value = val;
	t->state = SUSPENDED;
	if (p->state != EXIT) {
		p->state = CURRENT;
	}
	xctx_swap (t->ctx, p->ctx);
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
	if (p == NULL) {
		p = &top;
	}

	current = t;

	t->parent = p;
	t->value = val;
	t->state = CURRENT;
	if (p->state != EXIT) {
		p->state = ACTIVE;
	}
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

