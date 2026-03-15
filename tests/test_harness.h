/**
 * \file            test_harness.h
 * \brief           Minimal test macros — always active (never compiled out)
 */

#ifndef MVN_TEST_HARNESS_H
#define MVN_TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>

/** Always-on assertion: prints file/line and aborts on failure. */
#define MVN_ASSERT(expr)                                                          \
    do {                                                                          \
        if (!(expr)) {                                                            \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);     \
            fflush(stderr);                                                       \
            abort();                                                              \
        }                                                                         \
    } while (0)

/** Convenience: assert two ints are equal, printing both on failure. */
#define MVN_ASSERT_EQ_INT(a, b)                                                   \
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

/** Float equality within epsilon. */
#define MVN_ASSERT_NEAR(a, b, eps)                                                \
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
#define MVN_PASS() printf("  PASS %s\n", __func__)

#endif /* MVN_TEST_HARNESS_H */
