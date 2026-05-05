/**
 * \file            graphics_state.c
 * \brief           Palette / camera / clip / fill-pattern / colour / cursor
 *                  state mutators.  Pure POD writes, no rasterisation.
 */

#include "graphics.h"

void dtr_gfx_pal(dtr_graphics_t *gfx, uint8_t src, uint8_t dst, bool screen)
{
    if (screen) {
        gfx->screen_pal[src] = dst;
    } else {
        gfx->draw_pal[src] = dst;
    }
}

void dtr_gfx_pal_reset(dtr_graphics_t *gfx)
{
    for (int32_t idx = 0; idx < CONSOLE_PALETTE_SIZE; ++idx) {
        gfx->draw_pal[idx]   = (uint8_t)idx;
        gfx->screen_pal[idx] = (uint8_t)idx;
    }
}

void dtr_gfx_palt(dtr_graphics_t *gfx, uint8_t col, bool trans)
{
    gfx->transparent[col] = trans;
}

void dtr_gfx_palt_reset(dtr_graphics_t *gfx)
{
    for (int32_t idx = 0; idx < CONSOLE_PALETTE_SIZE; ++idx) {
        gfx->transparent[idx] = (idx == 0);
    }
}

void dtr_gfx_camera(dtr_graphics_t *gfx, int32_t x, int32_t y)
{
    gfx->camera_x = x;
    gfx->camera_y = y;
}

void dtr_gfx_clip(dtr_graphics_t *gfx, int32_t x, int32_t y, int32_t w, int32_t h)
{
    /* Clamp to framebuffer bounds */
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (w < 0) {
        w = 0;
    }
    if (h < 0) {
        h = 0;
    }
    if (x + w > gfx->width) {
        w = gfx->width - x;
    }
    if (y + h > gfx->height) {
        h = gfx->height - y;
    }
    gfx->clip_x = x;
    gfx->clip_y = y;
    gfx->clip_w = w;
    gfx->clip_h = h;
}

void dtr_gfx_clip_reset(dtr_graphics_t *gfx)
{
    gfx->clip_x = 0;
    gfx->clip_y = 0;
    gfx->clip_w = gfx->width;
    gfx->clip_h = gfx->height;
}

void dtr_gfx_fillp(dtr_graphics_t *gfx, uint16_t pattern)
{
    gfx->fill_pattern = pattern;
}

void dtr_gfx_color(dtr_graphics_t *gfx, uint8_t col)
{
    gfx->color = col;
}

void dtr_gfx_cursor(dtr_graphics_t *gfx, int32_t x, int32_t y)
{
    gfx->cursor_x = x;
    gfx->cursor_y = y;
}
