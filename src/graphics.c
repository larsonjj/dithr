/**
 * \file            graphics.c
 * \brief           Framebuffer, palette, primitives, sprites, text
 */

#include "graphics.h"

#include "font.h"

#include <math.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Default 256-colour palette (first 16 inspired by PICO-8)           */
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

mvn_graphics_t *mvn_gfx_create(int32_t width, int32_t height)
{
    mvn_graphics_t *gfx;

    gfx = MVN_CALLOC(1, sizeof(mvn_graphics_t));
    if (gfx == NULL) {
        return NULL;
    }

    gfx->width  = width;
    gfx->height = height;

    mvn_gfx_init_default_palette(gfx);
    mvn_gfx_reset(gfx);

    return gfx;
}

void mvn_gfx_destroy(mvn_graphics_t *gfx)
{
    if (gfx == NULL) {
        return;
    }
    if (gfx->sheet.pixels != NULL) {
        MVN_FREE(gfx->sheet.pixels);
    }
    MVN_FREE(gfx);
}

void mvn_gfx_reset(mvn_graphics_t *gfx)
{
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

void mvn_gfx_init_default_palette(mvn_graphics_t *gfx)
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

static void prv_put_pixel(mvn_graphics_t *gfx, int32_t raw_x, int32_t raw_y, uint8_t col)
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

/* ------------------------------------------------------------------ */
/*  Internal: fast horizontal span (pre-clipped scanline)              */
/* ------------------------------------------------------------------ */

/**
 * \brief           Draw a horizontal span from (raw_x0..raw_x1) at raw_y.
 *
 * Camera transform, Y-clipping, and X-clipping are done once per span
 * rather than per pixel.  When the fill pattern is zero (solid fill)
 * the span is written with SDL_memset for maximum throughput.
 */
static void
prv_hline(mvn_graphics_t *gfx, int32_t raw_x0, int32_t raw_x1, int32_t raw_y, uint8_t col)
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

    /* Remap colour once */
    col = gfx->draw_pal[col];

    if (gfx->fill_pattern == 0) {
        /* Solid fill — memset the span */
        SDL_memset(&gfx->framebuffer[scr_y * gfx->width + left], col,
                   (size_t)(right - left + 1));
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
/*  Core drawing                                                       */
/* ------------------------------------------------------------------ */

void mvn_gfx_cls(mvn_graphics_t *gfx, uint8_t col)
{
    SDL_memset(gfx->framebuffer, col, (size_t)(gfx->width * gfx->height));
}

void mvn_gfx_pset(mvn_graphics_t *gfx, int32_t x, int32_t y, uint8_t col)
{
    prv_put_pixel(gfx, x, y, col);
}

uint8_t mvn_gfx_pget(mvn_graphics_t *gfx, int32_t x, int32_t y)
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

void mvn_gfx_line(mvn_graphics_t *gfx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t col)
{
    int32_t dx_val;
    int32_t dy_val;
    int32_t sx_val;
    int32_t sy_val;
    int32_t err;

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

void mvn_gfx_rect(mvn_graphics_t *gfx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t col)
{
    mvn_gfx_line(gfx, x0, y0, x1, y0, col);
    mvn_gfx_line(gfx, x1, y0, x1, y1, col);
    mvn_gfx_line(gfx, x1, y1, x0, y1, col);
    mvn_gfx_line(gfx, x0, y1, x0, y0, col);
}

void mvn_gfx_rectfill(mvn_graphics_t *gfx,
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

void mvn_gfx_circ(mvn_graphics_t *gfx, int32_t x, int32_t y, int32_t r, uint8_t col)
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

void mvn_gfx_circfill(mvn_graphics_t *gfx, int32_t x, int32_t y, int32_t r, uint8_t col)
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

void mvn_gfx_tri(mvn_graphics_t *gfx,
                 int32_t         x0,
                 int32_t         y0,
                 int32_t         x1,
                 int32_t         y1,
                 int32_t         x2,
                 int32_t         y2,
                 uint8_t         col)
{
    mvn_gfx_line(gfx, x0, y0, x1, y1, col);
    mvn_gfx_line(gfx, x1, y1, x2, y2, col);
    mvn_gfx_line(gfx, x2, y2, x0, y0, col);
}

void mvn_gfx_trifill(mvn_graphics_t *gfx,
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

void mvn_gfx_poly(mvn_graphics_t *gfx, const int32_t *pts, int32_t count, uint8_t col)
{
    if (count < 2) {
        return;
    }
    for (int32_t idx = 0; idx < count; ++idx) {
        int32_t next;

        next = (idx + 1) % count;
        mvn_gfx_line(gfx,
                     pts[idx * 2], pts[idx * 2 + 1],
                     pts[next * 2], pts[next * 2 + 1],
                     col);
    }
}

void mvn_gfx_polyfill(mvn_graphics_t *gfx, const int32_t *pts, int32_t count, uint8_t col)
{
    if (count < 3) {
        if (count == 2) {
            mvn_gfx_line(gfx, pts[0], pts[1], pts[2], pts[3], col);
        }
        return;
    }

    /* Fan-of-triangles from vertex 0 */
    for (int32_t idx = 1; idx < count - 1; ++idx) {
        mvn_gfx_trifill(gfx,
                        pts[0], pts[1],
                        pts[idx * 2], pts[idx * 2 + 1],
                        pts[(idx + 1) * 2], pts[(idx + 1) * 2 + 1],
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
prv_print_custom_char(mvn_graphics_t *gfx, uint8_t chr, int32_t x, int32_t y, uint8_t col)
{
    mvn_custom_font_t *  font;
    mvn_sprite_sheet_t * sht;
    int32_t              glyph_idx;
    int32_t              gx;
    int32_t              gy;

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

    for (int32_t row = 0; row < font->char_h; ++row) {
        for (int32_t px = 0; px < font->char_w; ++px) {
            int32_t src_x;
            int32_t src_y;
            uint8_t src_col;

            src_x = gx + px;
            src_y = gy + row;
            if (src_x < 0 || src_x >= sht->width || src_y < 0 || src_y >= sht->height) {
                continue;
            }

            src_col = sht->pixels[src_y * sht->width + src_x];
            if (gfx->transparent[src_col]) {
                continue;
            }
            prv_put_pixel(gfx, x + px, y + row, col);
        }
    }
}

int32_t mvn_gfx_print(mvn_graphics_t *gfx, const char *str, int32_t x, int32_t y, uint8_t col)
{
    int32_t     ox;
    const char *ptr;
    int32_t     cw;
    int32_t     ch;

    ox = x;
    cw = gfx->custom_font.active ? gfx->custom_font.char_w : MVN_FONT_W;
    ch = gfx->custom_font.active ? gfx->custom_font.char_h : MVN_FONT_H;

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
            if (chr < MVN_FONT_FIRST || chr > MVN_FONT_LAST) {
                x += cw + 1;
                continue;
            }

            {
                const unsigned char *glyph;
                int32_t              glyph_idx;

                glyph_idx = chr - MVN_FONT_FIRST;
                glyph     = MVN_FONT_DATA[glyph_idx];

                for (int32_t row = 0; row < MVN_FONT_H; ++row) {
                    for (int32_t bit = 0; bit < MVN_FONT_W; ++bit) {
                        if (glyph[row] & (0x8 >> bit)) {
                            prv_put_pixel(gfx, x + bit, y + row, col);
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

void mvn_gfx_font(mvn_graphics_t *gfx,
                  int32_t         sx,
                  int32_t         sy,
                  int32_t         char_w,
                  int32_t         char_h,
                  char            first,
                  int32_t         count)
{
    mvn_custom_font_t *font;

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

void mvn_gfx_font_reset(mvn_graphics_t *gfx)
{
    gfx->custom_font.active = false;
}

/* ------------------------------------------------------------------ */
/*  Sprites                                                            */
/* ------------------------------------------------------------------ */

void mvn_gfx_spr(mvn_graphics_t *gfx,
                 int32_t         idx,
                 int32_t         x,
                 int32_t         y,
                 int32_t         w_tiles,
                 int32_t         h_tiles,
                 bool            flip_x,
                 bool            flip_y)
{
    mvn_sprite_sheet_t *sht;
    int32_t             sx0;
    int32_t             sy0;
    int32_t             tile_w;
    int32_t             tile_h;
    int32_t             px_w;
    int32_t             px_h;

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

    for (int32_t row = 0; row < px_h; ++row) {
        for (int32_t px = 0; px < px_w; ++px) {
            int32_t src_x;
            int32_t src_y;
            uint8_t col;

            src_x = flip_x ? (px_w - 1 - px) : px;
            src_y = flip_y ? (px_h - 1 - row) : row;
            src_x += sx0;
            src_y += sy0;

            if (src_x < 0 || src_x >= sht->width || src_y < 0 || src_y >= sht->height) {
                continue;
            }

            col = sht->pixels[src_y * sht->width + src_x];
            if (gfx->transparent[col]) {
                continue;
            }
            prv_put_pixel(gfx, x + px, y + row, col);
        }
    }
}

void mvn_gfx_sspr(mvn_graphics_t *gfx,
                  int32_t         sx,
                  int32_t         sy,
                  int32_t         sw,
                  int32_t         sh,
                  int32_t         dx,
                  int32_t         dy,
                  int32_t         dw,
                  int32_t         dh)
{
    mvn_sprite_sheet_t *sht;

    sht = &gfx->sheet;
    if (sht->pixels == NULL || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
        return;
    }

    for (int32_t row = 0; row < dh; ++row) {
        for (int32_t px = 0; px < dw; ++px) {
            int32_t src_x;
            int32_t src_y;
            uint8_t col;

            src_x = sx + px * sw / dw;
            src_y = sy + row * sh / dh;

            if (src_x < 0 || src_x >= sht->width || src_y < 0 || src_y >= sht->height) {
                continue;
            }

            col = sht->pixels[src_y * sht->width + src_x];
            if (gfx->transparent[col]) {
                continue;
            }
            prv_put_pixel(gfx, dx + px, dy + row, col);
        }
    }
}

void mvn_gfx_spr_rot(mvn_graphics_t *gfx,
                     int32_t         idx,
                     int32_t         x,
                     int32_t         y,
                     float           angle,
                     int32_t         cx,
                     int32_t         cy)
{
    mvn_sprite_sheet_t *sht;
    int32_t             sx0;
    int32_t             sy0;
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
    sx0    = (idx % sht->cols) * tile_w;
    sy0    = (idx / sht->cols) * tile_h;
    cos_a  = cosf(angle);
    sin_a  = sinf(angle);

    /* Scan bounding box of rotated sprite */
    {
        int32_t extent;
        int32_t max_dim;

        max_dim = (tile_w > tile_h) ? tile_w : tile_h;
        extent  = (int32_t)(max_dim * 1.42f) + 1;

        for (int32_t dy_val = -extent; dy_val <= extent; ++dy_val) {
            for (int32_t dx_val = -extent; dx_val <= extent; ++dx_val) {
                int32_t src_x;
                int32_t src_y;
                uint8_t col;
                float   fx;
                float   fy;

                /* Inverse rotate */
                fx = cos_a * (float)dx_val + sin_a * (float)dy_val + (float)cx;
                fy = -sin_a * (float)dx_val + cos_a * (float)dy_val + (float)cy;

                src_x = (int32_t)fx;
                src_y = (int32_t)fy;

                if (src_x < 0 || src_x >= tile_w || src_y < 0 || src_y >= tile_h) {
                    continue;
                }

                col = sht->pixels[(sy0 + src_y) * sht->width + (sx0 + src_x)];
                if (gfx->transparent[col]) {
                    continue;
                }
                prv_put_pixel(gfx, x + dx_val, y + dy_val, col);
            }
        }
    }
}

void mvn_gfx_spr_affine(mvn_graphics_t *gfx,
                        int32_t         idx,
                        int32_t         x,
                        int32_t         y,
                        float           origin_x,
                        float           origin_y,
                        float           rot_x,
                        float           rot_y)
{
    /* Simplified affine: rot_x/rot_y define the 2D rotation basis */
    mvn_sprite_sheet_t *sht;
    int32_t             sx0;
    int32_t             sy0;
    int32_t             tile_w;
    int32_t             tile_h;

    sht = &gfx->sheet;
    if (sht->pixels == NULL || idx < 0 || idx >= sht->count) {
        return;
    }

    tile_w = sht->tile_w;
    tile_h = sht->tile_h;
    sx0    = (idx % sht->cols) * tile_w;
    sy0    = (idx / sht->cols) * tile_h;

    {
        int32_t extent;
        int32_t max_dim;

        max_dim = (tile_w > tile_h) ? tile_w : tile_h;
        extent  = max_dim * 2;

        for (int32_t dy_val = -extent; dy_val <= extent; ++dy_val) {
            for (int32_t dx_val = -extent; dx_val <= extent; ++dx_val) {
                int32_t src_x;
                int32_t src_y;
                uint8_t col;
                float   fx;
                float   fy;
                float   det;

                /* Inverse affine transform */
                det = rot_x * rot_x + rot_y * rot_y;
                if (det < 0.001f) {
                    continue;
                }
                fx = (rot_x * (float)dx_val + rot_y * (float)dy_val) / det + origin_x;
                fy = (-rot_y * (float)dx_val + rot_x * (float)dy_val) / det + origin_y;

                src_x = (int32_t)fx;
                src_y = (int32_t)fy;

                if (src_x < 0 || src_x >= tile_w || src_y < 0 || src_y >= tile_h) {
                    continue;
                }

                col = sht->pixels[(sy0 + src_y) * sht->width + (sx0 + src_x)];
                if (gfx->transparent[col]) {
                    continue;
                }
                prv_put_pixel(gfx, x + dx_val, y + dy_val, col);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Sprite flags                                                       */
/* ------------------------------------------------------------------ */

uint8_t mvn_gfx_fget(mvn_graphics_t *gfx, int32_t idx)
{
    if (idx < 0 || idx >= CONSOLE_MAX_SPRITES) {
        return 0;
    }
    return gfx->sheet.flags[idx];
}

bool mvn_gfx_fget_bit(mvn_graphics_t *gfx, int32_t idx, int32_t flag)
{
    if (idx < 0 || idx >= CONSOLE_MAX_SPRITES || flag < 0 || flag >= CONSOLE_SPRITE_FLAGS) {
        return false;
    }
    return (gfx->sheet.flags[idx] & (1 << flag)) != 0;
}

void mvn_gfx_fset(mvn_graphics_t *gfx, int32_t idx, uint8_t mask)
{
    if (idx < 0 || idx >= CONSOLE_MAX_SPRITES) {
        return;
    }
    gfx->sheet.flags[idx] = mask;
}

void mvn_gfx_fset_bit(mvn_graphics_t *gfx, int32_t idx, int32_t flag, bool val)
{
    if (idx < 0 || idx >= CONSOLE_MAX_SPRITES || flag < 0 || flag >= CONSOLE_SPRITE_FLAGS) {
        return;
    }
    if (val) {
        gfx->sheet.flags[idx] |= (uint8_t)(1 << flag);
    } else {
        gfx->sheet.flags[idx] &= (uint8_t) ~(1 << flag);
    }
}

/* ------------------------------------------------------------------ */
/*  Palette                                                            */
/* ------------------------------------------------------------------ */

void mvn_gfx_pal(mvn_graphics_t *gfx, uint8_t src, uint8_t dst, bool screen)
{
    if (screen) {
        gfx->screen_pal[src] = dst;
    } else {
        gfx->draw_pal[src] = dst;
    }
}

void mvn_gfx_pal_reset(mvn_graphics_t *gfx)
{
    for (int32_t idx = 0; idx < CONSOLE_PALETTE_SIZE; ++idx) {
        gfx->draw_pal[idx]   = (uint8_t)idx;
        gfx->screen_pal[idx] = (uint8_t)idx;
    }
}

void mvn_gfx_palt(mvn_graphics_t *gfx, uint8_t col, bool trans)
{
    gfx->transparent[col] = trans;
}

void mvn_gfx_palt_reset(mvn_graphics_t *gfx)
{
    for (int32_t idx = 0; idx < CONSOLE_PALETTE_SIZE; ++idx) {
        gfx->transparent[idx] = (idx == 0);
    }
}

/* ------------------------------------------------------------------ */
/*  State                                                              */
/* ------------------------------------------------------------------ */

void mvn_gfx_camera(mvn_graphics_t *gfx, int32_t x, int32_t y)
{
    gfx->camera_x = x;
    gfx->camera_y = y;
}

void mvn_gfx_clip(mvn_graphics_t *gfx, int32_t x, int32_t y, int32_t w, int32_t h)
{
    gfx->clip_x = x;
    gfx->clip_y = y;
    gfx->clip_w = w;
    gfx->clip_h = h;
}

void mvn_gfx_clip_reset(mvn_graphics_t *gfx)
{
    gfx->clip_x = 0;
    gfx->clip_y = 0;
    gfx->clip_w = gfx->width;
    gfx->clip_h = gfx->height;
}

void mvn_gfx_fillp(mvn_graphics_t *gfx, uint16_t pattern)
{
    gfx->fill_pattern = pattern;
}

void mvn_gfx_color(mvn_graphics_t *gfx, uint8_t col)
{
    gfx->color = col;
}

void mvn_gfx_cursor(mvn_graphics_t *gfx, int32_t x, int32_t y)
{
    gfx->cursor_x = x;
    gfx->cursor_y = y;
}

/* ------------------------------------------------------------------ */
/*  Screen transitions                                                 */
/* ------------------------------------------------------------------ */

void mvn_gfx_fade(mvn_graphics_t *gfx, uint8_t color, int32_t frames)
{
    if (frames < 1) {
        frames = 1;
    }
    gfx->transition.type     = MVN_TRANS_FADE;
    gfx->transition.color    = color;
    gfx->transition.duration = frames;
    gfx->transition.frame    = 0;
}

void mvn_gfx_wipe(mvn_graphics_t *gfx, int32_t direction, uint8_t color, int32_t frames)
{
    if (frames < 1) {
        frames = 1;
    }
    gfx->transition.type      = MVN_TRANS_WIPE;
    gfx->transition.direction = (mvn_wipe_dir_t)direction;
    gfx->transition.color     = color;
    gfx->transition.duration  = frames;
    gfx->transition.frame     = 0;
}

void mvn_gfx_dissolve(mvn_graphics_t *gfx, uint8_t color, int32_t frames)
{
    if (frames < 1) {
        frames = 1;
    }
    gfx->transition.type     = MVN_TRANS_DISSOLVE;
    gfx->transition.color    = color;
    gfx->transition.duration = frames;
    gfx->transition.frame    = 0;
}

bool mvn_gfx_transitioning(mvn_graphics_t *gfx)
{
    return gfx->transition.type != MVN_TRANS_NONE;
}

/**
 * \\brief  Simple pseudo-random hash for dissolve pixel selection
 */
static uint32_t prv_hash_pixel(int32_t x, int32_t y, int32_t seed)
{
    uint32_t h;

    h = (uint32_t)(x * 374761393 + y * 668265263 + seed * 2147483647);
    h = (h ^ (h >> 13)) * 1274126177;
    h = h ^ (h >> 16);
    return h;
}

void mvn_gfx_transition_update_buf(mvn_graphics_t *gfx, uint32_t *pixels)
{
    mvn_transition_t *tr;
    float             t;

    tr = &gfx->transition;
    if (tr->type == MVN_TRANS_NONE) {
        return;
    }

    ++tr->frame;
    t = (float)tr->frame / (float)tr->duration;
    if (t > 1.0f) {
        t = 1.0f;
    }

    switch (tr->type) {
        case MVN_TRANS_FADE: {
            /* Blend already-flipped RGBA pixels towards target colour */
            uint32_t target_rgba;
            int32_t  tr_val;
            int32_t  tg_val;
            int32_t  tb_val;
            int32_t  total;

            target_rgba = gfx->colors[tr->color];
            tr_val      = (int32_t)((target_rgba >> 24) & 0xFF);
            tg_val      = (int32_t)((target_rgba >> 16) & 0xFF);
            tb_val      = (int32_t)((target_rgba >> 8) & 0xFF);

            total = gfx->width * gfx->height;
            for (int32_t idx = 0; idx < total; ++idx) {
                uint32_t src;
                int32_t  sr;
                int32_t  sg;
                int32_t  sb;
                int32_t  rr;
                int32_t  rg;
                int32_t  rb;

                src = pixels[idx];
                sr  = (int32_t)((src >> 24) & 0xFF);
                sg  = (int32_t)((src >> 16) & 0xFF);
                sb  = (int32_t)((src >> 8) & 0xFF);

                rr = sr + (int32_t)((float)(tr_val - sr) * t);
                rg = sg + (int32_t)((float)(tg_val - sg) * t);
                rb = sb + (int32_t)((float)(tb_val - sb) * t);

                pixels[idx] =
                    ((uint32_t)rr << 24) | ((uint32_t)rg << 16) | ((uint32_t)rb << 8) | 0xFF;
            }
            break;
        }

        case MVN_TRANS_WIPE: {
            /* Progressive fill from one edge over flipped pixels */
            int32_t  limit;
            uint32_t col_rgba;

            col_rgba = gfx->colors[tr->color];

            switch (tr->direction) {
                case MVN_WIPE_LEFT:
                    limit = (int32_t)((float)gfx->width * t);
                    for (int32_t y = 0; y < gfx->height; ++y) {
                        for (int32_t x = 0; x < limit; ++x) {
                            pixels[y * gfx->width + x] = col_rgba;
                        }
                    }
                    break;
                case MVN_WIPE_RIGHT:
                    limit = gfx->width - (int32_t)((float)gfx->width * t);
                    for (int32_t y = 0; y < gfx->height; ++y) {
                        for (int32_t x = limit; x < gfx->width; ++x) {
                            pixels[y * gfx->width + x] = col_rgba;
                        }
                    }
                    break;
                case MVN_WIPE_UP:
                    limit = (int32_t)((float)gfx->height * t);
                    for (int32_t y = 0; y < limit; ++y) {
                        for (int32_t x = 0; x < gfx->width; ++x) {
                            pixels[y * gfx->width + x] = col_rgba;
                        }
                    }
                    break;
                case MVN_WIPE_DOWN:
                    limit = gfx->height - (int32_t)((float)gfx->height * t);
                    for (int32_t y = limit; y < gfx->height; ++y) {
                        for (int32_t x = 0; x < gfx->width; ++x) {
                            pixels[y * gfx->width + x] = col_rgba;
                        }
                    }
                    break;
            }
            break;
        }

        case MVN_TRANS_DISSOLVE: {
            /* Random pixel replacement over flipped pixels */
            uint32_t col_rgba;
            uint32_t threshold;

            col_rgba  = gfx->colors[tr->color];
            threshold = (uint32_t)(t * 4294967295.0f);

            for (int32_t y = 0; y < gfx->height; ++y) {
                for (int32_t x = 0; x < gfx->width; ++x) {
                    if (prv_hash_pixel(x, y, 42) <= threshold) {
                        pixels[y * gfx->width + x] = col_rgba;
                    }
                }
            }
            break;
        }

        default:
            break;
    }

    /* Complete when done */
    if (tr->frame >= tr->duration) {
        tr->type = MVN_TRANS_NONE;
    }
}

void mvn_gfx_transition_update(mvn_graphics_t *gfx)
{
    mvn_gfx_transition_update_buf(gfx, gfx->pixels);
}

/* ------------------------------------------------------------------ */
/*  Draw list (sprite batch)                                            */
/* ------------------------------------------------------------------ */

void mvn_gfx_dl_begin(mvn_graphics_t *gfx)
{
    gfx->draw_list.count  = 0;
    gfx->draw_list.active = true;
}

static int prv_draw_cmd_cmp(const void *a, const void *b)
{
    const mvn_draw_cmd_t *ca;
    const mvn_draw_cmd_t *cb;

    ca = (const mvn_draw_cmd_t *)a;
    cb = (const mvn_draw_cmd_t *)b;
    if (ca->layer != cb->layer) {
        return (ca->layer < cb->layer) ? -1 : 1;
    }
    return (ca->order < cb->order) ? -1 : 1;
}

void mvn_gfx_dl_spr(mvn_graphics_t *gfx,
                    int32_t         layer,
                    int32_t         idx,
                    int32_t         x,
                    int32_t         y,
                    int32_t         w,
                    int32_t         h,
                    bool            flip_x,
                    bool            flip_y)
{
    mvn_draw_cmd_t *cmd;

    if (gfx->draw_list.count >= CONSOLE_MAX_DRAW_CMDS) {
        return;
    }
    cmd        = &gfx->draw_list.cmds[gfx->draw_list.count];
    cmd->type  = MVN_DRAW_SPR;
    cmd->layer = layer;
    cmd->order = gfx->draw_list.count;
    cmd->u.spr.idx = idx;
    cmd->u.spr.x   = x;
    cmd->u.spr.y   = y;
    cmd->u.spr.w   = w;
    cmd->u.spr.h   = h;
    cmd->u.spr.fx  = flip_x;
    cmd->u.spr.fy  = flip_y;
    ++gfx->draw_list.count;
}

void mvn_gfx_dl_sspr(mvn_graphics_t *gfx,
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
    mvn_draw_cmd_t *cmd;

    if (gfx->draw_list.count >= CONSOLE_MAX_DRAW_CMDS) {
        return;
    }
    cmd         = &gfx->draw_list.cmds[gfx->draw_list.count];
    cmd->type   = MVN_DRAW_SSPR;
    cmd->layer  = layer;
    cmd->order  = gfx->draw_list.count;
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

void mvn_gfx_dl_spr_rot(mvn_graphics_t *gfx,
                        int32_t         layer,
                        int32_t         idx,
                        int32_t         x,
                        int32_t         y,
                        float           angle,
                        int32_t         cx,
                        int32_t         cy)
{
    mvn_draw_cmd_t *cmd;

    if (gfx->draw_list.count >= CONSOLE_MAX_DRAW_CMDS) {
        return;
    }
    cmd             = &gfx->draw_list.cmds[gfx->draw_list.count];
    cmd->type       = MVN_DRAW_SPR_ROT;
    cmd->layer      = layer;
    cmd->order      = gfx->draw_list.count;
    cmd->u.spr_rot.idx   = idx;
    cmd->u.spr_rot.x     = x;
    cmd->u.spr_rot.y     = y;
    cmd->u.spr_rot.angle = angle;
    cmd->u.spr_rot.cx    = cx;
    cmd->u.spr_rot.cy    = cy;
    ++gfx->draw_list.count;
}

void mvn_gfx_dl_spr_affine(mvn_graphics_t *gfx,
                           int32_t         layer,
                           int32_t         idx,
                           int32_t         x,
                           int32_t         y,
                           float           origin_x,
                           float           origin_y,
                           float           rot_x,
                           float           rot_y)
{
    mvn_draw_cmd_t *cmd;

    if (gfx->draw_list.count >= CONSOLE_MAX_DRAW_CMDS) {
        return;
    }
    cmd                = &gfx->draw_list.cmds[gfx->draw_list.count];
    cmd->type          = MVN_DRAW_SPR_AFFINE;
    cmd->layer         = layer;
    cmd->order         = gfx->draw_list.count;
    cmd->u.spr_affine.idx = idx;
    cmd->u.spr_affine.x   = x;
    cmd->u.spr_affine.y   = y;
    cmd->u.spr_affine.ox  = origin_x;
    cmd->u.spr_affine.oy  = origin_y;
    cmd->u.spr_affine.rx  = rot_x;
    cmd->u.spr_affine.ry  = rot_y;
    ++gfx->draw_list.count;
}

void mvn_gfx_dl_end(mvn_graphics_t *gfx)
{
    gfx->draw_list.active = false;
    if (gfx->draw_list.count == 0) {
        return;
    }

    /* Sort by layer then insertion order */
    qsort(gfx->draw_list.cmds,
          (size_t)gfx->draw_list.count,
          sizeof(mvn_draw_cmd_t),
          prv_draw_cmd_cmp);

    /* Flush all commands */
    for (int32_t i = 0; i < gfx->draw_list.count; ++i) {
        mvn_draw_cmd_t *cmd;

        cmd = &gfx->draw_list.cmds[i];
        switch (cmd->type) {
            case MVN_DRAW_SPR:
                mvn_gfx_spr(gfx,
                            cmd->u.spr.idx,
                            cmd->u.spr.x,
                            cmd->u.spr.y,
                            cmd->u.spr.w,
                            cmd->u.spr.h,
                            cmd->u.spr.fx,
                            cmd->u.spr.fy);
                break;
            case MVN_DRAW_SSPR:
                mvn_gfx_sspr(gfx,
                             cmd->u.sspr.sx,
                             cmd->u.sspr.sy,
                             cmd->u.sspr.sw,
                             cmd->u.sspr.sh,
                             cmd->u.sspr.dx,
                             cmd->u.sspr.dy,
                             cmd->u.sspr.dw,
                             cmd->u.sspr.dh);
                break;
            case MVN_DRAW_SPR_ROT:
                mvn_gfx_spr_rot(gfx,
                                cmd->u.spr_rot.idx,
                                cmd->u.spr_rot.x,
                                cmd->u.spr_rot.y,
                                cmd->u.spr_rot.angle,
                                cmd->u.spr_rot.cx,
                                cmd->u.spr_rot.cy);
                break;
            case MVN_DRAW_SPR_AFFINE:
                mvn_gfx_spr_affine(gfx,
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

void mvn_gfx_flip_to(mvn_graphics_t *gfx, uint32_t *dst)
{
    int32_t total;

    total = gfx->width * gfx->height;
    for (int32_t idx = 0; idx < total; ++idx) {
        uint8_t pal_idx;

        pal_idx  = gfx->framebuffer[idx];
        pal_idx  = gfx->screen_pal[pal_idx];
        dst[idx] = gfx->colors[pal_idx];
    }
}

void mvn_gfx_flip(mvn_graphics_t *gfx)
{
    mvn_gfx_flip_to(gfx, gfx->pixels);
}

/* ------------------------------------------------------------------ */
/*  Sprite sheet loading                                               */
/* ------------------------------------------------------------------ */

bool mvn_gfx_load_sheet(mvn_graphics_t *gfx,
                        const uint8_t * rgba,
                        int32_t         width,
                        int32_t         height,
                        int32_t         tile_w,
                        int32_t         tile_h)
{
    mvn_sprite_sheet_t *sht;
    int32_t             total;
    size_t              alloc_sz;

    sht = &gfx->sheet;

    if (sht->pixels != NULL) {
        MVN_FREE(sht->pixels);
        sht->pixels = NULL;
    }

    total       = width * height;
    alloc_sz    = (size_t)total;
    sht->pixels = MVN_MALLOC(alloc_sz);
    if (sht->pixels == NULL) {
        return false;
    }

    /* Quantise RGBA to nearest palette colour */
    for (int32_t idx = 0; idx < total; ++idx) {
        uint8_t src_r;
        uint8_t src_g;
        uint8_t src_b;
        uint8_t src_a;
        int32_t best;
        int32_t best_dist;

        src_r = rgba[idx * 4 + 0];
        src_g = rgba[idx * 4 + 1];
        src_b = rgba[idx * 4 + 2];
        src_a = rgba[idx * 4 + 3];

        /* Fully transparent → palette 0 */
        if (src_a < 128) {
            sht->pixels[idx] = 0;
            continue;
        }

        best      = 0;
        best_dist = 0x7FFFFFFF;

        for (int32_t pal = 0; pal < CONSOLE_PALETTE_SIZE; ++pal) {
            int32_t pal_r;
            int32_t pal_g;
            int32_t pal_b;
            int32_t dr_val;
            int32_t dg_val;
            int32_t db_val;
            int32_t dist;

            pal_r = (int32_t)((gfx->colors[pal] >> 24) & 0xFF);
            pal_g = (int32_t)((gfx->colors[pal] >> 16) & 0xFF);
            pal_b = (int32_t)((gfx->colors[pal] >> 8) & 0xFF);

            dr_val = (int32_t)src_r - pal_r;
            dg_val = (int32_t)src_g - pal_g;
            db_val = (int32_t)src_b - pal_b;
            dist   = dr_val * dr_val + dg_val * dg_val + db_val * db_val;

            if (dist < best_dist) {
                best_dist = dist;
                best      = pal;
                if (dist == 0) {
                    break;
                }
            }
        }

        sht->pixels[idx] = (uint8_t)best;
    }

    sht->width  = width;
    sht->height = height;
    sht->tile_w = tile_w;
    sht->tile_h = tile_h;
    sht->cols   = width / tile_w;
    sht->rows   = height / tile_h;
    sht->count  = sht->cols * sht->rows;

    if (sht->count > CONSOLE_MAX_SPRITES) {
        sht->count = CONSOLE_MAX_SPRITES;
    }

    return true;
}
