/**
 * \file            perf_primitives.c
 * \brief           Throughput micro-benchmarks for drawing primitives
 *                  and the per-frame palette flip.
 */

#include "graphics.h"
#include "perf_harness.h"

#include <stdlib.h>

#define FB_W 320
#define FB_H 180

int main(void)
{
    dtr_graphics_t *gfx = dtr_gfx_create(FB_W, FB_H);
    uint32_t       *out = (uint32_t *)malloc((size_t)FB_W * (size_t)FB_H * sizeof(uint32_t));
    DTR_PERF_BEGIN("primitives");

    DTR_BENCH("cls", 50000, 200, { dtr_gfx_cls(gfx, 0); });

    DTR_BENCH("rectfill 64x32", 50000, 200, { dtr_gfx_rectfill(gfx, 8, 8, 71, 39, 7); });

    DTR_BENCH("rect 64x32 outline", 50000, 200, { dtr_gfx_rect(gfx, 8, 8, 71, 39, 7); });

    DTR_BENCH("line diagonal 200px", 50000, 200, { dtr_gfx_line(gfx, 0, 0, 200, 150, 7); });

    DTR_BENCH("circ r=20", 50000, 200, { dtr_gfx_circ(gfx, 100, 90, 20, 7); });

    DTR_BENCH("circfill r=20", 20000, 100, { dtr_gfx_circfill(gfx, 100, 90, 20, 7); });

    DTR_BENCH("flip_to (320x180 palette->RGBA)", 5000, 50, { dtr_gfx_flip_to(gfx, out); });

    free(out);
    dtr_gfx_destroy(gfx);
    DTR_PERF_END();
}
