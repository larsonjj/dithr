/**
 * \file            test_harness.h
 * \brief           Minimal test macros — always active (never compiled out)
 */

#ifndef DTR_TEST_HARNESS_H
#define DTR_TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Always-on assertion: prints file/line and aborts on failure. */
#define DTR_ASSERT(expr)                                                          \
    do {                                                                          \
        if (!(expr)) {                                                            \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);     \
            fflush(stderr);                                                       \
            abort();                                                              \
        }                                                                         \
    } while (0)

/** Assert a pointer is not NULL, printing the expression on failure. */
#define DTR_ASSERT_NOT_NULL(ptr)                                                  \
    do {                                                                          \
        if ((ptr) == NULL) {                                                      \
            fprintf(stderr, "  FAIL %s:%d: %s == NULL\n",                         \
                    __FILE__, __LINE__, #ptr);                                    \
            fflush(stderr);                                                       \
            abort();                                                              \
        }                                                                         \
    } while (0)

/** Convenience: assert two ints are equal, printing both on failure. */
#define DTR_ASSERT_EQ_INT(a, b)                                                   \
    do {                                                                          \
        long long _a = (long long)(a);                                            \
        long long _b = (long long)(b);                                            \
        if (_a != _b) {                                                           \
            fprintf(stderr, "  FAIL %s:%d: %s == %lld, expected %s == %lld\n",    \
                    __FILE__, __LINE__, #a, _a, #b, _b);                          \
            fflush(stderr);                                                       \
            abort();                                                              \
        }                                                                         \
    } while (0)

/** Convenience: assert two strings are equal, printing both on failure. */
#define DTR_ASSERT_EQ_STR(a, b)                                                   \
    do {                                                                          \
        const char *_a = (a);                                                     \
        const char *_b = (b);                                                     \
        if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0) {                    \
            fprintf(stderr, "  FAIL %s:%d: %s == \"%s\", expected %s == \"%s\"\n",\
                    __FILE__, __LINE__, #a, _a ? _a : "(null)",                   \
                    #b, _b ? _b : "(null)");                                      \
            fflush(stderr);                                                       \
            abort();                                                              \
        }                                                                         \
    } while (0)

/** Float equality within epsilon. */
#define DTR_ASSERT_NEAR(a, b, eps)                                                \
    do {                                                                          \
        double _a = (double)(a);                                                  \
        double _b = (double)(b);                                                  \
        if ((_a - _b) > (eps) || (_b - _a) > (eps)) {                            \
            fprintf(stderr, "  FAIL %s:%d: %s == %f, expected %s == %f (eps %f)\n", \
                    __FILE__, __LINE__, #a, _a, #b, _b, (double)(eps));           \
            fflush(stderr);                                                       \
            abort();                                                              \
        }                                                                         \
    } while (0)

/** Print a passing test name. */
#define DTR_PASS() printf("  PASS %s\n", __func__)

/* ------------------------------------------------------------------ */
/*  Test runner helpers — optional counter / summary                    */
/* ------------------------------------------------------------------ */

/** Declare a test counter (call once at the top of main). */
#define DTR_TEST_BEGIN(suite_name)                                                 \
    int _dtr_test_count = 0;                                                      \
    printf("=== %s ===\n", (suite_name))

/** Run a single test function and bump the counter. */
#define DTR_RUN_TEST(fn)                                                           \
    do {                                                                           \
        ++_dtr_test_count;                                                         \
        fn();                                                                      \
    } while (0)

/** Print a summary line and return 0 (tests abort on failure, so reaching       */
/** this point means everything passed).                                         */
#define DTR_TEST_END()                                                             \
    do {                                                                           \
        printf("%d/%d tests passed.\n", _dtr_test_count, _dtr_test_count);        \
        return 0;                                                                  \
    } while (0)

#endif /* DTR_TEST_HARNESS_H */
