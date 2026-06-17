/*
 * Deterministic AddressSanitizer positive control. A heap-buffer-overflow that
 * ASan must catch and abort on. Used by leak/selftest.sh to prove the ASan
 * toolchain in the image actually detects corruption, before trusting a clean
 * `rake leak:asan` scan of the extension.
 *
 *   gcc -fsanitize=address -g -O0 -o asan_probe asan_probe.c && ./asan_probe
 *   => ERROR: AddressSanitizer: heap-buffer-overflow ... WRITE of size 64
 */
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char *p = malloc(16);
    memset(p, 'A', 64); /* 48 bytes past a 16-byte allocation */
    free(p);
    return 0;
}
