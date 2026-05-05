/**
 * \file            graphics_sprite_meta.c
 * \brief           Sprite-flag accessors and direct sprite-sheet pixel I/O.
 */

#include "graphics.h"
#include "graphics_internal.h"

/* ------------------------------------------------------------------ */
/*  Sprite flags                                                       */
/* ------------------------------------------------------------------ */

uint8_t dtr_gfx_fget(dtr_graphics_t *gfx, int32_t idx)
{
    if (idx < 0 || idx >= CONSOLE_MAX_SPRITES) {
        return 0;
    }
    return gfx->sheet.flags[idx];
}

bool dtr_gfx_fget_bit(dtr_graphics_t *gfx, int32_t idx, int32_t flag)
{
    if (idx < 0 || idx >= CONSOLE_MAX_SPRITES || flag < 0 || flag >= CONSOLE_SPRITE_FLAGS) {
        return false;
    }
    return (gfx->sheet.flags[idx] & (1 << flag)) != 0;
}

void dtr_gfx_fset(dtr_graphics_t *gfx, int32_t idx, uint8_t mask)
{
    if (idx < 0 || idx >= CONSOLE_MAX_SPRITES) {
        return;
    }
    gfx->sheet.flags[idx] = mask;
}

void dtr_gfx_fset_bit(dtr_graphics_t *gfx, int32_t idx, int32_t flag, bool val)
{
    if (idx < 0 || idx >= CONSOLE_MAX_SPRITES || flag < 0 || flag >= CONSOLE_SPRITE_FLAGS) {
        return;
    }
    if (val) {
        gfx->sheet.flags[idx] |= (uint8_t)(1 << flag);
    } else {
        gfx->sheet.flags[idx] &= (uint8_t)~(1 << flag);
    }
}

bool dtr_gfx_load_flags_hex(dtr_graphics_t *gfx, const char *hex, size_t hex_len)
{
    if (hex == NULL) {
        return false;
    }

    /* Strip trailing whitespace */
    while (hex_len > 0 && (hex[hex_len - 1] == '\n' || hex[hex_len - 1] == '\r' ||
                           hex[hex_len - 1] == ' ' || hex[hex_len - 1] == '\t')) {
        --hex_len;
    }

    if (hex_len != (size_t)CONSOLE_MAX_SPRITES * 2) {
        return false;
    }

    for (int32_t idx = 0; idx < CONSOLE_MAX_SPRITES; ++idx) {
        int     hic;
        int     loc;
        uint8_t chi;
        uint8_t clo;

        chi = (uint8_t)hex[(size_t)idx * 2];
        clo = (uint8_t)hex[(size_t)idx * 2 + 1];

        if (!dtr_gfx_hex_nibble(chi, &hic) || !dtr_gfx_hex_nibble(clo, &loc)) {
            return false;
        }

        gfx->sheet.flags[idx] = (uint8_t)((hic << 4) | loc);
    }

    return true;
}

/* ------------------------------------------------------------------ */
/*  Spritesheet pixel access                                           */
/* ------------------------------------------------------------------ */

uint8_t dtr_gfx_sget(dtr_graphics_t *gfx, int32_t x, int32_t y)
{
    if (gfx->sheet.pixels == NULL) {
        return 0;
    }
    if (x < 0 || x >= gfx->sheet.width || y < 0 || y >= gfx->sheet.height) {
        return 0;
    }
    return gfx->sheet.pixels[y * gfx->sheet.width + x];
}

void dtr_gfx_sset(dtr_graphics_t *gfx, int32_t x, int32_t y, uint8_t col)
{
    if (gfx->sheet.pixels == NULL) {
        return;
    }
    if (x < 0 || x >= gfx->sheet.width || y < 0 || y >= gfx->sheet.height) {
        return;
    }
    gfx->sheet.pixels[y * gfx->sheet.width + x] = col;
}
