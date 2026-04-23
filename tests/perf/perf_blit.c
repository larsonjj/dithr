/**
 * \file            perf_blit.c
 * \brief           Sprite blit throughput micro-benchmark
 *
 * Measures \c dtr_gfx_spr in three scenarios that exercise the
 * 8-wide unrolled blit-span code path: forward, flipped, and a
 * mixed batch.  Run with `ctest -L perf` after a Release build.
 */

#include "graphics.h"
#include "perf_harness.h"

#include <stdlib.h>
#include <string.h>

#define FB_W       320
#define FB_H       180
#define SHEET_W    128
#define SHEET_H    128
#define TILE_W     8
#define TILE_H     8
#define BATCH_SIZE 1024

static dtr_graphics_t *prv_make_gfx(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(FB_W, FB_H);
    /* Build a synthetic spritesheet whose tiles are non-trivial
     * (mix of opaque + transparent) so the blit path is realistic. */
    dtr_gfx_create_sheet(gfx, SHEET_W, SHEET_H, TILE_W, TILE_H);
    for (int32_t i = 0; i < SHEET_W * SHEET_H; ++i) {
        gfx->sheet.pixels[i] = (uint8_t)(((uint32_t)i * 31u ^ (uint32_t)i >> 3) & 0x0Fu);
    }
    return gfx;
}

int main(void)
{
    dtr_graphics_t *gfx = prv_make_gfx();
    DTR_PERF_BEGIN("blit");

    DTR_BENCH("spr 8x8 forward (single)", 200000, 1000, {
        dtr_gfx_spr(gfx, 0, 16, 16, 1, 1, false, false);
    });

    DTR_BENCH("spr 8x8 flipped (single)", 200000, 1000, {
        dtr_gfx_spr(gfx, 0, 16, 16, 1, 1, true, false);
    });

    DTR_BENCH("spr 16x16 forward (single)", 100000, 500, {
        dtr_gfx_spr(gfx, 0, 16, 16, 2, 2, false, false);
    });

    DTR_BENCH("spr 8x8 batch x1024", 1000, 10, {
        for (int32_t i = 0; i < BATCH_SIZE; ++i) {
            int32_t x = (i * 7) & (FB_W - 1);
            int32_t y = (i * 11) & (FB_H - 1);
            dtr_gfx_spr(gfx, i & 0xFF, x, y, 1, 1, (i & 1) != 0, false);
        }
    });

    dtr_gfx_destroy(gfx);
    DTR_PERF_END();
}
