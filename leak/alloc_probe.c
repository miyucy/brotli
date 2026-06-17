/*
 * Tiny target to validate an allocation-failure injector mechanically: it just
 * mallocs a distinctive size in a loop and reports (exit 42) if any call was
 * forced to return NULL. Used by leak/selftest.sh to prove libfiu / the shim
 * can actually inject a failure, independent of whether a given run happens to
 * hit a specific allocation inside the (resilient, retrying) Ruby VM.
 *
 *   gcc -O0 -o alloc_probe alloc_probe.c
 *   FAIL_SIZE=17185 LD_PRELOAD=./failmalloc.so ./alloc_probe ; echo $?   # => 42
 */
#include <stdlib.h>

int main(void)
{
    int failed = 0;
    for (int i = 0; i < 2000; i++) {
        void *p = malloc(17185); /* distinctive size, unlikely during startup */
        if (!p) {
            failed = 1;
        } else {
            free(p);
        }
    }
    return failed ? 42 : 0;
}
