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
        for (int32_t px = min_x; px <= max_x; ++px) {
            prv_put_pixel(gfx, px, row, col);
        }
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
    for (int32_t dy_val = -r; dy_val <= r; ++dy_val) {
        int32_t half_w;

        half_w = (int32_t)sqrtf((float)(r * r - dy_val * dy_val));
        for (int32_t dx_val = -half_w; dx_val <= half_w; ++dx_val) {
            prv_put_pixel(gfx, x + dx_val, y + dy_val, col);
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

        for (int32_t px = left; px <= right; ++px) {
            prv_put_pixel(gfx, px, row, col);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Text                                                               */
/* ------------------------------------------------------------------ */

int32_t mvn_gfx_print(mvn_graphics_t *gfx, const char *str, int32_t x, int32_t y, uint8_t col)
{
    int32_t     ox;
    const char *ptr;

    ox = x;

    for (ptr = str; *ptr != '\0'; ++ptr) {
        uint8_t chr;

        chr = (uint8_t)*ptr;
        if (chr == '\n') {
            x = ox;
            y += MVN_FONT_H;
            continue;
        }
        if (chr < MVN_FONT_FIRST || chr > MVN_FONT_LAST) {
            x += MVN_FONT_W + 1;
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

        x += MVN_FONT_W + 1;
    }

    /* Update cursor */
    gfx->cursor_x = x;
    gfx->cursor_y = y;

    return x - ox;
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
/*  Flip — palette framebuffer → RGBA                                  */
/* ------------------------------------------------------------------ */

void mvn_gfx_flip(mvn_graphics_t *gfx)
{
    int32_t total;

    total = gfx->width * gfx->height;
    for (int32_t idx = 0; idx < total; ++idx) {
        uint8_t pal_idx;

        pal_idx          = gfx->framebuffer[idx];
        pal_idx          = gfx->screen_pal[pal_idx];
        gfx->pixels[idx] = gfx->colors[pal_idx];
    }
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
