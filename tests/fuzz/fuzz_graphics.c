/**
 * \file            fuzz_graphics.c
 * \brief           Fuzz harness for graphics primitives
 *
 * Feed arbitrary bytes to exercise sprite rendering, circle/polygon
 * fill, and draw-list operations with random parameters.
 */

#include "graphics.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Read a little-endian int32 from the fuzz input, advancing the cursor. */
static int32_t prv_read_i32(const uint8_t **p, const uint8_t *end)
{
    int32_t v = 0;

    if (*p + 4 > end) {
        *p = end;
        return 0;
    }
    memcpy(&v, *p, 4);
    *p += 4;
    return v;
}

static uint8_t prv_read_u8(const uint8_t **p, const uint8_t *end)
{
    if (*p >= end) {
        return 0;
    }
    return *(*p)++;
}

/* libFuzzer entry point */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    dtr_graphics_t *gfx;
    const uint8_t  *p   = data;
    const uint8_t  *end = data + size;

    if (size > 8192) {
        return 0;
    }

    gfx = dtr_gfx_create(CONSOLE_FB_WIDTH, CONSOLE_FB_HEIGHT);
    if (gfx == NULL) {
        return 0;
    }

    while (p < end) {
        uint8_t op = prv_read_u8(&p, end);

        switch (op & 0x07) {
            case 0: { /* pset */
                int32_t x   = prv_read_i32(&p, end);
                int32_t y   = prv_read_i32(&p, end);
                uint8_t col = prv_read_u8(&p, end);
                dtr_gfx_pset(gfx, x, y, col);
                break;
            }
            case 1: { /* circfill */
                int32_t x   = prv_read_i32(&p, end);
                int32_t y   = prv_read_i32(&p, end);
                int32_t r   = prv_read_i32(&p, end);
                uint8_t col = prv_read_u8(&p, end);
                /* Clamp radius to avoid huge loops */
                if (r > 2048) {
                    r = 2048;
                }
                if (r < -2048) {
                    r = -2048;
                }
                dtr_gfx_circfill(gfx, x, y, r, col);
                break;
            }
            case 2: { /* rectfill */
                int32_t x0  = prv_read_i32(&p, end);
                int32_t y0  = prv_read_i32(&p, end);
                int32_t x1  = prv_read_i32(&p, end);
                int32_t y1  = prv_read_i32(&p, end);
                uint8_t col = prv_read_u8(&p, end);
                dtr_gfx_rectfill(gfx, x0, y0, x1, y1, col);
                break;
            }
            case 3: { /* line */
                int32_t x0  = prv_read_i32(&p, end);
                int32_t y0  = prv_read_i32(&p, end);
                int32_t x1  = prv_read_i32(&p, end);
                int32_t y1  = prv_read_i32(&p, end);
                uint8_t col = prv_read_u8(&p, end);
                dtr_gfx_line(gfx, x0, y0, x1, y1, col);
                break;
            }
            case 4: { /* trifill */
                int32_t x0  = prv_read_i32(&p, end);
                int32_t y0  = prv_read_i32(&p, end);
                int32_t x1  = prv_read_i32(&p, end);
                int32_t y1  = prv_read_i32(&p, end);
                int32_t x2  = prv_read_i32(&p, end);
                int32_t y2  = prv_read_i32(&p, end);
                uint8_t col = prv_read_u8(&p, end);
                dtr_gfx_trifill(gfx, x0, y0, x1, y1, x2, y2, col);
                break;
            }
            case 5: { /* polyfill — small polygon */
                uint8_t count = prv_read_u8(&p, end);
                int32_t pts[32]; /* max 16 vertices */
                int32_t n;

                count = count & 0x0F; /* max 15 */
                if (count > 16) {
                    count = 16;
                }
                n = (int32_t)count;
                for (int32_t i = 0; i < n * 2 && i < 32; ++i) {
                    pts[i] = prv_read_i32(&p, end);
                }
                dtr_gfx_polyfill(gfx, pts, n, prv_read_u8(&p, end));
                break;
            }
            case 6: { /* circ outline */
                int32_t x   = prv_read_i32(&p, end);
                int32_t y   = prv_read_i32(&p, end);
                int32_t r   = prv_read_i32(&p, end);
                uint8_t col = prv_read_u8(&p, end);
                if (r > 2048) {
                    r = 2048;
                }
                if (r < -2048) {
                    r = -2048;
                }
                dtr_gfx_circ(gfx, x, y, r, col);
                break;
            }
            case 7: { /* fill pattern then cls */
                uint8_t a = prv_read_u8(&p, end);
                uint8_t b = prv_read_u8(&p, end);
                dtr_gfx_fillp(gfx, (uint16_t)((uint16_t)a << 8 | (uint16_t)b));
                dtr_gfx_cls(gfx, prv_read_u8(&p, end));
                break;
            }
        }
    }

    dtr_gfx_destroy(gfx);
    return 0;
}
