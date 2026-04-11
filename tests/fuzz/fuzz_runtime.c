/**
 * \file            fuzz_runtime.c
 * \brief           Fuzz harness for QuickJS JS evaluation path
 *
 * Feed arbitrary bytes as JavaScript source code and verify the
 * runtime handles malformed input without crashing.
 */

#include "console_defs.h"
#include "runtime.h"

#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* libFuzzer entry point */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    dtr_runtime_t *rt;
    char          *src;

    /* Skip very large inputs */
    if (size > 4096) {
        return 0;
    }

    rt = dtr_runtime_create(CONSOLE_JS_HEAP_MB, CONSOLE_JS_STACK_KB);
    if (rt == NULL) {
        return 0;
    }

    /* NUL-terminate the input for JS_Eval */
    src = SDL_malloc(size + 1);
    if (src != NULL) {
        memcpy(src, data, size);
        src[size] = '\0';

        dtr_runtime_eval(rt, src, size);
        SDL_free(src);
    }

    dtr_runtime_destroy(rt);
    return 0;
}
