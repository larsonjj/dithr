/**
 * \file            gfx_api.c
 * \brief           JS gfx namespace — drawing, palette, sprites
 */

#include "../graphics.h"
#include "api_common.h"

#define GFX(ctx) (dtr_api_get_console(ctx)->graphics)

/**
 * \brief  Resolve an optional colour argument: -1 → current draw colour
 */
static uint8_t prv_resolve_col(JSContext *ctx, int argc, JSValueConst *argv, int idx)
{
    int32_t raw = dtr_api_opt_int(ctx, argc, argv, idx, -1);
    return (raw < 0) ? GFX(ctx)->color : (uint8_t)raw;
}

/* ---- Drawing ---------------------------------------------------------- */

static JSValue js_gfx_cls(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_cls(GFX(ctx), (uint8_t)dtr_api_opt_int(ctx, argc, argv, 0, 0));
    return JS_UNDEFINED;
}

static JSValue js_gfx_pset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_pset(GFX(ctx),
                 dtr_api_opt_int(ctx, argc, argv, 0, 0),
                 dtr_api_opt_int(ctx, argc, argv, 1, 0),
                 prv_resolve_col(ctx, argc, argv, 2));
    return JS_UNDEFINED;
}

static JSValue js_gfx_pget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewInt32(ctx,
                       dtr_gfx_pget(GFX(ctx),
                                    dtr_api_opt_int(ctx, argc, argv, 0, 0),
                                    dtr_api_opt_int(ctx, argc, argv, 1, 0)));
}

static JSValue js_gfx_line(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_line(GFX(ctx),
                 dtr_api_opt_int(ctx, argc, argv, 0, 0),
                 dtr_api_opt_int(ctx, argc, argv, 1, 0),
                 dtr_api_opt_int(ctx, argc, argv, 2, 0),
                 dtr_api_opt_int(ctx, argc, argv, 3, 0),
                 prv_resolve_col(ctx, argc, argv, 4));
    return JS_UNDEFINED;
}

static JSValue js_gfx_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_rect(GFX(ctx),
                 dtr_api_opt_int(ctx, argc, argv, 0, 0),
                 dtr_api_opt_int(ctx, argc, argv, 1, 0),
                 dtr_api_opt_int(ctx, argc, argv, 2, 0),
                 dtr_api_opt_int(ctx, argc, argv, 3, 0),
                 prv_resolve_col(ctx, argc, argv, 4));
    return JS_UNDEFINED;
}

static JSValue js_gfx_rectfill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_rectfill(GFX(ctx),
                     dtr_api_opt_int(ctx, argc, argv, 0, 0),
                     dtr_api_opt_int(ctx, argc, argv, 1, 0),
                     dtr_api_opt_int(ctx, argc, argv, 2, 0),
                     dtr_api_opt_int(ctx, argc, argv, 3, 0),
                     prv_resolve_col(ctx, argc, argv, 4));
    return JS_UNDEFINED;
}

/* gfx.tilemap(tiles, mapW, mapH, colors [, tileW, tileH])
 *   tiles  — flat JS array of tile indices (uint8)
 *   mapW   — map width in tiles
 *   mapH   — map height in tiles
 *   colors — JS array mapping tile index → palette colour
 *   tileW  — tile pixel width  (default 8)
 *   tileH  — tile pixel height (default 8)
 */
static JSValue js_gfx_tilemap(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t  map_w;
    int32_t  map_h;
    int32_t  tile_w;
    int32_t  tile_h;
    int32_t  tile_cnt;
    int32_t  col_cnt;
    uint8_t *tiles;
    uint8_t *colors;
    JSValue  len_val;

    (void)this_val;
    if (argc < 4 || !JS_IsArray(argv[0]) || !JS_IsArray(argv[3])) {
        return JS_UNDEFINED;
    }

    map_w  = dtr_api_opt_int(ctx, argc, argv, 1, 0);
    map_h  = dtr_api_opt_int(ctx, argc, argv, 2, 0);
    tile_w = dtr_api_opt_int(ctx, argc, argv, 4, 8);
    tile_h = dtr_api_opt_int(ctx, argc, argv, 5, 8);

    if (map_w <= 0 || map_h <= 0 || tile_w <= 0 || tile_h <= 0) {
        return JS_UNDEFINED;
    }
    if (map_w > INT32_MAX / map_h) {
        return JS_UNDEFINED;
    }

    /* Read tiles array into temp buffer */
    len_val = JS_GetPropertyStr(ctx, argv[0], "length");
    JS_ToInt32(ctx, &tile_cnt, len_val);
    JS_FreeValue(ctx, len_val);

    if (tile_cnt < map_w * map_h) {
        return JS_UNDEFINED;
    }

    tiles = (uint8_t *)SDL_malloc((size_t)map_w * (size_t)map_h);
    if (tiles == NULL) {
        return JS_UNDEFINED;
    }

    for (int32_t idx = 0; idx < map_w * map_h; ++idx) {
        JSValue elem;
        int32_t val;

        elem = JS_GetPropertyUint32(ctx, argv[0], (uint32_t)idx);
        JS_ToInt32(ctx, &val, elem);
        JS_FreeValue(ctx, elem);
        tiles[idx] = (uint8_t)val;
    }

    /* Read colors array */
    len_val = JS_GetPropertyStr(ctx, argv[3], "length");
    JS_ToInt32(ctx, &col_cnt, len_val);
    JS_FreeValue(ctx, len_val);

    if (col_cnt > 256) {
        col_cnt = 256;
    }

    colors = (uint8_t *)SDL_malloc((size_t)col_cnt);
    if (colors == NULL) {
        SDL_free(tiles);
        return JS_UNDEFINED;
    }

    for (int32_t idx = 0; idx < col_cnt; ++idx) {
        JSValue elem;
        int32_t val;

        elem = JS_GetPropertyUint32(ctx, argv[3], (uint32_t)idx);
        JS_ToInt32(ctx, &val, elem);
        JS_FreeValue(ctx, elem);
        colors[idx] = (uint8_t)val;
    }

    dtr_gfx_tilemap(GFX(ctx), tiles, map_w, map_h, tile_w, tile_h, colors, col_cnt);

    SDL_free(tiles);
    SDL_free(colors);
    return JS_UNDEFINED;
}

static JSValue js_gfx_circ(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_circ(GFX(ctx),
                 dtr_api_opt_int(ctx, argc, argv, 0, 0),
                 dtr_api_opt_int(ctx, argc, argv, 1, 0),
                 dtr_api_opt_int(ctx, argc, argv, 2, 4),
                 prv_resolve_col(ctx, argc, argv, 3));
    return JS_UNDEFINED;
}

static JSValue js_gfx_circfill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_circfill(GFX(ctx),
                     dtr_api_opt_int(ctx, argc, argv, 0, 0),
                     dtr_api_opt_int(ctx, argc, argv, 1, 0),
                     dtr_api_opt_int(ctx, argc, argv, 2, 4),
                     prv_resolve_col(ctx, argc, argv, 3));
    return JS_UNDEFINED;
}

static JSValue js_gfx_tri(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_tri(GFX(ctx),
                dtr_api_opt_int(ctx, argc, argv, 0, 0),
                dtr_api_opt_int(ctx, argc, argv, 1, 0),
                dtr_api_opt_int(ctx, argc, argv, 2, 0),
                dtr_api_opt_int(ctx, argc, argv, 3, 0),
                dtr_api_opt_int(ctx, argc, argv, 4, 0),
                dtr_api_opt_int(ctx, argc, argv, 5, 0),
                prv_resolve_col(ctx, argc, argv, 6));
    return JS_UNDEFINED;
}

static JSValue js_gfx_trifill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_trifill(GFX(ctx),
                    dtr_api_opt_int(ctx, argc, argv, 0, 0),
                    dtr_api_opt_int(ctx, argc, argv, 1, 0),
                    dtr_api_opt_int(ctx, argc, argv, 2, 0),
                    dtr_api_opt_int(ctx, argc, argv, 3, 0),
                    dtr_api_opt_int(ctx, argc, argv, 4, 0),
                    dtr_api_opt_int(ctx, argc, argv, 5, 0),
                    prv_resolve_col(ctx, argc, argv, 6));
    return JS_UNDEFINED;
}

/* Max vertices for polygon calls (stack-allocated) */
#define PRV_MAX_POLY_PTS 64

/**
 * \\brief  Read a JS array of numbers into a stack-allocated int32 array
 * \\return Number of elements read, or 0 on failure
 */
static int32_t prv_read_int_array(JSContext *ctx, JSValueConst arr, int32_t *out, int32_t max)
{
    int32_t len;
    JSValue len_val;

    if (!JS_IsArray(arr)) {
        return 0;
    }

    len_val = JS_GetPropertyStr(ctx, arr, "length");
    JS_ToInt32(ctx, &len, len_val);
    JS_FreeValue(ctx, len_val);

    if (len > max) {
        len = max;
    }

    for (int32_t idx = 0; idx < len; ++idx) {
        JSValue elem;

        elem = JS_GetPropertyUint32(ctx, arr, (uint32_t)idx);
        JS_ToInt32(ctx, &out[idx], elem);
        JS_FreeValue(ctx, elem);
    }
    return len;
}

static JSValue js_gfx_poly(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t pts[PRV_MAX_POLY_PTS * 2];
    int32_t n;
    int32_t vert_count;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    n          = prv_read_int_array(ctx, argv[0], pts, PRV_MAX_POLY_PTS * 2);
    vert_count = n / 2;

    if (vert_count >= 2) {
        dtr_gfx_poly(GFX(ctx), pts, vert_count, prv_resolve_col(ctx, argc, argv, 1));
    }
    return JS_UNDEFINED;
}

static JSValue js_gfx_polyfill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t pts[PRV_MAX_POLY_PTS * 2];
    int32_t n;
    int32_t vert_count;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    n          = prv_read_int_array(ctx, argv[0], pts, PRV_MAX_POLY_PTS * 2);
    vert_count = n / 2;

    if (vert_count >= 3) {
        dtr_gfx_polyfill(GFX(ctx), pts, vert_count, prv_resolve_col(ctx, argc, argv, 1));
    }
    return JS_UNDEFINED;
}

/* ---- Text ------------------------------------------------------------- */

static JSValue js_gfx_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *text;
    int32_t     x;
    int32_t     y;
    uint8_t     col;

    (void)this_val;

    if (argc < 1) {
        return JS_UNDEFINED;
    }

    text = JS_ToCString(ctx, argv[0]);
    if (text == NULL) {
        return JS_UNDEFINED;
    }

    x   = dtr_api_opt_int(ctx, argc, argv, 1, -1);
    y   = dtr_api_opt_int(ctx, argc, argv, 2, -1);
    col = prv_resolve_col(ctx, argc, argv, 3);

    if (x == -1 && y == -1) {
        /* Use cursor position */
        dtr_gfx_print(GFX(ctx), text, GFX(ctx)->cursor_x, GFX(ctx)->cursor_y, col);
    } else {
        dtr_gfx_print(GFX(ctx), text, x, y, col);
    }

    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static JSValue
js_gfx_text_width(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *text;
    int32_t     w;

    (void)this_val;
    if (argc < 1) {
        return JS_NewInt32(ctx, 0);
    }
    text = JS_ToCString(ctx, argv[0]);
    if (text == NULL) {
        return JS_NewInt32(ctx, 0);
    }
    w = dtr_gfx_text_width(GFX(ctx), text);
    JS_FreeCString(ctx, text);
    return JS_NewInt32(ctx, w);
}

static JSValue
js_gfx_text_height(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *text;
    int32_t     h;

    (void)this_val;
    if (argc < 1) {
        return JS_NewInt32(ctx, 0);
    }
    text = JS_ToCString(ctx, argv[0]);
    if (text == NULL) {
        return JS_NewInt32(ctx, 0);
    }
    h = dtr_gfx_text_height(GFX(ctx), text);
    JS_FreeCString(ctx, text);
    return JS_NewInt32(ctx, h);
}

/* ---- Sprites ---------------------------------------------------------- */

static JSValue js_gfx_spr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_spr(GFX(ctx),
                dtr_api_opt_int(ctx, argc, argv, 0, 0),
                dtr_api_opt_int(ctx, argc, argv, 1, 0),
                dtr_api_opt_int(ctx, argc, argv, 2, 0),
                dtr_api_opt_int(ctx, argc, argv, 3, 1),
                dtr_api_opt_int(ctx, argc, argv, 4, 1),
                dtr_api_opt_int(ctx, argc, argv, 5, 0) != 0,
                dtr_api_opt_int(ctx, argc, argv, 6, 0) != 0);
    return JS_UNDEFINED;
}

static JSValue js_gfx_sspr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_sspr(GFX(ctx),
                 dtr_api_opt_int(ctx, argc, argv, 0, 0),
                 dtr_api_opt_int(ctx, argc, argv, 1, 0),
                 dtr_api_opt_int(ctx, argc, argv, 2, 8),
                 dtr_api_opt_int(ctx, argc, argv, 3, 8),
                 dtr_api_opt_int(ctx, argc, argv, 4, 0),
                 dtr_api_opt_int(ctx, argc, argv, 5, 0),
                 dtr_api_opt_int(ctx, argc, argv, 6, 8),
                 dtr_api_opt_int(ctx, argc, argv, 7, 8));
    return JS_UNDEFINED;
}

static JSValue js_gfx_spr_rot(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_spr_rot(GFX(ctx),
                    dtr_api_opt_int(ctx, argc, argv, 0, 0),
                    dtr_api_opt_int(ctx, argc, argv, 1, 0),
                    dtr_api_opt_int(ctx, argc, argv, 2, 0),
                    (float)dtr_api_opt_float(ctx, argc, argv, 3, 0.0),
                    dtr_api_opt_int(ctx, argc, argv, 4, -1),
                    dtr_api_opt_int(ctx, argc, argv, 5, -1));
    return JS_UNDEFINED;
}

static JSValue
js_gfx_spr_affine(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_spr_affine(GFX(ctx),
                       dtr_api_opt_int(ctx, argc, argv, 0, 0),
                       dtr_api_opt_int(ctx, argc, argv, 1, 0),
                       dtr_api_opt_int(ctx, argc, argv, 2, 0),
                       (float)dtr_api_opt_float(ctx, argc, argv, 3, 0.0),
                       (float)dtr_api_opt_float(ctx, argc, argv, 4, 0.0),
                       (float)dtr_api_opt_float(ctx, argc, argv, 5, 1.0),
                       (float)dtr_api_opt_float(ctx, argc, argv, 6, 0.0));
    return JS_UNDEFINED;
}

/* ---- Sprite flags ----------------------------------------------------- */

static JSValue js_gfx_fget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t n;
    int32_t bit;

    (void)this_val;
    n   = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    bit = dtr_api_opt_int(ctx, argc, argv, 1, -1);

    if (bit < 0) {
        return JS_NewInt32(ctx, dtr_gfx_fget(GFX(ctx), n));
    }
    return JS_NewBool(ctx, dtr_gfx_fget_bit(GFX(ctx), n, bit) != 0);
}

static JSValue js_gfx_fset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_fset_bit(GFX(ctx),
                     dtr_api_opt_int(ctx, argc, argv, 0, 0),
                     dtr_api_opt_int(ctx, argc, argv, 1, 0),
                     dtr_api_opt_int(ctx, argc, argv, 2, 1) != 0);
    return JS_UNDEFINED;
}

/* ---- Palette ---------------------------------------------------------- */

static JSValue js_gfx_pal(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    if (argc == 0) {
        dtr_gfx_pal_reset(GFX(ctx));
    } else {
        dtr_gfx_pal(GFX(ctx),
                    (uint8_t)dtr_api_opt_int(ctx, argc, argv, 0, 0),
                    (uint8_t)dtr_api_opt_int(ctx, argc, argv, 1, 0),
                    dtr_api_opt_int(ctx, argc, argv, 2, 0));
    }
    return JS_UNDEFINED;
}

static JSValue js_gfx_palt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    if (argc == 0) {
        dtr_gfx_palt_reset(GFX(ctx));
    } else {
        dtr_gfx_palt(GFX(ctx),
                     (uint8_t)dtr_api_opt_int(ctx, argc, argv, 0, 0),
                     dtr_api_opt_int(ctx, argc, argv, 1, 1) != 0);
    }
    return JS_UNDEFINED;
}

/* ---- State ------------------------------------------------------------ */

static JSValue js_gfx_camera(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_camera(
        GFX(ctx), dtr_api_opt_int(ctx, argc, argv, 0, 0), dtr_api_opt_int(ctx, argc, argv, 1, 0));
    return JS_UNDEFINED;
}

static JSValue js_gfx_clip(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    if (argc == 0) {
        dtr_gfx_clip(GFX(ctx), 0, 0, GFX(ctx)->width, GFX(ctx)->height);
    } else {
        dtr_gfx_clip(GFX(ctx),
                     dtr_api_opt_int(ctx, argc, argv, 0, 0),
                     dtr_api_opt_int(ctx, argc, argv, 1, 0),
                     dtr_api_opt_int(ctx, argc, argv, 2, 0),
                     dtr_api_opt_int(ctx, argc, argv, 3, 0));
    }
    return JS_UNDEFINED;
}

static JSValue js_gfx_fillp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_fillp(GFX(ctx), (uint16_t)dtr_api_opt_int(ctx, argc, argv, 0, 0));
    return JS_UNDEFINED;
}

static JSValue js_gfx_color(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_color(GFX(ctx), (uint8_t)dtr_api_opt_int(ctx, argc, argv, 0, 7));
    return JS_UNDEFINED;
}

static JSValue js_gfx_cursor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_cursor(
        GFX(ctx), dtr_api_opt_int(ctx, argc, argv, 0, 0), dtr_api_opt_int(ctx, argc, argv, 1, 0));
    return JS_UNDEFINED;
}

static JSValue js_gfx_font(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    if (argc == 0) {
        dtr_gfx_font_reset(GFX(ctx));
    } else {
        int32_t first_char;

        first_char = dtr_api_opt_int(ctx, argc, argv, 4, 32);
        dtr_gfx_font(GFX(ctx),
                     dtr_api_opt_int(ctx, argc, argv, 0, 0),
                     dtr_api_opt_int(ctx, argc, argv, 1, 0),
                     dtr_api_opt_int(ctx, argc, argv, 2, 8),
                     dtr_api_opt_int(ctx, argc, argv, 3, 8),
                     (char)first_char,
                     dtr_api_opt_int(ctx, argc, argv, 5, 95));
    }
    return JS_UNDEFINED;
}

/* ---- Screen transitions ----------------------------------------------- */

static JSValue js_gfx_fade(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_fade(GFX(ctx),
                 (uint8_t)dtr_api_opt_int(ctx, argc, argv, 0, 0),
                 dtr_api_opt_int(ctx, argc, argv, 1, 30));
    return JS_UNDEFINED;
}

static JSValue js_gfx_wipe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_wipe(GFX(ctx),
                 dtr_api_opt_int(ctx, argc, argv, 0, 0),
                 (uint8_t)dtr_api_opt_int(ctx, argc, argv, 1, 0),
                 dtr_api_opt_int(ctx, argc, argv, 2, 30));
    return JS_UNDEFINED;
}

static JSValue js_gfx_dissolve(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_dissolve(GFX(ctx),
                     (uint8_t)dtr_api_opt_int(ctx, argc, argv, 0, 0),
                     dtr_api_opt_int(ctx, argc, argv, 1, 30));
    return JS_UNDEFINED;
}

static JSValue
js_gfx_transitioning(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewBool(ctx, dtr_gfx_transitioning(GFX(ctx)));
}

/* ---- Draw list (sprite batch) ----------------------------------------- */

static JSValue js_gfx_dl_begin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    dtr_gfx_dl_begin(GFX(ctx));
    return JS_UNDEFINED;
}

static JSValue js_gfx_dl_end(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    dtr_gfx_dl_end(GFX(ctx));
    return JS_UNDEFINED;
}

static JSValue js_gfx_dl_spr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_dl_spr(GFX(ctx),
                   dtr_api_opt_int(ctx, argc, argv, 0, 0),
                   dtr_api_opt_int(ctx, argc, argv, 1, 0),
                   dtr_api_opt_int(ctx, argc, argv, 2, 0),
                   dtr_api_opt_int(ctx, argc, argv, 3, 0),
                   dtr_api_opt_int(ctx, argc, argv, 4, 1),
                   dtr_api_opt_int(ctx, argc, argv, 5, 1),
                   dtr_api_opt_int(ctx, argc, argv, 6, 0) != 0,
                   dtr_api_opt_int(ctx, argc, argv, 7, 0) != 0);
    return JS_UNDEFINED;
}

static JSValue js_gfx_dl_sspr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_dl_sspr(GFX(ctx),
                    dtr_api_opt_int(ctx, argc, argv, 0, 0),
                    dtr_api_opt_int(ctx, argc, argv, 1, 0),
                    dtr_api_opt_int(ctx, argc, argv, 2, 0),
                    dtr_api_opt_int(ctx, argc, argv, 3, 0),
                    dtr_api_opt_int(ctx, argc, argv, 4, 0),
                    dtr_api_opt_int(ctx, argc, argv, 5, 0),
                    dtr_api_opt_int(ctx, argc, argv, 6, 0),
                    dtr_api_opt_int(ctx, argc, argv, 7, 0),
                    dtr_api_opt_int(ctx, argc, argv, 8, 0));
    return JS_UNDEFINED;
}

static JSValue
js_gfx_dl_spr_rot(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_dl_spr_rot(GFX(ctx),
                       dtr_api_opt_int(ctx, argc, argv, 0, 0),
                       dtr_api_opt_int(ctx, argc, argv, 1, 0),
                       dtr_api_opt_int(ctx, argc, argv, 2, 0),
                       dtr_api_opt_int(ctx, argc, argv, 3, 0),
                       (float)dtr_api_opt_float(ctx, argc, argv, 4, 0.0),
                       dtr_api_opt_int(ctx, argc, argv, 5, -1),
                       dtr_api_opt_int(ctx, argc, argv, 6, -1));
    return JS_UNDEFINED;
}

static JSValue
js_gfx_dl_spr_affine(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_dl_spr_affine(GFX(ctx),
                          dtr_api_opt_int(ctx, argc, argv, 0, 0),
                          dtr_api_opt_int(ctx, argc, argv, 1, 0),
                          dtr_api_opt_int(ctx, argc, argv, 2, 0),
                          dtr_api_opt_int(ctx, argc, argv, 3, 0),
                          (float)dtr_api_opt_float(ctx, argc, argv, 4, 0.0),
                          (float)dtr_api_opt_float(ctx, argc, argv, 5, 0.0),
                          (float)dtr_api_opt_float(ctx, argc, argv, 6, 1.0),
                          (float)dtr_api_opt_float(ctx, argc, argv, 7, 0.0));
    return JS_UNDEFINED;
}

/* ---- Spritesheet pixel access ----------------------------------------- */

static JSValue js_gfx_sget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewInt32(ctx,
                       dtr_gfx_sget(GFX(ctx),
                                    dtr_api_opt_int(ctx, argc, argv, 0, 0),
                                    dtr_api_opt_int(ctx, argc, argv, 1, 0)));
}

static JSValue js_gfx_sset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_gfx_sset(GFX(ctx),
                 dtr_api_opt_int(ctx, argc, argv, 0, 0),
                 dtr_api_opt_int(ctx, argc, argv, 1, 0),
                 prv_resolve_col(ctx, argc, argv, 2));
    return JS_UNDEFINED;
}

static JSValue js_gfx_sheet_w(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, GFX(ctx)->sheet.width);
}

static JSValue js_gfx_sheet_h(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, GFX(ctx)->sheet.height);
}

/* ---- Function list ---------------------------------------------------- */

static const JSCFunctionListEntry js_gfx_funcs[] = {
    JS_CFUNC_DEF("cls", 1, js_gfx_cls),
    JS_CFUNC_DEF("pset", 3, js_gfx_pset),
    JS_CFUNC_DEF("pget", 2, js_gfx_pget),
    JS_CFUNC_DEF("line", 5, js_gfx_line),
    JS_CFUNC_DEF("rect", 5, js_gfx_rect),
    JS_CFUNC_DEF("rectfill", 5, js_gfx_rectfill),
    JS_CFUNC_DEF("tilemap", 4, js_gfx_tilemap),
    JS_CFUNC_DEF("circ", 4, js_gfx_circ),
    JS_CFUNC_DEF("circfill", 4, js_gfx_circfill),
    JS_CFUNC_DEF("tri", 7, js_gfx_tri),
    JS_CFUNC_DEF("trifill", 7, js_gfx_trifill),
    JS_CFUNC_DEF("poly", 2, js_gfx_poly),
    JS_CFUNC_DEF("polyfill", 2, js_gfx_polyfill),
    JS_CFUNC_DEF("print", 4, js_gfx_print),
    JS_CFUNC_DEF("textWidth", 1, js_gfx_text_width),
    JS_CFUNC_DEF("textHeight", 1, js_gfx_text_height),
    JS_CFUNC_DEF("spr", 7, js_gfx_spr),
    JS_CFUNC_DEF("sspr", 10, js_gfx_sspr),
    JS_CFUNC_DEF("spr_rot", 6, js_gfx_spr_rot),
    JS_CFUNC_DEF("spr_affine", 7, js_gfx_spr_affine),
    JS_CFUNC_DEF("fget", 2, js_gfx_fget),
    JS_CFUNC_DEF("fset", 3, js_gfx_fset),
    JS_CFUNC_DEF("pal", 3, js_gfx_pal),
    JS_CFUNC_DEF("palt", 2, js_gfx_palt),
    JS_CFUNC_DEF("camera", 2, js_gfx_camera),
    JS_CFUNC_DEF("clip", 4, js_gfx_clip),
    JS_CFUNC_DEF("fillp", 1, js_gfx_fillp),
    JS_CFUNC_DEF("color", 1, js_gfx_color),
    JS_CFUNC_DEF("cursor", 2, js_gfx_cursor),
    JS_CFUNC_DEF("font", 6, js_gfx_font),
    JS_CFUNC_DEF("sget", 2, js_gfx_sget),
    JS_CFUNC_DEF("sset", 3, js_gfx_sset),
    JS_CFUNC_DEF("sheetW", 0, js_gfx_sheet_w),
    JS_CFUNC_DEF("sheetH", 0, js_gfx_sheet_h),
    JS_CFUNC_DEF("fade", 2, js_gfx_fade),
    JS_CFUNC_DEF("wipe", 3, js_gfx_wipe),
    JS_CFUNC_DEF("dissolve", 2, js_gfx_dissolve),
    JS_CFUNC_DEF("transitioning", 0, js_gfx_transitioning),
    JS_CFUNC_DEF("dl_begin", 0, js_gfx_dl_begin),
    JS_CFUNC_DEF("dl_end", 0, js_gfx_dl_end),
    JS_CFUNC_DEF("dl_spr", 8, js_gfx_dl_spr),
    JS_CFUNC_DEF("dl_sspr", 9, js_gfx_dl_sspr),
    JS_CFUNC_DEF("dl_spr_rot", 7, js_gfx_dl_spr_rot),
    JS_CFUNC_DEF("dl_spr_affine", 8, js_gfx_dl_spr_affine),
};

void dtr_gfx_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_gfx_funcs, countof(js_gfx_funcs));
    JS_SetPropertyStr(ctx, global, "gfx", ns);
}
