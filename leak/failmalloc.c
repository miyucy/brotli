/*
 * Minimal LD_PRELOAD allocation-failure injector for the leak harness.
 *
 * Make a specific malloc/realloc return NULL so the extension's OOM paths
 * (create_buffer/append_buffer return NULL/-1, dict_data's ruby_xmalloc) fire,
 * then observe whether resources leak. The real libc functions are resolved in
 * a load-time constructor (before Ruby starts any thread), so injection is
 * deterministic and free of the lazy-init data race. calloc is intentionally
 * NOT intercepted: glibc's dlsym() uses it for its dlerror TLS, so leaving it
 * alone keeps dlsym from recursing into our own malloc during init.
 * deps: -ldl only.
 *
 *   gcc -shared -fPIC -o failmalloc.so failmalloc.c -ldl
 *
 * Env (any combination; first match fails the call):
 *   FAIL_MALLOC_N=<n>    fail the n-th malloc  (1-based count)
 *   FAIL_REALLOC_N=<n>   fail the n-th realloc (1-based) -- targets expand_buffer/F1
 *   FAIL_SIZE=<bytes>    fail any malloc/realloc requesting exactly this size
 *   FAIL_MIN_SIZE=<b>    fail any malloc/realloc requesting >= b bytes (use a
 *                        large value to hit only brotli's multi-MB no-GVL
 *                        buffers, leaving Ruby/GC's sub-MB allocations alone)
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <errno.h>

static void *(*real_malloc)(size_t);
static void *(*real_realloc)(void *, size_t);

static atomic_long malloc_count;
static atomic_long realloc_count;
static long fail_malloc_n  = -1;
static long fail_realloc_n = -1;
static long fail_size      = -1;
static long fail_min_size  = -1;

__attribute__((constructor))
static void init(void)
{
    real_malloc  = dlsym(RTLD_NEXT, "malloc");
    real_realloc = dlsym(RTLD_NEXT, "realloc");
    const char *m = getenv("FAIL_MALLOC_N");
    const char *r = getenv("FAIL_REALLOC_N");
    const char *s = getenv("FAIL_SIZE");
    const char *z = getenv("FAIL_MIN_SIZE");
    if (m) fail_malloc_n  = atol(m);
    if (r) fail_realloc_n = atol(r);
    if (s) fail_size      = atol(s);
    if (z) fail_min_size  = atol(z);
}

static int fail_for_size(size_t n)
{
    return (fail_size >= 0 && (long)n == fail_size) ||
           (fail_min_size >= 0 && (long)n >= fail_min_size);
}

void *malloc(size_t n)
{
    if (!real_malloc) init();
    long c = atomic_fetch_add(&malloc_count, 1) + 1;
    if ((fail_malloc_n > 0 && c == fail_malloc_n) || fail_for_size(n)) {
        errno = ENOMEM;
        return NULL;
    }
    return real_malloc(n);
}

void *realloc(void *p, size_t n)
{
    if (!real_realloc) init();
    long c = atomic_fetch_add(&realloc_count, 1) + 1;
    if ((fail_realloc_n > 0 && c == fail_realloc_n) || fail_for_size(n)) {
        errno = ENOMEM;
        return NULL;
    }
    return real_realloc(p, n);
}
