/**
 * \file            postfx.c
 * \brief           Software post-processing pipeline (CPU-based)
 */

#include "postfx.h"

#include <math.h>

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

mvn_postfx_t *mvn_postfx_create(int32_t width, int32_t height)
{
    mvn_postfx_t *pfx;

    pfx = MVN_CALLOC(1, sizeof(mvn_postfx_t));
    if (pfx != NULL) {
        pfx->width  = width;
        pfx->height = height;
    }
    return pfx;
}

void mvn_postfx_destroy(mvn_postfx_t *pfx)
{
    if (pfx != NULL) {
        MVN_FREE(pfx->scratch);
    }
    MVN_FREE(pfx);
}

/* ------------------------------------------------------------------ */
/*  Stack management                                                   */
/* ------------------------------------------------------------------ */

void mvn_postfx_push(mvn_postfx_t *pfx, mvn_postfx_id_t id, float strength)
{
    if (pfx->count >= MVN_POSTFX_MAX_STACK) {
        return;
    }

    pfx->stack[pfx->count].id       = id;
    pfx->stack[pfx->count].strength = strength;
    ++pfx->count;
}

void mvn_postfx_pop(mvn_postfx_t *pfx)
{
    if (pfx->count > 0) {
        --pfx->count;
    }
}

void mvn_postfx_clear(mvn_postfx_t *pfx)
{
    pfx->count = 0;
}

void mvn_postfx_set_param(mvn_postfx_t *pfx, int32_t index, int32_t param_idx, float value)
{
    if (index < 0 || index >= pfx->count) {
        return;
    }
    if (param_idx < 0 || param_idx >= MVN_POSTFX_MAX_PARAMS) {
        return;
    }
    pfx->stack[index].params[param_idx] = value;
}

/* ------------------------------------------------------------------ */
/*  Effect helpers (all work in-place on RGBA buffer)                   */
/* ------------------------------------------------------------------ */

/**
 * \brief           Apply scanline darkening (darken every other row)
 */
static void prv_apply_scanlines(uint32_t *pixels, int32_t w, int32_t h, float strength)
{
    float factor;

    factor = 1.0f - strength * 0.4f;

    for (int32_t y = 0; y < h; y += 2) {
        for (int32_t x = 0; x < w; ++x) {
            uint32_t px;
            uint32_t r;
            uint32_t g;
            uint32_t b;
            uint32_t a;

            px = pixels[y * w + x];
            r  = (px >> 0) & 0xFF;
            g  = (px >> 8) & 0xFF;
            b  = (px >> 16) & 0xFF;
            a  = (px >> 24) & 0xFF;

            r = (uint32_t)((float)r * factor);
            g = (uint32_t)((float)g * factor);
            b = (uint32_t)((float)b * factor);

            pixels[y * w + x] = r | (g << 8) | (b << 16) | (a << 24);
        }
    }
}

/**
 * \brief           Apply CRT curvature + slight color bleeding
 */
static void prv_apply_crt(uint32_t *pixels, int32_t w, int32_t h, float strength)
{
    /*
     * Simple CRT effect:
     * 1. Barrel distortion (simulated by darkening edges)
     * 2. Color bleeding (shift R channel slightly right)
     *
     * The vignette uses dist² from center.  Since the formula is
     * vignette = 1 - dist² * strength * 0.5, we never need sqrt —
     * dist² = (dx²+dy²) / max_dist² is computed directly.
     */

    float cx;
    float cy;
    float inv_max_dist_sq;
    float half_strength;

    cx               = (float)w * 0.5f;
    cy               = (float)h * 0.5f;
    inv_max_dist_sq  = 1.0f / (cx * cx + cy * cy);
    half_strength    = strength * 0.5f;

    for (int32_t y = 0; y < h; ++y) {
        float    dy;
        float    dy_sq;
        int32_t  row_off;

        dy      = (float)y - cy;
        dy_sq   = dy * dy;
        row_off = y * w;

        for (int32_t x = 0; x < w; ++x) {
            float    dx;
            float    dist_sq;
            float    vignette;
            uint32_t px;
            uint32_t r;
            uint32_t g;
            uint32_t b;
            uint32_t a;

            px = pixels[row_off + x];
            r  = (px >> 0) & 0xFF;
            g  = (px >> 8) & 0xFF;
            b  = (px >> 16) & 0xFF;
            a  = (px >> 24) & 0xFF;

            dx       = (float)x - cx;
            dist_sq  = (dx * dx + dy_sq) * inv_max_dist_sq;
            vignette = 1.0f - dist_sq * half_strength;

            if (vignette < 0.0f) {
                vignette = 0.0f;
            }

            r = (uint32_t)((float)r * vignette);
            g = (uint32_t)((float)g * vignette);
            b = (uint32_t)((float)b * vignette);

            pixels[row_off + x] = r | (g << 8) | (b << 16) | (a << 24);
        }
    }

    /* Color bleeding: shift R channel one pixel right */
    if (strength > 0.3f) {
        for (int32_t y = 0; y < h; ++y) {
            int32_t row_off;

            row_off = y * w;
            for (int32_t x = w - 1; x > 0; --x) {
                uint32_t cur;
                uint32_t prev;
                uint32_t bleed_r;

                cur     = pixels[row_off + x];
                prev    = pixels[row_off + x - 1];
                bleed_r = ((prev >> 0) & 0xFF);

                cur                    = (cur & 0xFFFFFF00u) | bleed_r;
                pixels[row_off + x]    = cur;
            }
        }
    }
}

/**
 * \brief           Simple bloom — brighten already bright pixels
 */
static void prv_apply_bloom(uint32_t *pixels, int32_t w, int32_t h, float strength)
{
    int32_t threshold;

    threshold = (int32_t)(200.0f - strength * 100.0f);
    if (threshold < 100) {
        threshold = 100;
    }

    for (int32_t y = 0; y < h; ++y) {
        for (int32_t x = 0; x < w; ++x) {
            uint32_t px;
            int32_t  r;
            int32_t  g;
            int32_t  b;
            uint32_t a;
            int32_t  lum;

            px = pixels[y * w + x];
            r  = (int32_t)((px >> 0) & 0xFF);
            g  = (int32_t)((px >> 8) & 0xFF);
            b  = (int32_t)((px >> 16) & 0xFF);
            a  = px & 0xFF000000u;

            lum = (r + g + b) / 3;
            if (lum > threshold) {
                float boost;

                boost = 1.0f + strength * 0.3f;
                r     = (int32_t)((float)r * boost);
                g     = (int32_t)((float)g * boost);
                b     = (int32_t)((float)b * boost);

                if (r > 255) {
                    r = 255;
                }
                if (g > 255) {
                    g = 255;
                }
                if (b > 255) {
                    b = 255;
                }
            }

            pixels[y * w + x] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | a;
        }
    }
}

/**
 * \brief           Chromatic aberration — offset R and B channels
 *
 * Reuses pfx->scratch instead of allocating every frame.
 */
static void prv_apply_aberration(mvn_postfx_t *pfx, uint32_t *pixels, int32_t w, int32_t h,
                                 float strength)
{
    int32_t   offset;
    uint32_t *temp;
    size_t    buf_sz;

    offset = (int32_t)(strength * 2.0f + 0.5f);
    if (offset < 1) {
        offset = 1;
    }

    /* Ensure scratch buffer is large enough */
    buf_sz = (size_t)w * (size_t)h * sizeof(uint32_t);
    if (pfx->scratch == NULL || pfx->width != w || pfx->height != h) {
        MVN_FREE(pfx->scratch);
        pfx->scratch = MVN_MALLOC(buf_sz);
        pfx->width   = w;
        pfx->height  = h;
        if (pfx->scratch == NULL) {
            return;
        }
    }

    temp = pfx->scratch;
    SDL_memcpy(temp, pixels, buf_sz);

    for (int32_t y = 0; y < h; ++y) {
        for (int32_t x = 0; x < w; ++x) {
            uint32_t center;
            uint32_t r_src;
            uint32_t b_src;
            uint32_t r;
            uint32_t g;
            uint32_t b;
            uint32_t a;
            int32_t  rx;
            int32_t  bx;

            center = temp[y * w + x];
            g      = (center >> 8) & 0xFF;
            a      = (center >> 24) & 0xFF;

            /* Shift R channel left */
            rx = x - offset;
            if (rx < 0) {
                rx = 0;
            }
            r_src = temp[y * w + rx];
            r     = (r_src >> 0) & 0xFF;

            /* Shift B channel right */
            bx = x + offset;
            if (bx >= w) {
                bx = w - 1;
            }
            b_src = temp[y * w + bx];
            b     = (b_src >> 16) & 0xFF;

            pixels[y * w + x] = r | (g << 8) | (b << 16) | (a << 24);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Apply full pipeline                                                */
/* ------------------------------------------------------------------ */

void mvn_postfx_apply(mvn_postfx_t *pfx, uint32_t *pixels, int32_t w, int32_t h)
{
    if (pfx == NULL || pfx->count == 0) {
        return;
    }

    for (int32_t idx = 0; idx < pfx->count; ++idx) {
        mvn_postfx_entry_t *entry;
        float               strength;

        entry    = &pfx->stack[idx];
        strength = entry->strength;

        switch (entry->id) {
            case MVN_POSTFX_SCANLINES:
                prv_apply_scanlines(pixels, w, h, strength);
                break;

            case MVN_POSTFX_CRT:
                prv_apply_crt(pixels, w, h, strength);
                break;

            case MVN_POSTFX_BLOOM:
                prv_apply_bloom(pixels, w, h, strength);
                break;

            case MVN_POSTFX_ABERRATION:
                prv_apply_aberration(pfx, pixels, w, h, strength);
                break;

            case MVN_POSTFX_NONE:
            default:
                break;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Name-based helpers                                                 */
/* ------------------------------------------------------------------ */

static const char *prv_effect_names[] = {"none", "crt", "scanlines", "bloom", "aberration"};

mvn_postfx_id_t mvn_postfx_id_from_name(const char *name)
{
    if (name == NULL) {
        return MVN_POSTFX_NONE;
    }
    for (int32_t idx = 0; idx < MVN_POSTFX_BUILTIN_COUNT; ++idx) {
        if (SDL_strcasecmp(name, prv_effect_names[idx]) == 0) {
            return (mvn_postfx_id_t)idx;
        }
    }
    return MVN_POSTFX_NONE;
}

const char **mvn_postfx_available(int32_t *out_count)
{
    if (out_count != NULL) {
        *out_count = MVN_POSTFX_BUILTIN_COUNT;
    }
    return prv_effect_names;
}

int32_t mvn_postfx_use(mvn_postfx_t *pfx, const char *name)
{
    mvn_postfx_id_t id;

    id                     = mvn_postfx_id_from_name(name);
    pfx->count             = 1;
    pfx->stack[0].id       = id;
    pfx->stack[0].strength = 1.0f;
    SDL_strlcpy(pfx->stack[0].name, name, MVN_POSTFX_NAME_LEN);
    return 0;
}
