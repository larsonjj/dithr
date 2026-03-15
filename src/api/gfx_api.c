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

/* ---- Function list ---------------------------------------------------- */

static const JSCFunctionListEntry js_gfx_funcs[] = {
    JS_CFUNC_DEF("cls", 1, js_gfx_cls),     JS_CFUNC_DEF("pset", 3, js_gfx_pset),
    JS_CFUNC_DEF("pget", 2, js_gfx_pget),   JS_CFUNC_DEF("line", 5, js_gfx_line),
    JS_CFUNC_DEF("rect", 5, js_gfx_rect),   JS_CFUNC_DEF("rectfill", 5, js_gfx_rectfill),
    JS_CFUNC_DEF("circ", 4, js_gfx_circ),   JS_CFUNC_DEF("circfill", 4, js_gfx_circfill),
    JS_CFUNC_DEF("tri", 7, js_gfx_tri),     JS_CFUNC_DEF("trifill", 7, js_gfx_trifill),
    JS_CFUNC_DEF("print", 4, js_gfx_print), JS_CFUNC_DEF("spr", 7, js_gfx_spr),
    JS_CFUNC_DEF("sspr", 10, js_gfx_sspr),  JS_CFUNC_DEF("fget", 2, js_gfx_fget),
    JS_CFUNC_DEF("fset", 3, js_gfx_fset),   JS_CFUNC_DEF("pal", 3, js_gfx_pal),
    JS_CFUNC_DEF("palt", 2, js_gfx_palt),   JS_CFUNC_DEF("camera", 2, js_gfx_camera),
    JS_CFUNC_DEF("clip", 4, js_gfx_clip),   JS_CFUNC_DEF("fillp", 1, js_gfx_fillp),
    JS_CFUNC_DEF("color", 1, js_gfx_color), JS_CFUNC_DEF("cursor", 2, js_gfx_cursor),
};

void mvn_gfx_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_gfx_funcs, countof(js_gfx_funcs));
    JS_SetPropertyStr(ctx, global, "gfx", ns);
}
