/**
 * \file            fuzz_postfx.c
 * \brief           Fuzz harness for the post-processing pipeline
 *
 * Push random effects with arbitrary parameters onto the postfx stack,
 * then apply them to a framebuffer.  Exercises per-pixel SIMD code
 * paths with edge-case parameter values.
 */

#include "console_defs.h"
#include "postfx.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* libFuzzer entry point */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    dtr_postfx_t *pfx;
    uint32_t      pixels[64 * 64];
    const size_t  pixel_count = sizeof(pixels) / sizeof(pixels[0]);

    if (size > 1024) {
        return 0;
    }

    pfx = dtr_postfx_create(64, 64);
    if (pfx == NULL) {
        return 0;
    }

    /* Seed the pixel buffer with fuzz data */
    {
        size_t copy_len = size < sizeof(pixels) ? size : sizeof(pixels);

        memset(pixels, 0xFF, sizeof(pixels));
        memcpy(pixels, data, copy_len);
    }

    /* Push effects from fuzz data */
    {
        const uint8_t *p   = data;
        const uint8_t *end = data + size;

        while (p + 2 <= end) {
            uint8_t raw_id   = *p++;
            uint8_t raw_str  = *p++;
            int32_t id       = (int32_t)(raw_id % DTR_POSTFX_BUILTIN_COUNT);
            float   strength = (float)raw_str / 255.0f;

            dtr_postfx_push(pfx, (dtr_postfx_id_t)id, strength);
        }
    }

    dtr_postfx_apply(pfx, pixels, 64, 64);

    /* Verify we can pop and re-apply without crashing */
    dtr_postfx_pop(pfx);
    if (pixel_count > 0) {
        memset(pixels, 0xAA, sizeof(pixels));
    }
    dtr_postfx_apply(pfx, pixels, 64, 64);

    dtr_postfx_destroy(pfx);
    return 0;
}
