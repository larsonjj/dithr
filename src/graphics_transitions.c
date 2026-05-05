/**
 * \file            graphics_transitions.c
 * \brief           Screen transitions: fade, wipe, dissolve.
 */

#include "graphics.h"

#include <string.h>

void dtr_gfx_fade(dtr_graphics_t *gfx, uint8_t color, int32_t frames)
{
    if (frames < 1) {
        frames = 1;
    }
    gfx->transition.type     = DTR_TRANS_FADE;
    gfx->transition.color    = color;
    gfx->transition.duration = frames;
    gfx->transition.frame    = 0;
}

/**
 * \brief           Simple pseudo-random hash for dissolve pixel selection
 */
static uint32_t prv_hash_pixel(int32_t x, int32_t y, int32_t seed)
{
    uint32_t h;

    h = (uint32_t)(x * 374761393 + y * 668265263 + seed * 2147483647);
    h = (h ^ (h >> 13)) * 1274126177;
    h = h ^ (h >> 16);
    return h;
}

void dtr_gfx_wipe(dtr_graphics_t *gfx, int32_t direction, uint8_t color, int32_t frames)
{
    if (frames < 1) {
        frames = 1;
    }
    gfx->transition.type      = DTR_TRANS_WIPE;
    gfx->transition.direction = (dtr_wipe_dir_t)direction;
    gfx->transition.color     = color;
    gfx->transition.duration  = frames;
    gfx->transition.frame     = 0;
}

void dtr_gfx_dissolve(dtr_graphics_t *gfx, uint8_t color, int32_t frames)
{
    int32_t   total;
    uint32_t *order;

    if (frames < 1) {
        frames = 1;
    }

    /* Free any previous dissolve table */
    if (gfx->transition.dissolve_order != NULL) {
        DTR_FREE(gfx->transition.dissolve_order);
        gfx->transition.dissolve_order = NULL;
    }

    total = gfx->width * gfx->height;
    order = (uint32_t *)DTR_MALLOC((size_t)total * sizeof(uint32_t));
    if (order != NULL) {
        /* Fill with pixel offsets then sort by hash so the random
         * dissolve pattern is computed once instead of every frame. */
        for (int32_t i = 0; i < total; ++i) {
            order[i] = (uint32_t)i;
        }

        /* Fisher-Yates shuffle seeded by the per-pixel hash gives a
         * uniform random order with O(n) work and no comparator
         * overhead from qsort. */
        for (int32_t i = total - 1; i > 0; --i) {
            uint32_t h_val;
            uint32_t j_idx;
            uint32_t tmp;

            h_val        = prv_hash_pixel(i % gfx->width, i / gfx->width, 42);
            j_idx        = h_val % ((uint32_t)i + 1);
            tmp          = order[i];
            order[i]     = order[j_idx];
            order[j_idx] = tmp;
        }
    }

    gfx->transition.type           = DTR_TRANS_DISSOLVE;
    gfx->transition.color          = color;
    gfx->transition.duration       = frames;
    gfx->transition.frame          = 0;
    gfx->transition.dissolve_order = order;
    gfx->transition.dissolve_count = total;
    gfx->transition.dissolve_done  = 0;
}

bool dtr_gfx_transitioning(dtr_graphics_t *gfx)
{
    return gfx->transition.type != DTR_TRANS_NONE;
}

void dtr_gfx_transition_update_buf(dtr_graphics_t *gfx, uint32_t *pixels)
{
    dtr_transition_t *tr;
    float             t;

    tr = &gfx->transition;
    if (tr->type == DTR_TRANS_NONE) {
        return;
    }

    ++tr->frame;
    t = (float)tr->frame / (float)tr->duration;
    if (t > 1.0f) {
        t = 1.0f;
    }

    switch (tr->type) {
        case DTR_TRANS_FADE: {
            /* Blend already-flipped RGBA pixels towards target colour */
            uint32_t target_rgba;
            int32_t  total;

            target_rgba = gfx->colors[tr->color];

            total = gfx->width * gfx->height;

            {
                /* Fixed-point 8-bit lerp: result = (src*(256-t) + tgt*t) >> 8
                 * Processes even/odd byte channels in parallel.  The
                 * weighted-sum formulation avoids unsigned underflow that
                 * would occur with (tgt - src) when tgt < src. */
                uint32_t t256;
                uint32_t inv;
                uint32_t tgt_rb; /* target odd bytes (R, B) */
                uint32_t tgt_ga; /* target even bytes (G, A) */

                t256 = (uint32_t)(t * 256.0f);
                if (t256 > 256) {
                    t256 = 256;
                }
                inv    = 256 - t256;
                tgt_rb = (target_rgba >> 8) & 0x00FF00FFu;
                tgt_ga = target_rgba & 0x00FF00FFu;

                for (int32_t idx = 0; idx < total; ++idx) {
                    uint32_t src_px;
                    uint32_t s_rb;
                    uint32_t s_ga;
                    uint32_t r_rb;
                    uint32_t r_ga;

                    src_px = pixels[idx];

                    /* High bytes of each 16-bit pair: R (bit 31-24), B (bit 15-8) */
                    s_rb = (src_px >> 8) & 0x00FF00FFu;
                    r_rb = (s_rb * inv + tgt_rb * t256) & 0xFF00FF00u;

                    /* Low bytes of each 16-bit pair: G (bit 23-16), A (bit 7-0) */
                    s_ga = src_px & 0x00FF00FFu;
                    r_ga = ((s_ga * inv + tgt_ga * t256) >> 8) & 0x00FF00FFu;

                    pixels[idx] = r_rb | r_ga;
                }
            }
            break;
        }

        case DTR_TRANS_WIPE: {
            /* Progressive fill from one edge over flipped pixels */
            int32_t  limit;
            uint32_t col_rgba;

            col_rgba = gfx->colors[tr->color];

            switch (tr->direction) {
                case DTR_WIPE_LEFT:
                    limit = (int32_t)((float)gfx->width * t);
                    for (int32_t y = 0; y < gfx->height; ++y) {
                        for (int32_t x = 0; x < limit; ++x) {
                            pixels[y * gfx->width + x] = col_rgba;
                        }
                    }
                    break;
                case DTR_WIPE_RIGHT:
                    limit = gfx->width - (int32_t)((float)gfx->width * t);
                    for (int32_t y = 0; y < gfx->height; ++y) {
                        for (int32_t x = limit; x < gfx->width; ++x) {
                            pixels[y * gfx->width + x] = col_rgba;
                        }
                    }
                    break;
                case DTR_WIPE_UP:
                    limit = (int32_t)((float)gfx->height * t);
                    for (int32_t y = 0; y < limit; ++y) {
                        for (int32_t x = 0; x < gfx->width; ++x) {
                            pixels[y * gfx->width + x] = col_rgba;
                        }
                    }
                    break;
                case DTR_WIPE_DOWN:
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

        case DTR_TRANS_DISSOLVE: {
            /* Incremental dissolve using pre-sorted permutation table.
             * Each frame only writes the newly-revealed pixels. */
            uint32_t col_rgba;
            int32_t  target;

            col_rgba = gfx->colors[tr->color];
            target   = (int32_t)((float)tr->dissolve_count * t);
            if (target > tr->dissolve_count) {
                target = tr->dissolve_count;
            }

            if (tr->dissolve_order != NULL) {
                for (int32_t i = tr->dissolve_done; i < target; ++i) {
                    pixels[tr->dissolve_order[i]] = col_rgba;
                }
                tr->dissolve_done = target;
            } else {
                /* Fallback: no table (malloc failed) — use hash */
                uint32_t threshold;

                threshold = (uint32_t)(t * 4294967295.0f);
                for (int32_t y_px = 0; y_px < gfx->height; ++y_px) {
                    for (int32_t x_px = 0; x_px < gfx->width; ++x_px) {
                        if (prv_hash_pixel(x_px, y_px, 42) <= threshold) {
                            pixels[y_px * gfx->width + x_px] = col_rgba;
                        }
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
        tr->type = DTR_TRANS_NONE;
        if (tr->dissolve_order != NULL) {
            DTR_FREE(tr->dissolve_order);
            tr->dissolve_order = NULL;
        }
    }
}

void dtr_gfx_transition_update(dtr_graphics_t *gfx)
{
    dtr_gfx_transition_update_buf(gfx, gfx->pixels);
}
