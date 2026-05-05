/**
 * \file            graphics_sheet.c
 * \brief           Sprite-sheet loading: RGBA quantisation, hex-string load,
 *                  blank sheet allocation.
 */

#include "graphics.h"
#include "graphics_internal.h"

#include <string.h>

bool dtr_gfx_load_sheet(dtr_graphics_t *gfx,
                        const uint8_t  *rgba,
                        int32_t         width,
                        int32_t         height,
                        int32_t         tile_w,
                        int32_t         tile_h)
{
    dtr_sprite_sheet_t *sht;
    int32_t             total;
    size_t              alloc_sz;
    uint8_t            *new_pixels;

    sht = &gfx->sheet;

    total      = width * height;
    alloc_sz   = (size_t)width * (size_t)height;
    new_pixels = DTR_MALLOC(alloc_sz);
    if (new_pixels == NULL) {
        return false;
    }

    if (sht->pixels != NULL) {
        DTR_FREE(sht->pixels);
    }
    sht->pixels = new_pixels;

    /* Quantise RGBA to nearest palette colour.
     * Build a 15-bit (5 bits per channel) colour-cube LUT for O(1) lookup.
     * Most sprite sheets use exact palette colours, so the LUT hits ~99%
     * of the time, avoiding the inner 256-entry brute-force loop.
     * The LUT is cached in gfx->pal_lut and reused across sheet loads. */
    {
        uint8_t *lut;
        int32_t  lut_size = 32 * 32 * 32;

        if (gfx->pal_lut == NULL) {
            gfx->pal_lut = DTR_MALLOC((size_t)lut_size);
        }
        lut = gfx->pal_lut;
        if (lut != NULL) {
            /* Populate LUT: for each 15-bit colour, find nearest palette entry */
            for (int32_t ri = 0; ri < 32; ++ri) {
                for (int32_t gi = 0; gi < 32; ++gi) {
                    for (int32_t bi = 0; bi < 32; ++bi) {
                        int32_t lr    = ri * 255 / 31;
                        int32_t lg    = gi * 255 / 31;
                        int32_t lb    = bi * 255 / 31;
                        int32_t lbest = 0;
                        int32_t ldist = 0x7FFFFFFF;
                        int32_t lidx  = (ri << 10) | (gi << 5) | bi;

                        for (int32_t pal = 0; pal < CONSOLE_PALETTE_SIZE; ++pal) {
                            int32_t pr  = (int32_t)((gfx->colors[pal] >> 24) & 0xFF);
                            int32_t pg  = (int32_t)((gfx->colors[pal] >> 16) & 0xFF);
                            int32_t pb  = (int32_t)((gfx->colors[pal] >> 8) & 0xFF);
                            int32_t ddr = lr - pr;
                            int32_t ddg = lg - pg;
                            int32_t ddb = lb - pb;
                            int32_t dd  = ddr * ddr + ddg * ddg + ddb * ddb;

                            if (dd < ldist) {
                                ldist = dd;
                                lbest = pal;
                                if (dd == 0) {
                                    break;
                                }
                            }
                        }
                        lut[lidx] = (uint8_t)lbest;
                    }
                }
            }

            /* Quantise using LUT */
            for (int32_t idx = 0; idx < total; ++idx) {
                uint8_t src_a;
                int32_t key;

                src_a = rgba[idx * 4 + 3];
                if (src_a < 128) {
                    sht->pixels[idx] = 0;
                    continue;
                }
                key = ((int32_t)(rgba[idx * 4 + 0] >> 3) << 10) |
                      ((int32_t)(rgba[idx * 4 + 1] >> 3) << 5) | (int32_t)(rgba[idx * 4 + 2] >> 3);
                sht->pixels[idx] = lut[key];
            }
        } else {
            /* Fallback: brute-force per-pixel quantisation */
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
        }
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

bool dtr_gfx_load_sheet_hex(dtr_graphics_t *gfx,
                            const char     *hex,
                            size_t          hex_len,
                            int32_t         width,
                            int32_t         height,
                            int32_t         tile_w,
                            int32_t         tile_h)
{
    dtr_sprite_sheet_t *sht;
    int32_t             total;
    size_t              alloc_sz;
    uint8_t            *new_pixels;

    if (hex == NULL || width <= 0 || height <= 0 || tile_w <= 0 || tile_h <= 0) {
        return false;
    }

    total    = width * height;
    alloc_sz = (size_t)width * (size_t)height;

    /* Strip trailing whitespace (newlines, spaces) that text editors may add */
    while (hex_len > 0 && (hex[hex_len - 1] == '\n' || hex[hex_len - 1] == '\r' ||
                           hex[hex_len - 1] == ' ' || hex[hex_len - 1] == '\t')) {
        --hex_len;
    }

    /* Hex string must have exactly 2 chars per pixel */
    if (hex_len != alloc_sz * 2) {
        return false;
    }

    new_pixels = DTR_MALLOC(alloc_sz);
    if (new_pixels == NULL) {
        return false;
    }

    /* Decode hex pairs directly to palette indices */
    for (int32_t idx = 0; idx < total; ++idx) {
        int     hic;
        int     loc;
        uint8_t chi;
        uint8_t clo;

        chi = (uint8_t)hex[(size_t)idx * 2];
        clo = (uint8_t)hex[(size_t)idx * 2 + 1];

        if (!dtr_gfx_hex_nibble(chi, &hic) || !dtr_gfx_hex_nibble(clo, &loc)) {
            DTR_FREE(new_pixels);
            return false;
        }

        new_pixels[idx] = (uint8_t)((hic << 4) | loc);
    }

    sht = &gfx->sheet;

    if (sht->pixels != NULL) {
        DTR_FREE(sht->pixels);
    }

    sht->pixels = new_pixels;
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

bool dtr_gfx_create_sheet(dtr_graphics_t *gfx,
                          int32_t         width,
                          int32_t         height,
                          int32_t         tile_w,
                          int32_t         tile_h)
{
    dtr_sprite_sheet_t *sht;
    size_t              alloc_sz;
    uint8_t            *new_pixels;

    if (width <= 0 || height <= 0 || tile_w <= 0 || tile_h <= 0) {
        return false;
    }

    sht      = &gfx->sheet;
    alloc_sz = (size_t)width * (size_t)height;

    new_pixels = DTR_CALLOC(1, alloc_sz);
    if (new_pixels == NULL) {
        return false;
    }

    if (sht->pixels != NULL) {
        DTR_FREE(sht->pixels);
    }

    sht->pixels = new_pixels;
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

    memset(sht->flags, 0, sizeof(sht->flags));

    return true;
}
