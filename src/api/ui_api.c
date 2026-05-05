/**
 * \file            ui_api.c
 * \brief           JS bindings for stateless UI layout helpers
 */

#include "../ui.h"
#include "api_common.h"

/* ---- Helper: build {x, y, w, h} JS object from rect ------------------- */

static JSValue prv_rect_to_js(JSContext *ctx, dtr_ui_rect_t rect)
{
    JSValue obj;

    obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, rect.pos_x));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, rect.pos_y));
    JS_SetPropertyStr(ctx, obj, "w", JS_NewInt32(ctx, rect.width));
    JS_SetPropertyStr(ctx, obj, "h", JS_NewInt32(ctx, rect.height));
    return obj;
}

/* ---- Helper: read {x, y, w, h} JS object into rect -------------------- */

static dtr_ui_rect_t prv_js_to_rect(JSContext *ctx, JSValueConst val)
{
    dtr_ui_rect_t rect;
    JSValue       prop;

    prop = JS_GetPropertyStr(ctx, val, "x");
    JS_ToInt32(ctx, &rect.pos_x, prop);
    JS_FreeValue(ctx, prop);

    prop = JS_GetPropertyStr(ctx, val, "y");
    JS_ToInt32(ctx, &rect.pos_y, prop);
    JS_FreeValue(ctx, prop);

    prop = JS_GetPropertyStr(ctx, val, "w");
    JS_ToInt32(ctx, &rect.width, prop);
    JS_FreeValue(ctx, prop);

    prop = JS_GetPropertyStr(ctx, val, "h");
    JS_ToInt32(ctx, &rect.height, prop);
    JS_FreeValue(ctx, prop);

    return rect;
}

/* ------------------------------------------------------------------ */
/*  ui.rect(x, y, w, h) → {x, y, w, h}                                */
/* ------------------------------------------------------------------ */

static JSValue js_ui_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return prv_rect_to_js(ctx,
                          dtr_ui_rect(dtr_api_opt_int(ctx, argc, argv, 0, 0),
                                      dtr_api_opt_int(ctx, argc, argv, 1, 0),
                                      dtr_api_opt_int(ctx, argc, argv, 2, 0),
                                      dtr_api_opt_int(ctx, argc, argv, 3, 0)));
}

/* ------------------------------------------------------------------ */
/*  ui.inset(rect, n) → {x, y, w, h}                                   */
/* ------------------------------------------------------------------ */

static JSValue js_ui_inset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_ui_rect_t rect;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }
    rect = prv_js_to_rect(ctx, argv[0]);
    return prv_rect_to_js(ctx, dtr_ui_inset(rect, dtr_api_opt_int(ctx, argc, argv, 1, 0)));
}

/* ------------------------------------------------------------------ */
/*  ui.anchor(ax, ay, w, h) → {x, y, w, h}                             */
/* ------------------------------------------------------------------ */

static JSValue js_ui_anchor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    return prv_rect_to_js(ctx,
                          dtr_ui_anchor(dtr_api_opt_float(ctx, argc, argv, 0, 0.0),
                                        dtr_api_opt_float(ctx, argc, argv, 1, 0.0),
                                        dtr_api_opt_int(ctx, argc, argv, 2, 0),
                                        dtr_api_opt_int(ctx, argc, argv, 3, 0),
                                        con->fb_width,
                                        con->fb_height));
}

/* ------------------------------------------------------------------ */
/*  ui.hsplit(rect, t, gap?) → [left, right]                            */
/* ------------------------------------------------------------------ */

static JSValue js_ui_hsplit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t left;
    dtr_ui_rect_t right;
    JSValue       arr;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }
    rect = prv_js_to_rect(ctx, argv[0]);
    dtr_ui_hsplit(rect,
                  dtr_api_opt_float(ctx, argc, argv, 1, 0.5),
                  dtr_api_opt_int(ctx, argc, argv, 2, 0),
                  &left,
                  &right);

    arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, prv_rect_to_js(ctx, left));
    JS_SetPropertyUint32(ctx, arr, 1, prv_rect_to_js(ctx, right));
    return arr;
}

/* ------------------------------------------------------------------ */
/*  ui.vsplit(rect, t, gap?) → [top, bottom]                            */
/* ------------------------------------------------------------------ */

static JSValue js_ui_vsplit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t top;
    dtr_ui_rect_t bottom;
    JSValue       arr;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }
    rect = prv_js_to_rect(ctx, argv[0]);
    dtr_ui_vsplit(rect,
                  dtr_api_opt_float(ctx, argc, argv, 1, 0.5),
                  dtr_api_opt_int(ctx, argc, argv, 2, 0),
                  &top,
                  &bottom);

    arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, prv_rect_to_js(ctx, top));
    JS_SetPropertyUint32(ctx, arr, 1, prv_rect_to_js(ctx, bottom));
    return arr;
}

/* ------------------------------------------------------------------ */
/*  ui.hstack(rect, n, gap?) → [{x,y,w,h}, ...]                        */
/* ------------------------------------------------------------------ */

#define UI_MAX_STACK 64

static JSValue js_ui_hstack(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t out[UI_MAX_STACK];
    int32_t       count;
    int32_t       idx;
    JSValue       arr;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }
    rect  = prv_js_to_rect(ctx, argv[0]);
    count = dtr_api_opt_int(ctx, argc, argv, 1, 1);
    if (count < 1) {
        count = 1;
    }
    if (count > UI_MAX_STACK) {
        count = UI_MAX_STACK;
    }
    dtr_ui_hstack(rect, count, dtr_api_opt_int(ctx, argc, argv, 2, 0), out);

    arr = JS_NewArray(ctx);
    for (idx = 0; idx < count; ++idx) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)idx, prv_rect_to_js(ctx, out[idx]));
    }
    return arr;
}

/* ------------------------------------------------------------------ */
/*  ui.vstack(rect, n, gap?) → [{x,y,w,h}, ...]                        */
/* ------------------------------------------------------------------ */

static JSValue js_ui_vstack(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_ui_rect_t rect;
    dtr_ui_rect_t out[UI_MAX_STACK];
    int32_t       count;
    int32_t       idx;
    JSValue       arr;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }
    rect  = prv_js_to_rect(ctx, argv[0]);
    count = dtr_api_opt_int(ctx, argc, argv, 1, 1);
    if (count < 1) {
        count = 1;
    }
    if (count > UI_MAX_STACK) {
        count = UI_MAX_STACK;
    }
    dtr_ui_vstack(rect, count, dtr_api_opt_int(ctx, argc, argv, 2, 0), out);

    arr = JS_NewArray(ctx);
    for (idx = 0; idx < count; ++idx) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)idx, prv_rect_to_js(ctx, out[idx]));
    }
    return arr;
}

/* ------------------------------------------------------------------ */
/*  ui.place(parent, ax, ay, w, h) → {x, y, w, h}                      */
/* ------------------------------------------------------------------ */

static JSValue js_ui_place(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_ui_rect_t parent;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }
    parent = prv_js_to_rect(ctx, argv[0]);
    return prv_rect_to_js(ctx,
                          dtr_ui_place(parent,
                                       dtr_api_opt_float(ctx, argc, argv, 1, 0.5),
                                       dtr_api_opt_float(ctx, argc, argv, 2, 0.5),
                                       dtr_api_opt_int(ctx, argc, argv, 3, 0),
                                       dtr_api_opt_int(ctx, argc, argv, 4, 0)));
}

/* ---- Function list ---------------------------------------------------- */

/* ------------------------------------------------------------------ */
/*  ui.groupPush(rect, clip?) → bool                                    */
/* ------------------------------------------------------------------ */

static JSValue js_ui_group_push(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_ui_rect_t  rect;
    bool           clip;
    dtr_console_t *con;

    (void)this_val;
    if (argc < 1) {
        return JS_FALSE;
    }
    con  = dtr_api_get_console(ctx);
    rect = prv_js_to_rect(ctx, argv[0]);
    clip = argc >= 2 ? (bool)JS_ToBool(ctx, argv[1]) : false;
    return JS_NewBool(ctx, dtr_ui_group_push(con->ui, rect, clip));
}

/* ------------------------------------------------------------------ */
/*  ui.groupPop() → undefined                                           */
/* ------------------------------------------------------------------ */

static JSValue js_ui_group_pop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);
    dtr_ui_group_pop(con->ui);
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  ui.groupRect() → {x, y, w, h}                                      */
/* ------------------------------------------------------------------ */

static JSValue js_ui_group_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);
    return prv_rect_to_js(ctx, dtr_ui_group_current(con->ui));
}

/* ------------------------------------------------------------------ */
/*  ui.fit(rect) → {x, y, w, h}                                        */
/* ------------------------------------------------------------------ */

static JSValue js_ui_fit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }
    con = dtr_api_get_console(ctx);
    return prv_rect_to_js(ctx, dtr_ui_fit(con->ui, prv_js_to_rect(ctx, argv[0])));
}

/* ------------------------------------------------------------------ */
/*  ui.withGroup(rect, fn, opts?) → undefined                           */
/*  opts: { clip?: boolean }                                            */
/* ------------------------------------------------------------------ */

static JSValue js_ui_with_group(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    dtr_ui_rect_t  rect;
    bool           clip;
    JSValue        opts_clip;
    JSValue        result;

    (void)this_val;
    if (argc < 2) {
        return JS_UNDEFINED;
    }

    con  = dtr_api_get_console(ctx);
    rect = prv_js_to_rect(ctx, argv[0]);
    clip = false;

    /* opts (arg 2) may be an object with { clip: bool } */
    if (argc >= 3 && JS_IsObject(argv[2])) {
        opts_clip = JS_GetPropertyStr(ctx, argv[2], "clip");
        if (!JS_IsUndefined(opts_clip)) {
            clip = (bool)JS_ToBool(ctx, opts_clip);
        }
        JS_FreeValue(ctx, opts_clip);
    }

    dtr_ui_group_push(con->ui, rect, clip);
    result = JS_Call(ctx, argv[1], JS_UNDEFINED, 0, NULL);
    dtr_ui_group_pop(con->ui);

    if (JS_IsException(result)) {
        JS_FreeValue(ctx, result);
    } else {
        JS_FreeValue(ctx, result);
    }
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_ui_funcs[] = {
    JS_CFUNC_DEF("rect", 4, js_ui_rect),
    JS_CFUNC_DEF("inset", 2, js_ui_inset),
    JS_CFUNC_DEF("anchor", 4, js_ui_anchor),
    JS_CFUNC_DEF("hsplit", 3, js_ui_hsplit),
    JS_CFUNC_DEF("vsplit", 3, js_ui_vsplit),
    JS_CFUNC_DEF("hstack", 3, js_ui_hstack),
    JS_CFUNC_DEF("vstack", 3, js_ui_vstack),
    JS_CFUNC_DEF("place", 5, js_ui_place),
    JS_CFUNC_DEF("groupPush", 2, js_ui_group_push),
    JS_CFUNC_DEF("groupPop", 0, js_ui_group_pop),
    JS_CFUNC_DEF("groupRect", 0, js_ui_group_rect),
    JS_CFUNC_DEF("fit", 1, js_ui_fit),
    JS_CFUNC_DEF("withGroup", 3, js_ui_with_group),
};

void dtr_ui_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_ui_funcs, countof(js_ui_funcs));
    JS_SetPropertyStr(ctx, global, "ui", ns);
}
