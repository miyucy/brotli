#include <check.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/resource.h>

/* Include the actual buffer implementation */
#include "ext/brotli/buffer.h"

#define MAX_ALLOWED_BYTES (256 * 1024 * 1024)  /* 256 MB hard ceiling */

START_TEST(test_buffer_expansion_bounded)
{
    /* Invariant: buffer size must never exceed a safe maximum regardless of input */
    const size_t payloads[] = {
        SIZE_MAX / 2,          /* exact exploit: triggers unbounded doubling */
        SIZE_MAX,              /* boundary: maximum size_t value */
        1024,                  /* valid: normal small allocation */
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    /* Limit virtual memory to catch runaway allocation */
    struct rlimit rl;
    rl.rlim_cur = MAX_ALLOWED_BYTES;
    rl.rlim_max = MAX_ALLOWED_BYTES;
    setrlimit(RLIMIT_AS, &rl);

    for (int i = 0; i < num_payloads; i++) {
        struct brotli_buffer buf;
        brotli_buffer_init(&buf);

        int result = brotli_buffer_expand(&buf, payloads[i]);

        if (result == 0) {
            /* If expansion succeeded, size must be within safe bounds */
            ck_assert_msg(buf.size <= MAX_ALLOWED_BYTES,
                "Buffer size %zu exceeds maximum allowed %d bytes for payload index %d",
                buf.size, MAX_ALLOWED_BYTES, i);
        }
        /* If result != 0, expansion was rejected — that is acceptable safe behavior */

        brotli_buffer_free(&buf);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_buffer_expansion_bounded);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}