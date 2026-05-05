/**
 * \file            graphics.c
 * \brief           Framebuffer, palette, primitives, sprites, text
 */

#include "graphics.h"

#include "font.h"
#include "simd.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Default 256-colour palette (first 16 match the classic palette)    */
/* ------------------------------------------------------------------ */

/* RGBA packed as 0xRRGGBBAA */
static const uint32_t DEFAULT_PALETTE[CONSOLE_PALETTE_SIZE] = {
    0x000000FF,
    0x1D2B53FF,
    0x7E2553FF,
    0x008751FF, /*  0- 3 */
    0xAB5236FF,
    0x5F574FFF,
    0xC2C3C7FF,
    0xFFF1E8FF, /*  4- 7 */
    0xFF004DFF,
    0xFFA300FF,
    0xFFEC27FF,
    0x00E436FF, /*  8-11 */
    0x29ADFFFF,
    0x83769CFF,
    0xFF77A8FF,
    0xFFCCAAFF, /* 12-15 */
    /* 16-255: greyscale + colour ramps */
    0x111111FF,
    0x222222FF,
    0x333333FF,
    0x444444FF, /* 16-19 */
    0x555555FF,
    0x666666FF,
    0x777777FF,
    0x888888FF, /* 20-23 */
    0x999999FF,
    0xAAAAAAFF,
    0xBBBBBBFF,
    0xCCCCCCFF, /* 24-27 */
    0xDDDDDDFF,
    0xEEEEEEFF,
    0xFEFEFEFF,
    0xF0F0F0FF, /* 28-31 */
    /* Fill remaining 224 entries with incremental hues */
};

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

dtr_graphics_t *dtr_gfx_create(int32_t width, int32_t height)
{
    dtr_graphics_t *gfx;
    size_t          pixel_count;

    gfx = DTR_CALLOC(1, sizeof(dtr_graphics_t));
    if (gfx == NULL) {
        return NULL;
    }

    gfx->width  = width;
    gfx->height = height;

    pixel_count = (size_t)width * (size_t)height;

    gfx->framebuffer = DTR_CALLOC(pixel_count, sizeof(uint8_t));
    gfx->pixels      = DTR_CALLOC(pixel_count, sizeof(uint32_t));
    if (gfx->framebuffer == NULL || gfx->pixels == NULL) {
        DTR_FREE(gfx->framebuffer);
        DTR_FREE(gfx->pixels);
        DTR_FREE(gfx);
        return NULL;
    }

    dtr_gfx_init_default_palette(gfx);
    dtr_gfx_reset(gfx);

    return gfx;
}

void dtr_gfx_destroy(dtr_graphics_t *gfx)
{
    if (gfx == NULL) {
        return;
    }
    if (gfx->sheet.pixels != NULL) {
        DTR_FREE(gfx->sheet.pixels);
    }
    if (gfx->transition.dissolve_order != NULL) {
        DTR_FREE(gfx->transition.dissolve_order);
    }
    if (gfx->pal_lut != NULL) {
        DTR_FREE(gfx->pal_lut);
    }
    DTR_FREE(gfx->framebuffer);
    DTR_FREE(gfx->pixels);
    DTR_FREE(gfx);
}

void dtr_gfx_reset(dtr_graphics_t *gfx)
{
    if (gfx->transition.dissolve_order != NULL) {
        DTR_FREE(gfx->transition.dissolve_order);
        gfx->transition.dissolve_order = NULL;
    }
    gfx->transition.type = DTR_TRANS_NONE;

    gfx->color        = 7;
    gfx->cursor_x     = 0;
    gfx->cursor_y     = 0;
    gfx->fill_pattern = 0;
    gfx->camera_x     = 0;
    gfx->camera_y     = 0;
    gfx->clip_x       = 0;
    gfx->clip_y       = 0;
    gfx->clip_w       = gfx->width;
    gfx->clip_h       = gfx->height;

    /* Identity palettes */
    for (int32_t idx = 0; idx < CONSOLE_PALETTE_SIZE; ++idx) {
        gfx->draw_pal[idx]    = (uint8_t)idx;
        gfx->screen_pal[idx]  = (uint8_t)idx;
        gfx->transparent[idx] = false;
    }
    /* Colour 0 is transparent by default for sprites */
    gfx->transparent[0] = true;
}

void dtr_gfx_init_default_palette(dtr_graphics_t *gfx)
{
    int32_t idx;

    /* Copy known entries */
    for (idx = 0; idx < 32 && idx < CONSOLE_PALETTE_SIZE; ++idx) {
        gfx->colors[idx] = DEFAULT_PALETTE[idx];
    }

    /* Generate remaining as HSV ramps */
    for (; idx < CONSOLE_PALETTE_SIZE; ++idx) {
        float   hue;
        float   sat;
        float   val;
        int32_t rval;
        int32_t gval;
        int32_t bval;
        float   frac;
        float   sector;

        hue = (float)(idx - 32) / (float)(CONSOLE_PALETTE_SIZE - 32) * 360.0f;
        sat = 0.7f;
        val = 0.8f;

        /* HSV → RGB */
        sector = hue / 60.0f;
        frac   = sector - floorf(sector);

        {
            float   pval;
            float   qval;
            float   tval;
            int32_t sect;

            pval = val * (1.0f - sat);
            qval = val * (1.0f - sat * frac);
            tval = val * (1.0f - sat * (1.0f - frac));
            sect = (int32_t)sector % 6;

            switch (sect) {
                case 0:
                    rval = (int32_t)(val * 255.0f);
                    gval = (int32_t)(tval * 255.0f);
                    bval = (int32_t)(pval * 255.0f);
                    break;
                case 1:
                    rval = (int32_t)(qval * 255.0f);
                    gval = (int32_t)(val * 255.0f);
                    bval = (int32_t)(pval * 255.0f);
                    break;
                case 2:
                    rval = (int32_t)(pval * 255.0f);
                    gval = (int32_t)(val * 255.0f);
                    bval = (int32_t)(tval * 255.0f);
                    break;
                case 3:
                    rval = (int32_t)(pval * 255.0f);
                    gval = (int32_t)(qval * 255.0f);
                    bval = (int32_t)(val * 255.0f);
                    break;
                case 4:
                    rval = (int32_t)(tval * 255.0f);
                    gval = (int32_t)(pval * 255.0f);
                    bval = (int32_t)(val * 255.0f);
                    break;
                default:
                    rval = (int32_t)(val * 255.0f);
                    gval = (int32_t)(pval * 255.0f);
                    bval = (int32_t)(qval * 255.0f);
                    break;
            }
        }

        gfx->colors[idx] =
            ((uint32_t)rval << 24) | ((uint32_t)gval << 16) | ((uint32_t)bval << 8) | 0xFF;
    }
}

/* ------------------------------------------------------------------ */
/*  Internal: safe pixel write respecting clip, camera, fill pattern   */
/* ------------------------------------------------------------------ */

static void prv_put_pixel(dtr_graphics_t *gfx, int32_t raw_x, int32_t raw_y, uint8_t col)
{
    int32_t scr_x;
    int32_t scr_y;

    scr_x = raw_x - gfx->camera_x;
    scr_y = raw_y - gfx->camera_y;

    /* Clip */
    if (scr_x < gfx->clip_x || scr_x >= gfx->clip_x + gfx->clip_w) {
        return;
    }
    if (scr_y < gfx->clip_y || scr_y >= gfx->clip_y + gfx->clip_h) {
        return;
    }

    /* Fill pattern check */
    if (gfx->fill_pattern != 0) {
        int32_t pat_x;
        int32_t pat_y;
        int32_t bit;

        pat_x = scr_x & 3;
        pat_y = scr_y & 3;
        bit   = pat_y * 4 + pat_x;
        if (!(gfx->fill_pattern & (1 << bit))) {
            return;
        }
    }

    /* Draw palette remap */
    col = gfx->draw_pal[col];

    gfx->framebuffer[scr_y * gfx->width + scr_x] = col;
}

/**
 * \brief           Compute the four clip-rect edges from a graphics context.
 */
static void prv_get_clip_bounds(const dtr_graphics_t *gfx,
                                int32_t              *out_l,
                                int32_t              *out_t,
                                int32_t              *out_r,
                                int32_t              *out_b)
{
    *out_l = gfx->clip_x;
    *out_t = gfx->clip_y;
    *out_r = gfx->clip_x + gfx->clip_w - 1;
    *out_b = gfx->clip_y + gfx->clip_h - 1;
}

/**
 * \brief           Extract the 4-bit row mask for a given scanline from
 *                  the 16-bit fill pattern.
 * \return          0 when \p fill_pattern is zero (solid), else the 4-bit
 *                  mask for row \p scr_y.
 */
static uint16_t prv_row_pattern(uint16_t fill_pattern, int32_t scr_y)
{
    if (fill_pattern == 0) {
        return 0;
    }
    return (uint16_t)((fill_pattern >> ((scr_y & 3) * 4)) & 0xF);
}

/**
 * \brief           Blit a source span with transparency check and palette
 *                  remap using an 8-wide unrolled loop for better pipelining.
 *
 * Only compiled when DTR_HAS_SIMD is true (fast-path guard).
 */
#if DTR_HAS_SIMD
/**
 * \brief           Shared 8-wide unrolled blit-span implementation.
 *
 * \param[in]       src_step  +1 for forward (normal) blit, -1 for flipped
 *                            blit.  Must be a compile-time constant at the
 *                            call site so the optimizer can specialize the
 *                            two variants identically to the previous
 *                            hand-written pair.
 *
 * Forward callers pass \c src_base pointing at the first source pixel.
 * Flipped callers pass \c src_base pointing at the LAST source pixel
 * (i.e. \c src_ptr + span - 1) so that \c src_base[done * src_step]
 * walks the source in reverse while the destination is written
 * left-to-right.
 */
static inline void prv_blit_span_impl(uint8_t       *dst_row,
                                      const uint8_t *src_base,
                                      int32_t        scr_x,
                                      int32_t        span,
                                      const bool    *transp,
                                      const uint8_t *dpal,
                                      int8_t         src_step)
{
    int32_t  done;
    uint8_t *dst;

    done = 0;
    dst  = dst_row + scr_x;

    /* 8-wide: load 8 indices, remap, write non-transparent */
    for (; done + 7 < span; done += 8) {
        uint8_t i_0;
        uint8_t i_1;
        uint8_t i_2;
        uint8_t i_3;
        uint8_t i_4;
        uint8_t i_5;
        uint8_t i_6;
        uint8_t i_7;

        i_0 = src_base[(ptrdiff_t)(done + 0) * src_step];
        i_1 = src_base[(ptrdiff_t)(done + 1) * src_step];
        i_2 = src_base[(ptrdiff_t)(done + 2) * src_step];
        i_3 = src_base[(ptrdiff_t)(done + 3) * src_step];
        i_4 = src_base[(ptrdiff_t)(done + 4) * src_step];
        i_5 = src_base[(ptrdiff_t)(done + 5) * src_step];
        i_6 = src_base[(ptrdiff_t)(done + 6) * src_step];
        i_7 = src_base[(ptrdiff_t)(done + 7) * src_step];

        if (!transp[i_0]) {
            dst[done] = dpal[i_0];
        }
        if (!transp[i_1]) {
            dst[done + 1] = dpal[i_1];
        }
        if (!transp[i_2]) {
            dst[done + 2] = dpal[i_2];
        }
        if (!transp[i_3]) {
            dst[done + 3] = dpal[i_3];
        }
        if (!transp[i_4]) {
            dst[done + 4] = dpal[i_4];
        }
        if (!transp[i_5]) {
            dst[done + 5] = dpal[i_5];
        }
        if (!transp[i_6]) {
            dst[done + 6] = dpal[i_6];
        }
        if (!transp[i_7]) {
            dst[done + 7] = dpal[i_7];
        }
    }

    /* Scalar tail */
    for (; done < span; ++done) {
        uint8_t col;

        col = src_base[(ptrdiff_t)done * src_step];
        if (!transp[col]) {
            dst[done] = dpal[col];
        }
    }
}

/**
 * \brief           Forward blit: read source left-to-right, write left-to-right.
 */
static void prv_blit_span(uint8_t       *dst_row,
                          const uint8_t *src_ptr,
                          int32_t        scr_x,
                          int32_t        span,
                          const bool    *transp,
                          const uint8_t *dpal)
{
    prv_blit_span_impl(dst_row, src_ptr, scr_x, span, transp, dpal, (int8_t)1);
}

/**
 * \brief           Flipped blit: read source right-to-left, write left-to-right.
 *
 * \param[in]       src_end  Pointer to the LAST source pixel (i.e. the
 *                           rightmost pixel that should appear leftmost
 *                           in the destination).
 */
static void prv_blit_span_flip(uint8_t       *dst_row,
                               const uint8_t *src_end,
                               int32_t        scr_x,
                               int32_t        span,
                               const bool    *transp,
                               const uint8_t *dpal)
{
    prv_blit_span_impl(dst_row, src_end, scr_x, span, transp, dpal, (int8_t)-1);
}
#endif /* DTR_HAS_SIMD */

/* ------------------------------------------------------------------ */
/*  Internal: fast horizontal span (pre-clipped scanline)              */
/* ------------------------------------------------------------------ */

/**
 * \brief           Draw a horizontal span from (raw_x0..raw_x1) at raw_y.
 *
 * Camera transform, Y-clipping, and X-clipping are done once per span
 * rather than per pixel.  When the fill pattern is zero (solid fill)
 * the span is written with memset for maximum throughput.
 */
static void
prv_hline(dtr_graphics_t *gfx, int32_t raw_x0, int32_t raw_x1, int32_t raw_y, uint8_t col)
{
    int32_t scr_y;
    int32_t left;
    int32_t right;
    int32_t clip_l;
    int32_t clip_r;

    /* Camera transform Y and clip */
    scr_y = raw_y - gfx->camera_y;
    if (scr_y < gfx->clip_y || scr_y >= gfx->clip_y + gfx->clip_h) {
        return;
    }

    /* Camera transform X */
    left  = raw_x0 - gfx->camera_x;
    right = raw_x1 - gfx->camera_x;

    /* Clip X */
    clip_l = gfx->clip_x;
    clip_r = gfx->clip_x + gfx->clip_w - 1;
    if (left < clip_l) {
        left = clip_l;
    }
    if (right > clip_r) {
        right = clip_r;
    }
    if (left > right) {
        return;
    }

    /* Safety check: left must be non-negative after clipping */
    if (left < 0) {
        return;
    }

    /* Remap colour once */
    col = gfx->draw_pal[col];

    if (gfx->fill_pattern == 0) {
        /* Solid fill — memset the span */
        memset(&gfx->framebuffer[scr_y * gfx->width + left], col, (size_t)right - (size_t)left + 1);
    } else {
        /* Patterned fill */
        int32_t pat_y_bits;

        pat_y_bits = (scr_y & 3) * 4;
        for (int32_t sx = left; sx <= right; ++sx) {
            int32_t bit;

            bit = pat_y_bits + (sx & 3);
            if (gfx->fill_pattern & (1 << bit)) {
                gfx->framebuffer[scr_y * gfx->width + sx] = col;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Internal: fast vertical span (pre-clipped column)                   */
/* ------------------------------------------------------------------ */

static void
prv_vline(dtr_graphics_t *gfx, int32_t raw_x, int32_t raw_y0, int32_t raw_y1, uint8_t col)
{
    int32_t scr_x;
    int32_t top;
    int32_t bot;
    int32_t clip_t;
    int32_t clip_b;

    /* Camera transform X and clip */
    scr_x = raw_x - gfx->camera_x;
    if (scr_x < gfx->clip_x || scr_x >= gfx->clip_x + gfx->clip_w) {
        return;
    }

    /* Camera transform Y */
    top = raw_y0 - gfx->camera_y;
    bot = raw_y1 - gfx->camera_y;

    /* Clip Y */
    clip_t = gfx->clip_y;
    clip_b = gfx->clip_y + gfx->clip_h - 1;
    if (top < clip_t) {
        top = clip_t;
    }
    if (bot > clip_b) {
        bot = clip_b;
    }
    if (top > bot) {
        return;
    }

    /* Remap colour once */
    col = gfx->draw_pal[col];

    if (gfx->fill_pattern == 0) {
        /* Solid fill */
        int32_t stride;

        stride = gfx->width;
        for (int32_t sy = top; sy <= bot; ++sy) {
            gfx->framebuffer[sy * stride + scr_x] = col;
        }
    } else {
        /* Patterned fill */
        int32_t pat_x_bit;

        pat_x_bit = scr_x & 3;
        for (int32_t sy = top; sy <= bot; ++sy) {
            int32_t bit;

            bit = (sy & 3) * 4 + pat_x_bit;
            if (gfx->fill_pattern & (1 << bit)) {
                gfx->framebuffer[sy * gfx->width + scr_x] = col;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Core drawing                                                       */
/* ------------------------------------------------------------------ */

void dtr_gfx_cls(dtr_graphics_t *gfx, uint8_t col)
{
    memset(gfx->framebuffer, col, (size_t)gfx->width * (size_t)gfx->height);
}

void dtr_gfx_pset(dtr_graphics_t *gfx, int32_t x, int32_t y, uint8_t col)
{
    prv_put_pixel(gfx, x, y, col);
}

uint8_t dtr_gfx_pget(dtr_graphics_t *gfx, int32_t x, int32_t y)
{
    int32_t scr_x;
    int32_t scr_y;

    scr_x = x - gfx->camera_x;
    scr_y = y - gfx->camera_y;

    if (scr_x < 0 || scr_x >= gfx->width || scr_y < 0 || scr_y >= gfx->height) {
        return 0;
    }
    return gfx->framebuffer[scr_y * gfx->width + scr_x];
}

/* ------------------------------------------------------------------ */
/*  Primitives                                                         */
/* ------------------------------------------------------------------ */

void dtr_gfx_line(dtr_graphics_t *gfx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t col)
{
    int32_t dx_val;
    int32_t dy_val;
    int32_t sx_val;
    int32_t sy_val;
    int32_t err;

    /* Fast path for axis-aligned lines */
    if (y0 == y1) {
        int32_t lo = (x0 < x1) ? x0 : x1;
        int32_t hi = (x0 > x1) ? x0 : x1;
        prv_hline(gfx, lo, hi, y0, col);
        return;
    }
    if (x0 == x1) {
        int32_t lo = (y0 < y1) ? y0 : y1;
        int32_t hi = (y0 > y1) ? y0 : y1;
        prv_vline(gfx, x0, lo, hi, col);
        return;
    }

    dx_val = abs(x1 - x0);
    dy_val = -abs(y1 - y0);
    sx_val = (x0 < x1) ? 1 : -1;
    sy_val = (y0 < y1) ? 1 : -1;
    err    = dx_val + dy_val;

    for (;;) {
        int32_t err2;

        prv_put_pixel(gfx, x0, y0, col);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        err2 = 2 * err;
        if (err2 >= dy_val) {
            err += dy_val;
            x0 += sx_val;
        }
        if (err2 <= dx_val) {
            err += dx_val;
            y0 += sy_val;
        }
    }
}

void dtr_gfx_rect(dtr_graphics_t *gfx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t col)
{
    int32_t min_x;
    int32_t max_x;
    int32_t min_y;
    int32_t max_y;

    min_x = (x0 < x1) ? x0 : x1;
    max_x = (x0 > x1) ? x0 : x1;
    min_y = (y0 < y1) ? y0 : y1;
    max_y = (y0 > y1) ? y0 : y1;

    /* Top and bottom edges */
    prv_hline(gfx, min_x, max_x, min_y, col);
    if (max_y != min_y) {
        prv_hline(gfx, min_x, max_x, max_y, col);
    }

    /* Left and right edges (skip corners already drawn) */
    if (max_y - min_y > 1) {
        prv_vline(gfx, min_x, min_y + 1, max_y - 1, col);
        if (max_x != min_x) {
            prv_vline(gfx, max_x, min_y + 1, max_y - 1, col);
        }
    }
}

void dtr_gfx_rectfill(dtr_graphics_t *gfx,
                      int32_t         x0,
                      int32_t         y0,
                      int32_t         x1,
                      int32_t         y1,
                      uint8_t         col)
{
    int32_t min_x;
    int32_t max_x;
    int32_t min_y;
    int32_t max_y;

    min_x = (x0 < x1) ? x0 : x1;
    max_x = (x0 > x1) ? x0 : x1;
    min_y = (y0 < y1) ? y0 : y1;
    max_y = (y0 > y1) ? y0 : y1;

    for (int32_t row = min_y; row <= max_y; ++row) {
        prv_hline(gfx, min_x, max_x, row, col);
    }
}

void dtr_gfx_tilemap(dtr_graphics_t *gfx,
                     const uint8_t  *tiles,
                     int32_t         map_w,
                     int32_t         map_h,
                     int32_t         tile_w,
                     int32_t         tile_h,
                     const uint8_t  *colors,
                     int32_t         num_colors)
{
    int32_t view_x0;
    int32_t view_y0;
    int32_t view_x1;
    int32_t view_y1;
    int32_t start_tx;
    int32_t start_ty;
    int32_t end_tx;
    int32_t end_ty;

    /* Visible region in world coords */
    view_x0 = gfx->camera_x + gfx->clip_x;
    view_y0 = gfx->camera_y + gfx->clip_y;
    view_x1 = view_x0 + gfx->clip_w - 1;
    view_y1 = view_y0 + gfx->clip_h - 1;

    /* Tile range to draw */
    start_tx = view_x0 / tile_w;
    start_ty = view_y0 / tile_h;
    end_tx   = view_x1 / tile_w + 1;
    end_ty   = view_y1 / tile_h + 1;

    if (start_tx < 0) {
        start_tx = 0;
    }
    if (start_ty < 0) {
        start_ty = 0;
    }
    if (end_tx > map_w) {
        end_tx = map_w;
    }
    if (end_ty > map_h) {
        end_ty = map_h;
    }

    for (int32_t ty = start_ty; ty < end_ty; ++ty) {
        int32_t py;

        py = ty * tile_h;
        for (int32_t tx = start_tx; tx < end_tx; ++tx) {
            uint8_t tile;
            uint8_t col;
            int32_t px;

            tile = tiles[ty * map_w + tx];
            col  = (tile < num_colors) ? colors[tile] : 0;
            px   = tx * tile_w;

            for (int32_t row = py; row < py + tile_h; ++row) {
                prv_hline(gfx, px, px + tile_w - 1, row, col);
            }
        }
    }
}

void dtr_gfx_circ(dtr_graphics_t *gfx, int32_t x, int32_t y, int32_t r, uint8_t col)
{
    int32_t dx_val;
    int32_t dy_val;
    int32_t err;

    dx_val = -r;
    dy_val = 0;
    err    = 2 - 2 * r;

    do {
        prv_put_pixel(gfx, x - dx_val, y + dy_val, col);
        prv_put_pixel(gfx, x - dy_val, y - dx_val, col);
        prv_put_pixel(gfx, x + dx_val, y - dy_val, col);
        prv_put_pixel(gfx, x + dy_val, y + dx_val, col);

        {
            int32_t rad;

            rad = err;
            if (rad <= dy_val) {
                ++dy_val;
                err += dy_val * 2 + 1;
            }
            if (rad > dx_val || err > dy_val) {
                ++dx_val;
                err += dx_val * 2 + 1;
            }
        }
    } while (dx_val < 0);
}

void dtr_gfx_circfill(dtr_graphics_t *gfx, int32_t x, int32_t y, int32_t r, uint8_t col)
{
    int32_t dx_val;
    int32_t dy_val;
    int32_t err;

    if (r <= 0) {
        prv_put_pixel(gfx, x, y, col);
        return;
    }

    /* Midpoint circle algorithm — draw horizontal spans */
    dx_val = r;
    dy_val = 0;
    err    = 1 - r;

    while (dx_val >= dy_val) {
        /* Four symmetric horizontal spans */
        prv_hline(gfx, x - dx_val, x + dx_val, y + dy_val, col);
        prv_hline(gfx, x - dx_val, x + dx_val, y - dy_val, col);
        prv_hline(gfx, x - dy_val, x + dy_val, y + dx_val, col);
        prv_hline(gfx, x - dy_val, x + dy_val, y - dx_val, col);

        ++dy_val;
        if (err < 0) {
            err += 2 * dy_val + 1;
        } else {
            --dx_val;
            err += 2 * (dy_val - dx_val) + 1;
        }
    }
}

void dtr_gfx_tri(dtr_graphics_t *gfx,
                 int32_t         x0,
                 int32_t         y0,
                 int32_t         x1,
                 int32_t         y1,
                 int32_t         x2,
                 int32_t         y2,
                 uint8_t         col)
{
    dtr_gfx_line(gfx, x0, y0, x1, y1, col);
    dtr_gfx_line(gfx, x1, y1, x2, y2, col);
    dtr_gfx_line(gfx, x2, y2, x0, y0, col);
}

void dtr_gfx_trifill(dtr_graphics_t *gfx,
                     int32_t         x0,
                     int32_t         y0,
                     int32_t         x1,
                     int32_t         y1,
                     int32_t         x2,
                     int32_t         y2,
                     uint8_t         col)
{
    int32_t min_y;
    int32_t max_y;
    int32_t tmp;

    /* Sort vertices by Y */
    if (y0 > y1) {
        tmp = x0;
        x0  = x1;
        x1  = tmp;
        tmp = y0;
        y0  = y1;
        y1  = tmp;
    }
    if (y0 > y2) {
        tmp = x0;
        x0  = x2;
        x2  = tmp;
        tmp = y0;
        y0  = y2;
        y2  = tmp;
    }
    if (y1 > y2) {
        tmp = x1;
        x1  = x2;
        x2  = tmp;
        tmp = y1;
        y1  = y2;
        y2  = tmp;
    }

    min_y = y0;
    max_y = y2;

    for (int32_t row = min_y; row <= max_y; ++row) {
        float   xa;
        float   xb;
        int32_t left;
        int32_t right;

        /* Compute horizontal span using edge interpolation */
        if (y2 != y0) {
            xa = (float)x0 + (float)(x2 - x0) * (float)(row - y0) / (float)(y2 - y0);
        } else {
            xa = (float)x0;
        }

        if (row < y1) {
            if (y1 != y0) {
                xb = (float)x0 + (float)(x1 - x0) * (float)(row - y0) / (float)(y1 - y0);
            } else {
                xb = (float)x0;
            }
        } else {
            if (y2 != y1) {
                xb = (float)x1 + (float)(x2 - x1) * (float)(row - y1) / (float)(y2 - y1);
            } else {
                xb = (float)x1;
            }
        }

        left  = (int32_t)floorf(xa < xb ? xa : xb);
        right = (int32_t)ceilf(xa > xb ? xa : xb);

        prv_hline(gfx, left, right, row, col);
    }
}

void dtr_gfx_poly(dtr_graphics_t *gfx, const int32_t *pts, int32_t count, uint8_t col)
{
    if (count < 2) {
        return;
    }
    for (int32_t idx = 0; idx < count; ++idx) {
        int32_t next;

        next = (idx + 1) % count;
        dtr_gfx_line(gfx,
                     pts[(ptrdiff_t)idx * 2],
                     pts[(ptrdiff_t)idx * 2 + 1],
                     pts[(ptrdiff_t)next * 2],
                     pts[(ptrdiff_t)next * 2 + 1],
                     col);
    }
}

void dtr_gfx_polyfill(dtr_graphics_t *gfx, const int32_t *pts, int32_t count, uint8_t col)
{
    if (count < 3) {
        if (count == 2) {
            dtr_gfx_line(gfx, pts[0], pts[1], pts[2], pts[3], col);
        }
        return;
    }

    /* Fan-of-triangles from vertex 0 */
    for (int32_t idx = 1; idx < count - 1; ++idx) {
        dtr_gfx_trifill(gfx,
                        pts[0],
                        pts[1],
                        pts[(ptrdiff_t)idx * 2],
                        pts[(ptrdiff_t)idx * 2 + 1],
                        pts[(ptrdiff_t)(idx + 1) * 2],
                        pts[(ptrdiff_t)(idx + 1) * 2 + 1],
                        col);
    }
}

/* ------------------------------------------------------------------ */
/*  Text                                                               */
/* ------------------------------------------------------------------ */

/**
 * \\brief           Print a single character using the custom sprite-sheet font
 */
static void
prv_print_custom_char(dtr_graphics_t *gfx, uint8_t chr, int32_t x, int32_t y, uint8_t col)
{
    dtr_custom_font_t  *font;
    dtr_sprite_sheet_t *sht;
    int32_t             glyph_idx;
    int32_t             gx;
    int32_t             gy;
    int32_t             cam_x;
    int32_t             cam_y;
    int32_t             clip_l;
    int32_t             clip_t;
    int32_t             clip_r;
    int32_t             clip_b;

    font = &gfx->custom_font;
    sht  = &gfx->sheet;

    if (sht->pixels == NULL) {
        return;
    }

    glyph_idx = (int32_t)chr - (int32_t)font->first;
    if (glyph_idx < 0 || glyph_idx >= font->count) {
        return;
    }

    gx = font->sx + (glyph_idx % font->cols) * font->char_w;
    gy = font->sy + (glyph_idx / font->cols) * font->char_h;

    cam_x = gfx->camera_x;
    cam_y = gfx->camera_y;
    prv_get_clip_bounds(gfx, &clip_l, &clip_t, &clip_r, &clip_b);

    /* Early reject entire glyph */
    {
        int32_t scr_x0 = x - cam_x;
        int32_t scr_y0 = y - cam_y;
        int32_t scr_x1 = scr_x0 + font->char_w - 1;
        int32_t scr_y1 = scr_y0 + font->char_h - 1;

        if (scr_x1 < clip_l || scr_x0 > clip_r || scr_y1 < clip_t || scr_y0 > clip_b) {
            return;
        }
    }

    {
        int32_t  fb_w;
        uint16_t pat;
        uint8_t  mapped_col;

        fb_w       = gfx->width;
        pat        = gfx->fill_pattern;
        mapped_col = gfx->draw_pal[col];

        for (int32_t row = 0; row < font->char_h; ++row) {
            int32_t scr_y = y + row - cam_y;

            if (scr_y < clip_t || scr_y > clip_b) {
                continue;
            }

            {
                uint8_t *dst_row = &gfx->framebuffer[(ptrdiff_t)scr_y * fb_w];
                uint16_t row_pat = prv_row_pattern(pat, scr_y);

                for (int32_t px = 0; px < font->char_w; ++px) {
                    int32_t src_x;
                    int32_t src_y;
                    int32_t scr_x;

                    scr_x = x + px - cam_x;
                    if (scr_x < clip_l || scr_x > clip_r) {
                        continue;
                    }

                    src_x = gx + px;
                    src_y = gy + row;
                    if (src_x < 0 || src_x >= sht->width || src_y < 0 || src_y >= sht->height) {
                        continue;
                    }

                    if (gfx->transparent[sht->pixels[src_y * sht->width + src_x]]) {
                        continue;
                    }

                    if (pat == 0 || (row_pat & (1 << (scr_x & 3)))) {
                        dst_row[scr_x] = mapped_col;
                    }
                }
            }
        }
    }
}

int32_t dtr_gfx_print(dtr_graphics_t *gfx, const char *str, int32_t x, int32_t y, uint8_t col)
{
    int32_t     ox;
    const char *ptr;
    int32_t     cw;
    int32_t     ch;
    int32_t     cam_x;
    int32_t     cam_y;
    int32_t     clip_l;
    int32_t     clip_t;
    int32_t     clip_r;
    int32_t     clip_b;
    int32_t     fb_w;
    uint16_t    pat;
    uint8_t     mapped_col;

    ox    = x;
    cw    = gfx->custom_font.active ? gfx->custom_font.char_w : DTR_FONT_W;
    ch    = gfx->custom_font.active ? gfx->custom_font.char_h : DTR_FONT_H;
    cam_x = gfx->camera_x;
    cam_y = gfx->camera_y;
    prv_get_clip_bounds(gfx, &clip_l, &clip_t, &clip_r, &clip_b);
    fb_w       = gfx->width;
    pat        = gfx->fill_pattern;
    mapped_col = gfx->draw_pal[col];

    for (ptr = str; *ptr != '\0'; ++ptr) {
        uint8_t chr;

        chr = (uint8_t)*ptr;
        if (chr == '\n') {
            x = ox;
            y += ch;
            continue;
        }

        if (gfx->custom_font.active) {
            int32_t glyph_idx;

            glyph_idx = (int32_t)chr - (int32_t)gfx->custom_font.first;
            if (glyph_idx < 0 || glyph_idx >= gfx->custom_font.count) {
                x += cw + 1;
                continue;
            }
            prv_print_custom_char(gfx, chr, x, y, col);
        } else {
            int32_t scr_x0;
            int32_t scr_y0;

            if (chr < DTR_FONT_FIRST || chr > DTR_FONT_LAST) {
                x += cw + 1;
                continue;
            }

            /* Early reject entire glyph */
            scr_x0 = x - cam_x;
            scr_y0 = y - cam_y;
            if (scr_x0 + DTR_FONT_W - 1 < clip_l || scr_x0 > clip_r ||
                scr_y0 + DTR_FONT_H - 1 < clip_t || scr_y0 > clip_b) {
                x += cw + 1;
                continue;
            }

            {
                const unsigned char *glyph;
                int32_t              glyph_idx;

                glyph_idx = chr - DTR_FONT_FIRST;
                glyph     = DTR_FONT_DATA[glyph_idx];

                for (int32_t row = 0; row < DTR_FONT_H; ++row) {
                    int32_t  scr_y;
                    uint8_t *dst_row;
                    uint16_t row_pat;

                    scr_y = scr_y0 + row;
                    if (scr_y < clip_t || scr_y > clip_b) {
                        continue;
                    }

                    dst_row = &gfx->framebuffer[(ptrdiff_t)scr_y * fb_w];
                    row_pat = prv_row_pattern(pat, scr_y);

                    for (int32_t bit = 0; bit < DTR_FONT_W; ++bit) {
                        int32_t scr_x;

                        if (!(glyph[row] & (0x8 >> bit))) {
                            continue;
                        }
                        scr_x = scr_x0 + bit;
                        if (scr_x < clip_l || scr_x > clip_r) {
                            continue;
                        }
                        if (pat == 0 || (row_pat & (1 << (scr_x & 3)))) {
                            dst_row[scr_x] = mapped_col;
                        }
                    }
                }
            }
        }

        x += cw + 1;
    }

    /* Update cursor */
    gfx->cursor_x = x;
    gfx->cursor_y = y;

    return x - ox;
}

int32_t dtr_gfx_text_width(dtr_graphics_t *gfx, const char *str)
{
    const char *ptr;
    int32_t     cw;
    int32_t     line_w;
    int32_t     max_w;

    cw     = gfx->custom_font.active ? gfx->custom_font.char_w : DTR_FONT_W;
    line_w = 0;
    max_w  = 0;

    for (ptr = str; *ptr != '\0'; ++ptr) {
        if ((uint8_t)*ptr == '\n') {
            if (line_w > 0) {
                line_w -= 1;
            }
            if (line_w > max_w) {
                max_w = line_w;
            }
            line_w = 0;
            continue;
        }
        line_w += cw + 1;
    }
    if (line_w > 0) {
        line_w -= 1;
    }
    if (line_w > max_w) {
        max_w = line_w;
    }
    return max_w;
}

int32_t dtr_gfx_text_height(dtr_graphics_t *gfx, const char *str)
{
    const char *ptr;
    int32_t     ch;
    int32_t     lines;

    ch    = gfx->custom_font.active ? gfx->custom_font.char_h : DTR_FONT_H;
    lines = 1;

    for (ptr = str; *ptr != '\0'; ++ptr) {
        if ((uint8_t)*ptr == '\n') {
            ++lines;
        }
    }
    return lines * ch;
}

void dtr_gfx_font(dtr_graphics_t *gfx,
                  int32_t         sx,
                  int32_t         sy,
                  int32_t         char_w,
                  int32_t         char_h,
                  char            first,
                  int32_t         count)
{
    dtr_custom_font_t *font;

    font = &gfx->custom_font;

    if (char_w <= 0 || char_h <= 0 || count <= 0) {
        font->active = false;
        return;
    }

    font->sx     = sx;
    font->sy     = sy;
    font->char_w = char_w;
    font->char_h = char_h;
    font->first  = first;
    font->count  = count;

    /* Compute columns from sheet width */
    if (gfx->sheet.pixels != NULL && gfx->sheet.width > 0) {
        font->cols = (gfx->sheet.width - sx) / char_w;
    } else {
        font->cols = count; /* single row fallback */
    }
    if (font->cols < 1) {
        font->cols = 1;
    }

    font->active = true;
}

void dtr_gfx_font_reset(dtr_graphics_t *gfx)
{
    gfx->custom_font.active = false;
}

/* ------------------------------------------------------------------ */
/*  Sprites                                                            */
/* ------------------------------------------------------------------ */

void dtr_gfx_spr(dtr_graphics_t *gfx,
                 int32_t         idx,
                 int32_t         x,
                 int32_t         y,
                 int32_t         w_tiles,
                 int32_t         h_tiles,
                 bool            flip_x,
                 bool            flip_y)
{
    dtr_sprite_sheet_t *sht;
    int32_t             sx0;
    int32_t             sy0;
    int32_t             tile_w;
    int32_t             tile_h;
    int32_t             px_w;
    int32_t             px_h;
    int32_t             cam_x;
    int32_t             cam_y;
    int32_t             clip_l;
    int32_t             clip_t;
    int32_t             clip_r;
    int32_t             clip_b;
    int32_t             dst_x0;
    int32_t             dst_y0;
    int32_t             dst_x1;
    int32_t             dst_y1;
    int32_t             sht_w;

    sht = &gfx->sheet;
    if (sht->pixels == NULL || idx < 0 || idx >= sht->count) {
        return;
    }
    if (w_tiles <= 0) {
        w_tiles = 1;
    }
    if (h_tiles <= 0) {
        h_tiles = 1;
    }

    tile_w = sht->tile_w;
    tile_h = sht->tile_h;
    sx0    = (idx % sht->cols) * tile_w;
    sy0    = (idx / sht->cols) * tile_h;
    px_w   = w_tiles * tile_w;
    px_h   = h_tiles * tile_h;
    sht_w  = sht->width;

    /* Precompute camera-adjusted destination rect */
    cam_x  = gfx->camera_x;
    cam_y  = gfx->camera_y;
    dst_x0 = x - cam_x;
    dst_y0 = y - cam_y;
    dst_x1 = dst_x0 + px_w - 1;
    dst_y1 = dst_y0 + px_h - 1;

    /* Clip rect bounds */
    prv_get_clip_bounds(gfx, &clip_l, &clip_t, &clip_r, &clip_b);

    /* Early reject: sprite entirely outside clip rect */
    if (dst_x1 < clip_l || dst_x0 > clip_r || dst_y1 < clip_t || dst_y0 > clip_b) {
        return;
    }

    /* Clamp visible region to clip rect */
    {
        int32_t  vis_x0;
        int32_t  vis_y0;
        int32_t  vis_x1;
        int32_t  vis_y1;
        int32_t  fb_w;
        uint16_t pat;

        vis_x0 = (dst_x0 > clip_l) ? dst_x0 : clip_l;
        vis_y0 = (dst_y0 > clip_t) ? dst_y0 : clip_t;
        vis_x1 = (dst_x1 < clip_r) ? dst_x1 : clip_r;
        vis_y1 = (dst_y1 < clip_b) ? dst_y1 : clip_b;
        fb_w   = gfx->width;
        pat    = gfx->fill_pattern;

        /*
         * Fast path: single-tile sprite, no fill pattern.
         * Source coords are guaranteed in-range because the
         * visible region is clipped to the destination rect
         * which maps 1:1 onto a single tile.
         */
        if (w_tiles == 1 && h_tiles == 1 && pat == 0) {
            const bool    *transp = gfx->transparent;
            const uint8_t *dpal   = gfx->draw_pal;
            const uint8_t *sheet  = sht->pixels;

            for (int32_t scr_y = vis_y0; scr_y <= vis_y1; ++scr_y) {
                int32_t        row;
                int32_t        src_y;
                int32_t        row_off;
                uint8_t       *dst_row;
                const uint8_t *src_ptr;
                int32_t        span;

                row     = scr_y - dst_y0;
                src_y   = flip_y ? (tile_h - 1 - row) : row;
                row_off = (src_y + sy0) * sht_w + sx0;
                dst_row = &gfx->framebuffer[(ptrdiff_t)scr_y * fb_w];

                /*
                 * Non-flipped fast path: source pixels are contiguous so
                 * we can process 4 at a time, checking transparency via
                 * a SIMD comparison mask.
                 */
#if DTR_HAS_SIMD
                if (!flip_x) {
                    int32_t start_col;
                    int32_t scr_x;

                    start_col = vis_x0 - dst_x0;
                    src_ptr   = &sheet[row_off + start_col];
                    scr_x     = vis_x0;
                    span      = vis_x1 - vis_x0 + 1;

                    prv_blit_span(dst_row, src_ptr, scr_x, span, transp, dpal);
                } else {
                    int32_t        end_col;
                    const uint8_t *src_end;
                    int32_t        scr_x;

                    /*
                     * src_end points to the last source pixel that maps
                     * to vis_x0 (the leftmost visible destination pixel).
                     * For a flip, destination pixel vis_x0 reads from
                     * source column (tile_w - 1 - (vis_x0 - dst_x0)).
                     */
                    end_col = tile_w - 1 - (vis_x0 - dst_x0);
                    src_end = &sheet[row_off + end_col];
                    scr_x   = vis_x0;
                    span    = vis_x1 - vis_x0 + 1;

                    prv_blit_span_flip(dst_row, src_end, scr_x, span, transp, dpal);
                }
#else
                {
                    for (int32_t scr_x = vis_x0; scr_x <= vis_x1; ++scr_x) {
                        int32_t col_idx;
                        int32_t src_x;
                        uint8_t col;

                        col_idx = scr_x - dst_x0;
                        src_x   = flip_x ? (tile_w - 1 - col_idx) : col_idx;
                        col     = sheet[row_off + src_x];
                        if (transp[col]) {
                            continue;
                        }
                        dst_row[scr_x] = dpal[col];
                    }
                }
#endif /* DTR_HAS_SIMD */
            }
            return;
        }

        /* General path: multi-tile and/or fill-pattern */
        for (int32_t scr_y = vis_y0; scr_y <= vis_y1; ++scr_y) {
            int32_t  row;
            int32_t  src_y;
            int32_t  row_off;
            uint8_t *dst_row;
            uint16_t row_pat;

            row   = scr_y - dst_y0;
            src_y = flip_y ? (px_h - 1 - row) : row;
            src_y += sy0;
            if (src_y < 0 || src_y >= sht->height) {
                continue;
            }
            row_off = src_y * sht_w;
            dst_row = &gfx->framebuffer[(ptrdiff_t)scr_y * fb_w];

            /* Pre-shift pattern row once per scanline */
            row_pat = prv_row_pattern(pat, scr_y);

            for (int32_t scr_x = vis_x0; scr_x <= vis_x1; ++scr_x) {
                int32_t col_idx;
                int32_t src_x;
                uint8_t col;

                col_idx = scr_x - dst_x0;
                src_x   = flip_x ? (px_w - 1 - col_idx) : col_idx;
                src_x += sx0;
                if (src_x < 0 || src_x >= sht_w) {
                    continue;
                }

                col = sht->pixels[row_off + src_x];
                if (gfx->transparent[col]) {
                    continue;
                }
                col = gfx->draw_pal[col];

                if (pat != 0 && !(row_pat & (1 << (scr_x & 3)))) {
                    continue;
                }

                dst_row[scr_x] = col;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Batch sprite draw — single-tile sprites, no fill pattern           */
/* ------------------------------------------------------------------ */

void dtr_gfx_spr_batch(dtr_graphics_t *gfx, const int32_t *data, int32_t count)
{
    dtr_sprite_sheet_t *sht;
    const bool         *transp;
    const uint8_t      *dpal;
    const uint8_t      *sheet;
    int32_t             tile_w;
    int32_t             tile_h;
    int32_t             sht_w;
    int32_t             cam_x;
    int32_t             cam_y;
    int32_t             clip_l;
    int32_t             clip_t;
    int32_t             clip_r;
    int32_t             clip_b;
    int32_t             fb_w;
    int32_t             i;

    if (data == NULL || count <= 0) {
        return;
    }

    sht = &gfx->sheet;
    if (sht->pixels == NULL) {
        return;
    }

    /* Hoist invariants out of per-sprite loop */
    transp = gfx->transparent;
    dpal   = gfx->draw_pal;
    sheet  = sht->pixels;
    tile_w = sht->tile_w;
    tile_h = sht->tile_h;
    sht_w  = sht->width;
    cam_x  = gfx->camera_x;
    cam_y  = gfx->camera_y;
    fb_w   = gfx->width;

    prv_get_clip_bounds(gfx, &clip_l, &clip_t, &clip_r, &clip_b);

    for (i = 0; i < count; ++i) {
        int32_t   idx;
        int32_t   x;
        int32_t   y;
        bool      flip_x;
        bool      flip_y;
        int32_t   sx0;
        int32_t   sy0;
        int32_t   dst_x0;
        int32_t   dst_y0;
        int32_t   dst_x1;
        int32_t   dst_y1;
        int32_t   vis_x0;
        int32_t   vis_y0;
        int32_t   vis_x1;
        int32_t   vis_y1;
        ptrdiff_t base;

        base   = (ptrdiff_t)i * 5;
        idx    = data[base + 0];
        x      = data[base + 1];
        y      = data[base + 2];
        flip_x = data[base + 3] != 0;
        flip_y = data[base + 4] != 0;

        if (idx < 0 || idx >= sht->count) {
            continue;
        }

        sx0 = (idx % sht->cols) * tile_w;
        sy0 = (idx / sht->cols) * tile_h;

        dst_x0 = x - cam_x;
        dst_y0 = y - cam_y;
        dst_x1 = dst_x0 + tile_w - 1;
        dst_y1 = dst_y0 + tile_h - 1;

        /* Early reject */
        if (dst_x1 < clip_l || dst_x0 > clip_r || dst_y1 < clip_t || dst_y0 > clip_b) {
            continue;
        }

        /* Clamp visible region */
        vis_x0 = (dst_x0 > clip_l) ? dst_x0 : clip_l;
        vis_y0 = (dst_y0 > clip_t) ? dst_y0 : clip_t;
        vis_x1 = (dst_x1 < clip_r) ? dst_x1 : clip_r;
        vis_y1 = (dst_y1 < clip_b) ? dst_y1 : clip_b;

        for (int32_t scr_y = vis_y0; scr_y <= vis_y1; ++scr_y) {
            int32_t  row;
            int32_t  src_y;
            int32_t  row_off;
            uint8_t *dst_row;

            row     = scr_y - dst_y0;
            src_y   = flip_y ? (tile_h - 1 - row) : row;
            row_off = (src_y + sy0) * sht_w + sx0;
            dst_row = &gfx->framebuffer[(ptrdiff_t)scr_y * fb_w];

#if DTR_HAS_SIMD
            if (!flip_x) {
                int32_t        start_col;
                const uint8_t *src_ptr;
                int32_t        span;

                start_col = vis_x0 - dst_x0;
                src_ptr   = &sheet[row_off + start_col];
                span      = vis_x1 - vis_x0 + 1;

                prv_blit_span(dst_row, src_ptr, vis_x0, span, transp, dpal);
            } else {
                int32_t        end_col;
                const uint8_t *src_end;
                int32_t        span;

                end_col = tile_w - 1 - (vis_x0 - dst_x0);
                src_end = &sheet[row_off + end_col];
                span    = vis_x1 - vis_x0 + 1;

                prv_blit_span_flip(dst_row, src_end, vis_x0, span, transp, dpal);
            }
#else
            {
                for (int32_t scr_x = vis_x0; scr_x <= vis_x1; ++scr_x) {
                    int32_t col_idx;
                    int32_t src_x;
                    uint8_t col;

                    col_idx = scr_x - dst_x0;
                    src_x   = flip_x ? (tile_w - 1 - col_idx) : col_idx;
                    col     = sheet[row_off + src_x];
                    if (transp[col]) {
                        continue;
                    }
                    dst_row[scr_x] = dpal[col];
                }
            }
#endif /* DTR_HAS_SIMD */
        }
    }
}

void dtr_gfx_sspr(dtr_graphics_t *gfx,
                  int32_t         sx,
                  int32_t         sy,
                  int32_t         sw,
                  int32_t         sh,
                  int32_t         dx,
                  int32_t         dy,
                  int32_t         dw,
                  int32_t         dh)
{
    dtr_sprite_sheet_t *sht;

    sht = &gfx->sheet;
    if (sht->pixels == NULL || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
        return;
    }

    /* Sanity-limit destination size to prevent overflow in fixed-point math */
    if (dw > 16384 || dh > 16384) {
        return;
    }

    {
        int32_t        cam_x;
        int32_t        cam_y;
        int32_t        dst_x0;
        int32_t        dst_y0;
        int32_t        dst_x1;
        int32_t        dst_y1;
        int32_t        clip_l;
        int32_t        clip_t;
        int32_t        clip_r;
        int32_t        clip_b;
        int32_t        vis_x0;
        int32_t        vis_y0;
        int32_t        vis_x1;
        int32_t        vis_y1;
        int32_t        step_x;
        int32_t        step_y;
        int32_t        fb_w;
        uint16_t       pat;
        const bool    *transp;
        const uint8_t *dpal;

        cam_x  = gfx->camera_x;
        cam_y  = gfx->camera_y;
        dst_x0 = dx - cam_x;
        dst_y0 = dy - cam_y;
        dst_x1 = dst_x0 + dw - 1;
        dst_y1 = dst_y0 + dh - 1;

        prv_get_clip_bounds(gfx, &clip_l, &clip_t, &clip_r, &clip_b);

        if (dst_x1 < clip_l || dst_x0 > clip_r || dst_y1 < clip_t || dst_y0 > clip_b) {
            return;
        }

        vis_x0 = (dst_x0 > clip_l) ? dst_x0 : clip_l;
        vis_y0 = (dst_y0 > clip_t) ? dst_y0 : clip_t;
        vis_x1 = (dst_x1 < clip_r) ? dst_x1 : clip_r;
        vis_y1 = (dst_y1 < clip_b) ? dst_y1 : clip_b;

        /* Fixed-point 16.16 stepping */
        step_x = (sw << 16) / dw;
        step_y = (sh << 16) / dh;
        fb_w   = gfx->width;
        pat    = gfx->fill_pattern;
        transp = gfx->transparent;
        dpal   = gfx->draw_pal;

        for (int32_t scr_y = vis_y0; scr_y <= vis_y1; ++scr_y) {
            int32_t  row;
            int32_t  src_y;
            uint8_t *src_row_ptr;
            uint8_t *dst_row;
            uint16_t row_pat;
            int32_t  x_acc;

            row   = scr_y - dst_y0;
            src_y = sy + ((row * step_y) >> 16);
            if (src_y < 0 || src_y >= sht->height) {
                continue;
            }

            src_row_ptr = sht->pixels + (ptrdiff_t)src_y * sht->width;
            dst_row     = &gfx->framebuffer[(ptrdiff_t)scr_y * fb_w];
            row_pat     = prv_row_pattern(pat, scr_y);
            x_acc       = (vis_x0 - dst_x0) * step_x;

            for (int32_t scr_x = vis_x0; scr_x <= vis_x1; ++scr_x) {
                int32_t src_x;
                uint8_t col;

                src_x = sx + (x_acc >> 16);
                if (src_x >= 0 && src_x < sht->width) {
                    col = src_row_ptr[src_x];
                    if (!transp[col]) {
                        if (pat == 0 || (row_pat & (1 << (scr_x & 3)))) {
                            dst_row[scr_x] = dpal[col];
                        }
                    }
                }
                x_acc += step_x;
            }
        }
    }
}

/**
 * Shared affine sprite transform.
 *
 * Both spr_rot and spr_affine use the same structure:
 *   Forward:  dx = fwd_a * rx - fwd_b * ry,  dy = fwd_b * rx + fwd_a * ry
 *   Inverse:  sx = inv_a * dx + inv_b * dy + ox,  sy = -inv_b * dx + inv_a * dy + oy
 */
static void prv_spr_xform(dtr_graphics_t *gfx,
                          int32_t         sx0,
                          int32_t         sy0,
                          int32_t         tile_w,
                          int32_t         tile_h,
                          int32_t         x,
                          int32_t         y,
                          float           fwd_a,
                          float           fwd_b,
                          float           inv_a,
                          float           inv_b,
                          float           ox,
                          float           oy)
{
    int32_t        sht_w;
    int32_t        cam_x;
    int32_t        cam_y;
    int32_t        clip_l;
    int32_t        clip_t;
    int32_t        clip_r;
    int32_t        clip_b;
    int32_t        fb_w;
    uint16_t       pat;
    const bool    *transp;
    const uint8_t *dpal;
    float          min_dx;
    float          max_dx;
    float          min_dy;
    float          max_dy;
    int32_t        d_x0;
    int32_t        d_x1;
    int32_t        d_y0;
    int32_t        d_y1;
    int32_t        vis_x0;
    int32_t        vis_y0;
    int32_t        vis_x1;
    int32_t        vis_y1;

    sht_w = gfx->sheet.width;
    cam_x = gfx->camera_x;
    cam_y = gfx->camera_y;
    prv_get_clip_bounds(gfx, &clip_l, &clip_t, &clip_r, &clip_b);
    fb_w   = gfx->width;
    pat    = gfx->fill_pattern;
    transp = gfx->transparent;
    dpal   = gfx->draw_pal;

    /* Forward-transform the 4 source corners to find output AABB */
    min_dx = 1e9f;
    max_dx = -1e9f;
    min_dy = 1e9f;
    max_dy = -1e9f;

    for (int32_t c = 0; c < 4; ++c) {
        float rx;
        float ry;
        float od_x;
        float od_y;
        float sx_f;
        float sy_f;

        sx_f = (c & 1) ? (float)(tile_w - 1) : 0.0f;
        sy_f = (c & 2) ? (float)(tile_h - 1) : 0.0f;
        rx   = sx_f - ox;
        ry   = sy_f - oy;
        od_x = fwd_a * rx - fwd_b * ry;
        od_y = fwd_b * rx + fwd_a * ry;

        if (od_x < min_dx) {
            min_dx = od_x;
        }
        if (od_x > max_dx) {
            max_dx = od_x;
        }
        if (od_y < min_dy) {
            min_dy = od_y;
        }
        if (od_y > max_dy) {
            max_dy = od_y;
        }
    }

    d_x0 = (int32_t)floorf(min_dx);
    d_x1 = (int32_t)ceilf(max_dx);
    d_y0 = (int32_t)floorf(min_dy);
    d_y1 = (int32_t)ceilf(max_dy);

    /* Clip to screen */
    vis_x0 = x + d_x0 - cam_x;
    vis_y0 = y + d_y0 - cam_y;
    vis_x1 = x + d_x1 - cam_x;
    vis_y1 = y + d_y1 - cam_y;

    if (vis_x1 < clip_l || vis_x0 > clip_r || vis_y1 < clip_t || vis_y0 > clip_b) {
        return;
    }

    if (vis_x0 < clip_l) {
        d_x0 += clip_l - vis_x0;
    }
    if (vis_y0 < clip_t) {
        d_y0 += clip_t - vis_y0;
    }
    if (vis_x1 > clip_r) {
        d_x1 -= vis_x1 - clip_r;
    }
    if (vis_y1 > clip_b) {
        d_y1 -= vis_y1 - clip_b;
    }

    for (int32_t dy_val = d_y0; dy_val <= d_y1; ++dy_val) {
        int32_t  scr_y;
        uint8_t *dst_row;
        uint16_t row_pat;
        float    base_fx;
        float    base_fy;

        scr_y   = y + dy_val - cam_y;
        dst_row = &gfx->framebuffer[(ptrdiff_t)scr_y * fb_w];
        row_pat = prv_row_pattern(pat, scr_y);

        /* Precompute the dy_val contribution to the inverse transform */
        base_fx = inv_b * (float)dy_val + ox;
        base_fy = inv_a * (float)dy_val + oy;

        for (int32_t dx_val = d_x0; dx_val <= d_x1; ++dx_val) {
            int32_t src_x;
            int32_t src_y;
            uint8_t col;
            float   fx;
            float   fy;
            int32_t scr_x;

            fx = inv_a * (float)dx_val + base_fx;
            fy = -inv_b * (float)dx_val + base_fy;

            src_x = (int32_t)fx;
            src_y = (int32_t)fy;

            if (src_x < 0 || src_x >= tile_w || src_y < 0 || src_y >= tile_h) {
                continue;
            }

            col = gfx->sheet.pixels[(sy0 + src_y) * sht_w + (sx0 + src_x)];
            if (transp[col]) {
                continue;
            }

            scr_x = x + dx_val - cam_x;
            if (pat == 0 || (row_pat & (1 << (scr_x & 3)))) {
                dst_row[scr_x] = dpal[col];
            }
        }
    }
}

void dtr_gfx_spr_rot(dtr_graphics_t *gfx,
                     int32_t         idx,
                     int32_t         x,
                     int32_t         y,
                     float           angle,
                     int32_t         cx,
                     int32_t         cy)
{
    dtr_sprite_sheet_t *sht;
    int32_t             tile_w;
    int32_t             tile_h;
    float               cos_a;
    float               sin_a;

    sht = &gfx->sheet;
    if (sht->pixels == NULL || idx < 0 || idx >= sht->count) {
        return;
    }

    tile_w = sht->tile_w;
    tile_h = sht->tile_h;
    cos_a  = cosf(angle);
    sin_a  = sinf(angle);

    /* Rotation is orthonormal: forward = (cos, sin), inverse = (cos, sin) */
    prv_spr_xform(gfx,
                  (idx % sht->cols) * tile_w,
                  (idx / sht->cols) * tile_h,
                  tile_w,
                  tile_h,
                  x,
                  y,
                  cos_a,
                  sin_a,
                  cos_a,
                  sin_a,
                  (float)cx,
                  (float)cy);
}

void dtr_gfx_spr_affine(dtr_graphics_t *gfx,
                        int32_t         idx,
                        int32_t         x,
                        int32_t         y,
                        float           origin_x,
                        float           origin_y,
                        float           rot_x,
                        float           rot_y)
{
    dtr_sprite_sheet_t *sht;
    int32_t             tile_w;
    int32_t             tile_h;
    float               det;
    float               inv_det;

    sht = &gfx->sheet;
    if (sht->pixels == NULL || idx < 0 || idx >= sht->count) {
        return;
    }

    det = rot_x * rot_x + rot_y * rot_y;
    if (det < 0.001f) {
        return;
    }

    tile_w  = sht->tile_w;
    tile_h  = sht->tile_h;
    inv_det = 1.0f / det;

    /* Affine: forward = (rot_x, rot_y), inverse = (rot_x/det, rot_y/det) */
    prv_spr_xform(gfx,
                  (idx % sht->cols) * tile_w,
                  (idx / sht->cols) * tile_h,
                  tile_w,
                  tile_h,
                  x,
                  y,
                  rot_x,
                  rot_y,
                  rot_x * inv_det,
                  rot_y * inv_det,
                  origin_x,
                  origin_y);
}

/* ------------------------------------------------------------------ */
/*  Batch tilemap draw                                                 */
/* ------------------------------------------------------------------ */

void dtr_gfx_map_draw(dtr_graphics_t *gfx,
                      const int32_t  *tiles,
                      int32_t         map_w,
                      int32_t         map_h,
                      int32_t         src_x,
                      int32_t         src_y,
                      int32_t         num_w,
                      int32_t         num_h,
                      int32_t         dst_x,
                      int32_t         dst_y,
                      int32_t         tile_pw,
                      int32_t         tile_ph)
{
    dtr_sprite_sheet_t *sht;
    int32_t             sht_tw;
    int32_t             sht_th;
    int32_t             sht_w;
    int32_t             sht_cols;
    int32_t             sht_count;
    int32_t             cam_x;
    int32_t             cam_y;
    int32_t             clip_l;
    int32_t             clip_t;
    int32_t             clip_r;
    int32_t             clip_b;
    int32_t             fb_w;
    uint16_t            pat;
    const bool         *transp;
    const uint8_t      *dpal;
    const uint8_t      *sheet;

    sht = &gfx->sheet;
    if (sht->pixels == NULL || tiles == NULL || num_w <= 0 || num_h <= 0) {
        return;
    }

    sht_tw    = sht->tile_w;
    sht_th    = sht->tile_h;
    sht_w     = sht->width;
    sht_cols  = sht->cols;
    sht_count = sht->count;
    cam_x     = gfx->camera_x;
    cam_y     = gfx->camera_y;
    prv_get_clip_bounds(gfx, &clip_l, &clip_t, &clip_r, &clip_b);
    fb_w   = gfx->width;
    pat    = gfx->fill_pattern;
    transp = gfx->transparent;
    dpal   = gfx->draw_pal;
    sheet  = sht->pixels;

    for (int32_t ty = 0; ty < num_h; ++ty) {
        int32_t my;
        int32_t tile_dy;
        int32_t tile_dy1;

        my = src_y + ty;
        if (my < 0 || my >= map_h) {
            continue;
        }

        tile_dy  = dst_y + ty * tile_ph - cam_y;
        tile_dy1 = tile_dy + sht_th - 1;

        /* Skip entire row if off-screen */
        if (tile_dy1 < clip_t || tile_dy > clip_b) {
            continue;
        }

        for (int32_t tx = 0; tx < num_w; ++tx) {
            int32_t mx;
            int32_t tile;
            int32_t idx;
            int32_t sx0;
            int32_t sy0;
            int32_t tile_dx;
            int32_t tile_dx1;
            int32_t vis_x0;
            int32_t vis_y0;
            int32_t vis_x1;
            int32_t vis_y1;

            mx = src_x + tx;
            if (mx < 0 || mx >= map_w) {
                continue;
            }

            tile = tiles[my * map_w + mx];
            if (tile <= 0) {
                continue;
            }

            idx = tile - 1;
            if (idx >= sht_count) {
                continue;
            }

            /* Dest rect for this tile */
            tile_dx  = dst_x + tx * tile_pw - cam_x;
            tile_dx1 = tile_dx + sht_tw - 1;

            /* Skip tile if off-screen horizontally */
            if (tile_dx1 < clip_l || tile_dx > clip_r) {
                continue;
            }

            /* Source position in sprite sheet */
            sx0 = (idx % sht_cols) * sht_tw;
            sy0 = (idx / sht_cols) * sht_th;

            /* Visible region */
            vis_x0 = (tile_dx > clip_l) ? tile_dx : clip_l;
            vis_y0 = (tile_dy > clip_t) ? tile_dy : clip_t;
            vis_x1 = (tile_dx1 < clip_r) ? tile_dx1 : clip_r;
            vis_y1 = (tile_dy1 < clip_b) ? tile_dy1 : clip_b;

            for (int32_t scr_y = vis_y0; scr_y <= vis_y1; ++scr_y) {
                int32_t        src_row;
                int32_t        row_off;
                uint8_t       *dst_row;
                uint16_t       row_pat;
                const uint8_t *src_ptr;
                int32_t        span;

                src_row = scr_y - tile_dy;
                row_off = (sy0 + src_row) * sht_w + sx0;
                dst_row = &gfx->framebuffer[(ptrdiff_t)scr_y * fb_w];
                row_pat = prv_row_pattern(pat, scr_y);

#if DTR_HAS_SIMD
                if (pat == 0) {
                    int32_t start_col;
                    int32_t scr_x;

                    start_col = vis_x0 - tile_dx;
                    src_ptr   = &sheet[row_off + start_col];
                    scr_x     = vis_x0;
                    span      = vis_x1 - vis_x0 + 1;

                    prv_blit_span(dst_row, src_ptr, scr_x, span, transp, dpal);
                } else
#endif /* DTR_HAS_SIMD */
                {
                    for (int32_t scr_x = vis_x0; scr_x <= vis_x1; ++scr_x) {
                        int32_t src_col;
                        uint8_t col;

                        src_col = scr_x - tile_dx;
                        col     = sheet[row_off + sx0 + src_col];
                        if (transp[col]) {
                            continue;
                        }
                        if (pat != 0 && !(row_pat & (1 << (scr_x & 3)))) {
                            continue;
                        }
                        dst_row[scr_x] = dpal[col];
                    }
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Draw list (sprite batch)                                            */
/* ------------------------------------------------------------------ */

void dtr_gfx_dl_begin(dtr_graphics_t *gfx)
{
    gfx->draw_list.count  = 0;
    gfx->draw_list.active = true;
}

static int prv_draw_cmd_cmp(const void *a, const void *b)
{
    const dtr_draw_cmd_t *ca;
    const dtr_draw_cmd_t *cb;

    ca = (const dtr_draw_cmd_t *)a;
    cb = (const dtr_draw_cmd_t *)b;
    if (ca->layer != cb->layer) {
        return (ca->layer < cb->layer) ? -1 : 1;
    }
    return (ca->order < cb->order) ? -1 : 1;
}

void dtr_gfx_dl_spr(dtr_graphics_t *gfx,
                    int32_t         layer,
                    int32_t         idx,
                    int32_t         x,
                    int32_t         y,
                    int32_t         w,
                    int32_t         h,
                    bool            flip_x,
                    bool            flip_y)
{
    dtr_draw_cmd_t *cmd;

    if (gfx->draw_list.count >= CONSOLE_MAX_DRAW_CMDS) {
        fprintf(stderr, "Draw list: max commands reached, dropping command\n");
        return;
    }
    cmd            = &gfx->draw_list.cmds[gfx->draw_list.count];
    cmd->type      = DTR_DRAW_SPR;
    cmd->layer     = layer;
    cmd->order     = gfx->draw_list.count;
    cmd->u.spr.idx = idx;
    cmd->u.spr.x   = x;
    cmd->u.spr.y   = y;
    cmd->u.spr.w   = w;
    cmd->u.spr.h   = h;
    cmd->u.spr.fx  = flip_x;
    cmd->u.spr.fy  = flip_y;
    ++gfx->draw_list.count;
}

void dtr_gfx_dl_sspr(dtr_graphics_t *gfx,
                     int32_t         layer,
                     int32_t         sx,
                     int32_t         sy,
                     int32_t         sw,
                     int32_t         sh,
                     int32_t         dx,
                     int32_t         dy,
                     int32_t         dw,
                     int32_t         dh)
{
    dtr_draw_cmd_t *cmd;

    if (gfx->draw_list.count >= CONSOLE_MAX_DRAW_CMDS) {
        fprintf(stderr, "Draw list: max commands reached, dropping command\n");
        return;
    }
    cmd            = &gfx->draw_list.cmds[gfx->draw_list.count];
    cmd->type      = DTR_DRAW_SSPR;
    cmd->layer     = layer;
    cmd->order     = gfx->draw_list.count;
    cmd->u.sspr.sx = sx;
    cmd->u.sspr.sy = sy;
    cmd->u.sspr.sw = sw;
    cmd->u.sspr.sh = sh;
    cmd->u.sspr.dx = dx;
    cmd->u.sspr.dy = dy;
    cmd->u.sspr.dw = dw;
    cmd->u.sspr.dh = dh;
    ++gfx->draw_list.count;
}

void dtr_gfx_dl_spr_rot(dtr_graphics_t *gfx,
                        int32_t         layer,
                        int32_t         idx,
                        int32_t         x,
                        int32_t         y,
                        float           angle,
                        int32_t         cx,
                        int32_t         cy)
{
    dtr_draw_cmd_t *cmd;

    if (gfx->draw_list.count >= CONSOLE_MAX_DRAW_CMDS) {
        fprintf(stderr, "Draw list: max commands reached, dropping command\n");
        return;
    }
    cmd                  = &gfx->draw_list.cmds[gfx->draw_list.count];
    cmd->type            = DTR_DRAW_SPR_ROT;
    cmd->layer           = layer;
    cmd->order           = gfx->draw_list.count;
    cmd->u.spr_rot.idx   = idx;
    cmd->u.spr_rot.x     = x;
    cmd->u.spr_rot.y     = y;
    cmd->u.spr_rot.angle = angle;
    cmd->u.spr_rot.cx    = cx;
    cmd->u.spr_rot.cy    = cy;
    ++gfx->draw_list.count;
}

void dtr_gfx_dl_spr_affine(dtr_graphics_t *gfx,
                           int32_t         layer,
                           int32_t         idx,
                           int32_t         x,
                           int32_t         y,
                           float           origin_x,
                           float           origin_y,
                           float           rot_x,
                           float           rot_y)
{
    dtr_draw_cmd_t *cmd;

    if (gfx->draw_list.count >= CONSOLE_MAX_DRAW_CMDS) {
        fprintf(stderr, "Draw list: max commands reached, dropping command\n");
        return;
    }
    cmd                   = &gfx->draw_list.cmds[gfx->draw_list.count];
    cmd->type             = DTR_DRAW_SPR_AFFINE;
    cmd->layer            = layer;
    cmd->order            = gfx->draw_list.count;
    cmd->u.spr_affine.idx = idx;
    cmd->u.spr_affine.x   = x;
    cmd->u.spr_affine.y   = y;
    cmd->u.spr_affine.ox  = origin_x;
    cmd->u.spr_affine.oy  = origin_y;
    cmd->u.spr_affine.rx  = rot_x;
    cmd->u.spr_affine.ry  = rot_y;
    ++gfx->draw_list.count;
}

void dtr_gfx_dl_end(dtr_graphics_t *gfx)
{
    int32_t cnt;

    gfx->draw_list.active = false;
    cnt                   = gfx->draw_list.count;
    if (cnt == 0) {
        return;
    }

    /* Stable counting sort by layer.
     * Items with the same layer keep their insertion order because the
     * counting sort scans forward. */
    {
        int32_t         min_l;
        int32_t         max_l;
        int32_t         range;
        dtr_draw_cmd_t *cmds;

        cmds  = gfx->draw_list.cmds;
        min_l = cmds[0].layer;
        max_l = cmds[0].layer;

        for (int32_t i = 1; i < cnt; ++i) {
            if (cmds[i].layer < min_l) {
                min_l = cmds[i].layer;
            }
            if (cmds[i].layer > max_l) {
                max_l = cmds[i].layer;
            }
        }

        range = max_l - min_l + 1;

        if (range == 1) {
            /* All on same layer — already in insertion order, no sort needed */
        } else if (range <= 512) {
            /* Small range: counting sort into a static scratch buffer */
            int32_t               counts[512];
            static dtr_draw_cmd_t scratch[CONSOLE_MAX_DRAW_CMDS];

            memset(counts, 0, (size_t)range * sizeof(int32_t));

            /* Count */
            for (int32_t i = 0; i < cnt; ++i) {
                ++counts[cmds[i].layer - min_l];
            }

            /* Prefix sum */
            for (int32_t i = 1; i < range; ++i) {
                counts[i] += counts[i - 1];
            }

            /* Place in reverse to maintain stability */
            for (int32_t i = cnt - 1; i >= 0; --i) {
                int32_t bucket;

                bucket                    = cmds[i].layer - min_l;
                scratch[--counts[bucket]] = cmds[i];
            }

            memcpy(cmds, scratch, (size_t)cnt * sizeof(dtr_draw_cmd_t));
        } else {
            /* Wide range: fallback to qsort */
            qsort(cmds, (size_t)cnt, sizeof(dtr_draw_cmd_t), prv_draw_cmd_cmp);
        }
    }

    /* Flush all commands */
    for (int32_t i = 0; i < gfx->draw_list.count; ++i) {
        dtr_draw_cmd_t *cmd;

        cmd = &gfx->draw_list.cmds[i];
        switch (cmd->type) {
            case DTR_DRAW_SPR:
                dtr_gfx_spr(gfx,
                            cmd->u.spr.idx,
                            cmd->u.spr.x,
                            cmd->u.spr.y,
                            cmd->u.spr.w,
                            cmd->u.spr.h,
                            cmd->u.spr.fx,
                            cmd->u.spr.fy);
                break;
            case DTR_DRAW_SSPR:
                dtr_gfx_sspr(gfx,
                             cmd->u.sspr.sx,
                             cmd->u.sspr.sy,
                             cmd->u.sspr.sw,
                             cmd->u.sspr.sh,
                             cmd->u.sspr.dx,
                             cmd->u.sspr.dy,
                             cmd->u.sspr.dw,
                             cmd->u.sspr.dh);
                break;
            case DTR_DRAW_SPR_ROT:
                dtr_gfx_spr_rot(gfx,
                                cmd->u.spr_rot.idx,
                                cmd->u.spr_rot.x,
                                cmd->u.spr_rot.y,
                                cmd->u.spr_rot.angle,
                                cmd->u.spr_rot.cx,
                                cmd->u.spr_rot.cy);
                break;
            case DTR_DRAW_SPR_AFFINE:
                dtr_gfx_spr_affine(gfx,
                                   cmd->u.spr_affine.idx,
                                   cmd->u.spr_affine.x,
                                   cmd->u.spr_affine.y,
                                   cmd->u.spr_affine.ox,
                                   cmd->u.spr_affine.oy,
                                   cmd->u.spr_affine.rx,
                                   cmd->u.spr_affine.ry);
                break;
        }
    }

    gfx->draw_list.count = 0;
}

/* ------------------------------------------------------------------ */
/*  Flip — palette framebuffer → RGBA                                  */
/* ------------------------------------------------------------------ */

void dtr_gfx_flip_to(dtr_graphics_t *gfx, uint32_t *dst)
{
    int32_t        total;
    int32_t        idx;
    const uint8_t *src;
    uint32_t       lut[CONSOLE_PALETTE_SIZE];

    for (idx = 0; idx < CONSOLE_PALETTE_SIZE; ++idx) {
        lut[idx] = gfx->colors[gfx->screen_pal[idx]];
    }

    total = gfx->width * gfx->height;
    src   = gfx->framebuffer;

#if DTR_HAS_SIMD
    {
        /* Process 4 pixels at a time via SIMD gather from the LUT.
         * Each lane independently looks up its palette index. */
        int32_t tail = total & ~3;
        for (idx = 0; idx < tail; idx += 4) {
            dtr_v4i indices = dtr_v4i_set((int32_t)src[idx],
                                          (int32_t)src[idx + 1],
                                          (int32_t)src[idx + 2],
                                          (int32_t)src[idx + 3]);
            dtr_v4i colors  = dtr_v4i_set((int32_t)lut[dtr_v4i_lane(indices, 0)],
                                          (int32_t)lut[dtr_v4i_lane(indices, 1)],
                                          (int32_t)lut[dtr_v4i_lane(indices, 2)],
                                          (int32_t)lut[dtr_v4i_lane(indices, 3)]);
            dtr_v4i_store_u32(&dst[idx], colors);
        }
        for (; idx < total; ++idx) {
            dst[idx] = lut[src[idx]];
        }
    }
#else
    {
        int32_t tail = total & ~3;
        for (idx = 0; idx < tail; idx += 4) {
            dst[idx]     = lut[src[idx]];
            dst[idx + 1] = lut[src[idx + 1]];
            dst[idx + 2] = lut[src[idx + 2]];
            dst[idx + 3] = lut[src[idx + 3]];
        }
        for (; idx < total; ++idx) {
            dst[idx] = lut[src[idx]];
        }
    }
#endif
}

void dtr_gfx_flip(dtr_graphics_t *gfx)
{
    dtr_gfx_flip_to(gfx, gfx->pixels);
}
