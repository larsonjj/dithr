/**
 * \file            gfx_api.c
 * \brief           JS gfx namespace — drawing, palette, sprites
 */

#include "../graphics.h"
#include "api_common.h"

#define GFX(ctx) (mvn_api_get_console(ctx)->graphics)

/**
 * \brief  Resolve an optional colour argument: -1 → current draw colour
 */
static uint8_t prv_resolve_col(JSContext *ctx, int argc, JSValueConst *argv, int idx)
{
    int32_t raw = mvn_api_opt_int(ctx, argc, argv, idx, -1);
    return (raw < 0) ? GFX(ctx)->color : (uint8_t)raw;
}

/* ---- Drawing ---------------------------------------------------------- */

static JSValue js_gfx_cls(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_cls(GFX(ctx), (uint8_t)mvn_api_opt_int(ctx, argc, argv, 0, 0));
    return JS_UNDEFINED;
}

static JSValue js_gfx_pset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_pset(GFX(ctx),
                 mvn_api_opt_int(ctx, argc, argv, 0, 0),
                 mvn_api_opt_int(ctx, argc, argv, 1, 0),
                 prv_resolve_col(ctx, argc, argv, 2));
    return JS_UNDEFINED;
}

static JSValue js_gfx_pget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewInt32(ctx,
                       mvn_gfx_pget(GFX(ctx),
                                    mvn_api_opt_int(ctx, argc, argv, 0, 0),
                                    mvn_api_opt_int(ctx, argc, argv, 1, 0)));
}

static JSValue js_gfx_line(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_line(GFX(ctx),
                 mvn_api_opt_int(ctx, argc, argv, 0, 0),
                 mvn_api_opt_int(ctx, argc, argv, 1, 0),
                 mvn_api_opt_int(ctx, argc, argv, 2, 0),
                 mvn_api_opt_int(ctx, argc, argv, 3, 0),
                 prv_resolve_col(ctx, argc, argv, 4));
    return JS_UNDEFINED;
}

static JSValue js_gfx_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_rect(GFX(ctx),
                 mvn_api_opt_int(ctx, argc, argv, 0, 0),
                 mvn_api_opt_int(ctx, argc, argv, 1, 0),
                 mvn_api_opt_int(ctx, argc, argv, 2, 0),
                 mvn_api_opt_int(ctx, argc, argv, 3, 0),
                 prv_resolve_col(ctx, argc, argv, 4));
    return JS_UNDEFINED;
}

static JSValue js_gfx_rectfill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_rectfill(GFX(ctx),
                     mvn_api_opt_int(ctx, argc, argv, 0, 0),
                     mvn_api_opt_int(ctx, argc, argv, 1, 0),
                     mvn_api_opt_int(ctx, argc, argv, 2, 0),
                     mvn_api_opt_int(ctx, argc, argv, 3, 0),
                     prv_resolve_col(ctx, argc, argv, 4));
    return JS_UNDEFINED;
}

static JSValue js_gfx_circ(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_circ(GFX(ctx),
                 mvn_api_opt_int(ctx, argc, argv, 0, 0),
                 mvn_api_opt_int(ctx, argc, argv, 1, 0),
                 mvn_api_opt_int(ctx, argc, argv, 2, 4),
                 prv_resolve_col(ctx, argc, argv, 3));
    return JS_UNDEFINED;
}

static JSValue js_gfx_circfill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_circfill(GFX(ctx),
                     mvn_api_opt_int(ctx, argc, argv, 0, 0),
                     mvn_api_opt_int(ctx, argc, argv, 1, 0),
                     mvn_api_opt_int(ctx, argc, argv, 2, 4),
                     prv_resolve_col(ctx, argc, argv, 3));
    return JS_UNDEFINED;
}

static JSValue js_gfx_tri(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_tri(GFX(ctx),
                mvn_api_opt_int(ctx, argc, argv, 0, 0),
                mvn_api_opt_int(ctx, argc, argv, 1, 0),
                mvn_api_opt_int(ctx, argc, argv, 2, 0),
                mvn_api_opt_int(ctx, argc, argv, 3, 0),
                mvn_api_opt_int(ctx, argc, argv, 4, 0),
                mvn_api_opt_int(ctx, argc, argv, 5, 0),
                prv_resolve_col(ctx, argc, argv, 6));
    return JS_UNDEFINED;
}

static JSValue js_gfx_trifill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_trifill(GFX(ctx),
                    mvn_api_opt_int(ctx, argc, argv, 0, 0),
                    mvn_api_opt_int(ctx, argc, argv, 1, 0),
                    mvn_api_opt_int(ctx, argc, argv, 2, 0),
                    mvn_api_opt_int(ctx, argc, argv, 3, 0),
                    mvn_api_opt_int(ctx, argc, argv, 4, 0),
                    mvn_api_opt_int(ctx, argc, argv, 5, 0),
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

    n = prv_read_int_array(ctx, argv[0], pts, PRV_MAX_POLY_PTS * 2);
    vert_count = n / 2;

    if (vert_count >= 2) {
        mvn_gfx_poly(GFX(ctx), pts, vert_count, prv_resolve_col(ctx, argc, argv, 1));
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

    n = prv_read_int_array(ctx, argv[0], pts, PRV_MAX_POLY_PTS * 2);
    vert_count = n / 2;

    if (vert_count >= 3) {
        mvn_gfx_polyfill(GFX(ctx), pts, vert_count, prv_resolve_col(ctx, argc, argv, 1));
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

    x   = mvn_api_opt_int(ctx, argc, argv, 1, -1);
    y   = mvn_api_opt_int(ctx, argc, argv, 2, -1);
    col = prv_resolve_col(ctx, argc, argv, 3);

    if (x == -1 && y == -1) {
        /* Use cursor position */
        mvn_gfx_print(GFX(ctx), text, GFX(ctx)->cursor_x, GFX(ctx)->cursor_y, col);
    } else {
        mvn_gfx_print(GFX(ctx), text, x, y, col);
    }

    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

/* ---- Sprites ---------------------------------------------------------- */

static JSValue js_gfx_spr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_spr(GFX(ctx),
                mvn_api_opt_int(ctx, argc, argv, 0, 0),
                mvn_api_opt_int(ctx, argc, argv, 1, 0),
                mvn_api_opt_int(ctx, argc, argv, 2, 0),
                mvn_api_opt_int(ctx, argc, argv, 3, 1),
                mvn_api_opt_int(ctx, argc, argv, 4, 1),
                mvn_api_opt_int(ctx, argc, argv, 5, 0) != 0,
                mvn_api_opt_int(ctx, argc, argv, 6, 0) != 0);
    return JS_UNDEFINED;
}

static JSValue js_gfx_sspr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_sspr(GFX(ctx),
                 mvn_api_opt_int(ctx, argc, argv, 0, 0),
                 mvn_api_opt_int(ctx, argc, argv, 1, 0),
                 mvn_api_opt_int(ctx, argc, argv, 2, 8),
                 mvn_api_opt_int(ctx, argc, argv, 3, 8),
                 mvn_api_opt_int(ctx, argc, argv, 4, 0),
                 mvn_api_opt_int(ctx, argc, argv, 5, 0),
                 mvn_api_opt_int(ctx, argc, argv, 6, 8),
                 mvn_api_opt_int(ctx, argc, argv, 7, 8));
    return JS_UNDEFINED;
}

static JSValue js_gfx_spr_rot(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_spr_rot(GFX(ctx),
                    mvn_api_opt_int(ctx, argc, argv, 0, 0),
                    mvn_api_opt_int(ctx, argc, argv, 1, 0),
                    mvn_api_opt_int(ctx, argc, argv, 2, 0),
                    (float)mvn_api_opt_float(ctx, argc, argv, 3, 0.0),
                    mvn_api_opt_int(ctx, argc, argv, 4, -1),
                    mvn_api_opt_int(ctx, argc, argv, 5, -1));
    return JS_UNDEFINED;
}

static JSValue
js_gfx_spr_affine(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_spr_affine(GFX(ctx),
                       mvn_api_opt_int(ctx, argc, argv, 0, 0),
                       mvn_api_opt_int(ctx, argc, argv, 1, 0),
                       mvn_api_opt_int(ctx, argc, argv, 2, 0),
                       (float)mvn_api_opt_float(ctx, argc, argv, 3, 0.0),
                       (float)mvn_api_opt_float(ctx, argc, argv, 4, 0.0),
                       (float)mvn_api_opt_float(ctx, argc, argv, 5, 1.0),
                       (float)mvn_api_opt_float(ctx, argc, argv, 6, 0.0));
    return JS_UNDEFINED;
}

/* ---- Sprite flags ----------------------------------------------------- */

static JSValue js_gfx_fget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t n;
    int32_t bit;

    (void)this_val;
    n   = mvn_api_opt_int(ctx, argc, argv, 0, 0);
    bit = mvn_api_opt_int(ctx, argc, argv, 1, -1);

    if (bit < 0) {
        return JS_NewInt32(ctx, mvn_gfx_fget(GFX(ctx), n));
    }
    return JS_NewBool(ctx, mvn_gfx_fget_bit(GFX(ctx), n, bit) != 0);
}

static JSValue js_gfx_fset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_fset_bit(GFX(ctx),
                 mvn_api_opt_int(ctx, argc, argv, 0, 0),
                 mvn_api_opt_int(ctx, argc, argv, 1, 0),
                 mvn_api_opt_int(ctx, argc, argv, 2, 1) != 0);
    return JS_UNDEFINED;
}

/* ---- Palette ---------------------------------------------------------- */

static JSValue js_gfx_pal(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    if (argc == 0) {
        mvn_gfx_pal_reset(GFX(ctx));
    } else {
        mvn_gfx_pal(GFX(ctx),
                    (uint8_t)mvn_api_opt_int(ctx, argc, argv, 0, 0),
                    (uint8_t)mvn_api_opt_int(ctx, argc, argv, 1, 0),
                    mvn_api_opt_int(ctx, argc, argv, 2, 0));
    }
    return JS_UNDEFINED;
}

static JSValue js_gfx_palt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    if (argc == 0) {
        mvn_gfx_palt_reset(GFX(ctx));
    } else {
        mvn_gfx_palt(GFX(ctx),
                     (uint8_t)mvn_api_opt_int(ctx, argc, argv, 0, 0),
                     mvn_api_opt_int(ctx, argc, argv, 1, 1) != 0);
    }
    return JS_UNDEFINED;
}

/* ---- State ------------------------------------------------------------ */

static JSValue js_gfx_camera(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_camera(
        GFX(ctx), mvn_api_opt_int(ctx, argc, argv, 0, 0), mvn_api_opt_int(ctx, argc, argv, 1, 0));
    return JS_UNDEFINED;
}

static JSValue js_gfx_clip(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    if (argc == 0) {
        mvn_gfx_clip(GFX(ctx), 0, 0, GFX(ctx)->width, GFX(ctx)->height);
    } else {
        mvn_gfx_clip(GFX(ctx),
                     mvn_api_opt_int(ctx, argc, argv, 0, 0),
                     mvn_api_opt_int(ctx, argc, argv, 1, 0),
                     mvn_api_opt_int(ctx, argc, argv, 2, 0),
                     mvn_api_opt_int(ctx, argc, argv, 3, 0));
    }
    return JS_UNDEFINED;
}

static JSValue js_gfx_fillp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_fillp(GFX(ctx), (uint16_t)mvn_api_opt_int(ctx, argc, argv, 0, 0));
    return JS_UNDEFINED;
}

static JSValue js_gfx_color(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_color(GFX(ctx), (uint8_t)mvn_api_opt_int(ctx, argc, argv, 0, 7));
    return JS_UNDEFINED;
}

static JSValue js_gfx_cursor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_cursor(
        GFX(ctx), mvn_api_opt_int(ctx, argc, argv, 0, 0), mvn_api_opt_int(ctx, argc, argv, 1, 0));
    return JS_UNDEFINED;
}

static JSValue js_gfx_font(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    if (argc == 0) {
        mvn_gfx_font_reset(GFX(ctx));
    } else {
        int32_t first_char;

        first_char = mvn_api_opt_int(ctx, argc, argv, 4, 32);
        mvn_gfx_font(GFX(ctx),
                     mvn_api_opt_int(ctx, argc, argv, 0, 0),
                     mvn_api_opt_int(ctx, argc, argv, 1, 0),
                     mvn_api_opt_int(ctx, argc, argv, 2, 8),
                     mvn_api_opt_int(ctx, argc, argv, 3, 8),
                     (char)first_char,
                     mvn_api_opt_int(ctx, argc, argv, 5, 95));
    }
    return JS_UNDEFINED;
}

/* ---- Screen transitions ----------------------------------------------- */

static JSValue js_gfx_fade(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_fade(GFX(ctx),
                 (uint8_t)mvn_api_opt_int(ctx, argc, argv, 0, 0),
                 mvn_api_opt_int(ctx, argc, argv, 1, 30));
    return JS_UNDEFINED;
}

static JSValue js_gfx_wipe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_wipe(GFX(ctx),
                 mvn_api_opt_int(ctx, argc, argv, 0, 0),
                 (uint8_t)mvn_api_opt_int(ctx, argc, argv, 1, 0),
                 mvn_api_opt_int(ctx, argc, argv, 2, 30));
    return JS_UNDEFINED;
}

static JSValue js_gfx_dissolve(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_dissolve(GFX(ctx),
                     (uint8_t)mvn_api_opt_int(ctx, argc, argv, 0, 0),
                     mvn_api_opt_int(ctx, argc, argv, 1, 30));
    return JS_UNDEFINED;
}

static JSValue
js_gfx_transitioning(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewBool(ctx, mvn_gfx_transitioning(GFX(ctx)));
}

/* ---- Draw list (sprite batch) ----------------------------------------- */

static JSValue js_gfx_dl_begin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    mvn_gfx_dl_begin(GFX(ctx));
    return JS_UNDEFINED;
}

static JSValue js_gfx_dl_end(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    mvn_gfx_dl_end(GFX(ctx));
    return JS_UNDEFINED;
}

static JSValue js_gfx_dl_spr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_dl_spr(GFX(ctx),
                   mvn_api_opt_int(ctx, argc, argv, 0, 0),
                   mvn_api_opt_int(ctx, argc, argv, 1, 0),
                   mvn_api_opt_int(ctx, argc, argv, 2, 0),
                   mvn_api_opt_int(ctx, argc, argv, 3, 0),
                   mvn_api_opt_int(ctx, argc, argv, 4, 1),
                   mvn_api_opt_int(ctx, argc, argv, 5, 1),
                   mvn_api_opt_int(ctx, argc, argv, 6, 0) != 0,
                   mvn_api_opt_int(ctx, argc, argv, 7, 0) != 0);
    return JS_UNDEFINED;
}

static JSValue js_gfx_dl_sspr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_dl_sspr(GFX(ctx),
                    mvn_api_opt_int(ctx, argc, argv, 0, 0),
                    mvn_api_opt_int(ctx, argc, argv, 1, 0),
                    mvn_api_opt_int(ctx, argc, argv, 2, 0),
                    mvn_api_opt_int(ctx, argc, argv, 3, 0),
                    mvn_api_opt_int(ctx, argc, argv, 4, 0),
                    mvn_api_opt_int(ctx, argc, argv, 5, 0),
                    mvn_api_opt_int(ctx, argc, argv, 6, 0),
                    mvn_api_opt_int(ctx, argc, argv, 7, 0),
                    mvn_api_opt_int(ctx, argc, argv, 8, 0));
    return JS_UNDEFINED;
}

static JSValue
js_gfx_dl_spr_rot(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_dl_spr_rot(GFX(ctx),
                       mvn_api_opt_int(ctx, argc, argv, 0, 0),
                       mvn_api_opt_int(ctx, argc, argv, 1, 0),
                       mvn_api_opt_int(ctx, argc, argv, 2, 0),
                       mvn_api_opt_int(ctx, argc, argv, 3, 0),
                       (float)mvn_api_opt_float(ctx, argc, argv, 4, 0.0),
                       mvn_api_opt_int(ctx, argc, argv, 5, -1),
                       mvn_api_opt_int(ctx, argc, argv, 6, -1));
    return JS_UNDEFINED;
}

static JSValue
js_gfx_dl_spr_affine(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gfx_dl_spr_affine(GFX(ctx),
                          mvn_api_opt_int(ctx, argc, argv, 0, 0),
                          mvn_api_opt_int(ctx, argc, argv, 1, 0),
                          mvn_api_opt_int(ctx, argc, argv, 2, 0),
                          mvn_api_opt_int(ctx, argc, argv, 3, 0),
                          (float)mvn_api_opt_float(ctx, argc, argv, 4, 0.0),
                          (float)mvn_api_opt_float(ctx, argc, argv, 5, 0.0),
                          (float)mvn_api_opt_float(ctx, argc, argv, 6, 1.0),
                          (float)mvn_api_opt_float(ctx, argc, argv, 7, 0.0));
    return JS_UNDEFINED;
}

/* ---- Function list ---------------------------------------------------- */

static const JSCFunctionListEntry js_gfx_funcs[] = {
    JS_CFUNC_DEF("cls", 1, js_gfx_cls),     JS_CFUNC_DEF("pset", 3, js_gfx_pset),
    JS_CFUNC_DEF("pget", 2, js_gfx_pget),   JS_CFUNC_DEF("line", 5, js_gfx_line),
    JS_CFUNC_DEF("rect", 5, js_gfx_rect),   JS_CFUNC_DEF("rectfill", 5, js_gfx_rectfill),
    JS_CFUNC_DEF("circ", 4, js_gfx_circ),   JS_CFUNC_DEF("circfill", 4, js_gfx_circfill),
    JS_CFUNC_DEF("tri", 7, js_gfx_tri),     JS_CFUNC_DEF("trifill", 7, js_gfx_trifill),
    JS_CFUNC_DEF("poly", 2, js_gfx_poly),   JS_CFUNC_DEF("polyfill", 2, js_gfx_polyfill),
    JS_CFUNC_DEF("print", 4, js_gfx_print), JS_CFUNC_DEF("spr", 7, js_gfx_spr),
    JS_CFUNC_DEF("sspr", 10, js_gfx_sspr),  JS_CFUNC_DEF("spr_rot", 6, js_gfx_spr_rot),
    JS_CFUNC_DEF("spr_affine", 7, js_gfx_spr_affine),
    JS_CFUNC_DEF("fget", 2, js_gfx_fget),
    JS_CFUNC_DEF("fset", 3, js_gfx_fset),   JS_CFUNC_DEF("pal", 3, js_gfx_pal),
    JS_CFUNC_DEF("palt", 2, js_gfx_palt),   JS_CFUNC_DEF("camera", 2, js_gfx_camera),
    JS_CFUNC_DEF("clip", 4, js_gfx_clip),   JS_CFUNC_DEF("fillp", 1, js_gfx_fillp),
    JS_CFUNC_DEF("color", 1, js_gfx_color), JS_CFUNC_DEF("cursor", 2, js_gfx_cursor),
    JS_CFUNC_DEF("font", 6, js_gfx_font),
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

void mvn_gfx_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_gfx_funcs, countof(js_gfx_funcs));
    JS_SetPropertyStr(ctx, global, "gfx", ns);
}
