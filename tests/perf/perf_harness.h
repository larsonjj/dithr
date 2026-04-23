/**
 * \file            perf_harness.h
 * \brief           Minimal wall-clock benchmark harness for tests/perf/
 *
 * Each benchmark prints "name <iters> <ns_per_iter>" lines so results
 * can be diffed across builds.  Benchmarks use \c clock_gettime on POSIX
 * and \c QueryPerformanceCounter on Windows.  No external deps.
 *
 * Benchmarks are CTest-registered with the LABELS "perf" property so
 * the default test run skips them; opt in with:
 *
 *   ctest -L perf
 */

#ifndef DTR_PERF_HARNESS_H
#define DTR_PERF_HARNESS_H

#include <stdint.h>
#include <stdio.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static inline uint64_t dtr_perf_ns_(void)
{
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER        now;

    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000000000ULL) / (uint64_t)freq.QuadPart);
}
#else
#include <time.h>
static inline uint64_t dtr_perf_ns_(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
#endif

/**
 * \brief           Run \p body \p iters times and print ns/iter.
 *
 * Discards a configurable warm-up so the first cold-cache pass does not
 * skew the average.  \p body is a brace-enclosed block.
 */
#define DTR_BENCH(name, iters, warmup, body)                                                       \
    do {                                                                                           \
        const int32_t _iters  = (iters);                                                           \
        const int32_t _warmup = (warmup);                                                          \
        for (int32_t _w = 0; _w < _warmup; ++_w) {                                                 \
            body;                                                                                  \
        }                                                                                          \
        uint64_t _t0 = dtr_perf_ns_();                                                             \
        for (int32_t _i = 0; _i < _iters; ++_i) {                                                  \
            body;                                                                                  \
        }                                                                                          \
        uint64_t _t1     = dtr_perf_ns_();                                                         \
        double   _ns_per = (double)(_t1 - _t0) / (double)_iters;                                   \
        printf("perf %-32s iters=%-8d ns/iter=%.1f\n", (name), _iters, _ns_per);                   \
        fflush(stdout);                                                                            \
    } while (0)

#define DTR_PERF_BEGIN(suite_name) printf("=== perf: %s ===\n", (suite_name))
#define DTR_PERF_END()             return 0

#endif /* DTR_PERF_HARNESS_H */
