/**
 * \file            mouse_api.c
 * \brief           JS mouse namespace — position, buttons, wheel
 */

#include "../mouse.h"
#include "api_common.h"

#define MOUSE(ctx) (dtr_api_get_console(ctx)->mouse)

static JSValue js_mouse_x(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, (int32_t)MOUSE(ctx)->x);
}

static JSValue js_mouse_y(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, (int32_t)MOUSE(ctx)->y);
}

static JSValue js_mouse_dx(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, (int32_t)MOUSE(ctx)->dx);
}

static JSValue js_mouse_dy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, (int32_t)MOUSE(ctx)->dy);
}

static JSValue js_mouse_wheel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, (int32_t)MOUSE(ctx)->wheel);
}

static JSValue js_mouse_btn(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(
        ctx, dtr_mouse_btn(MOUSE(ctx), (dtr_mouse_btn_t)dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_mouse_btnp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(
        ctx, dtr_mouse_btnp(MOUSE(ctx), (dtr_mouse_btn_t)dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_mouse_show(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    dtr_mouse_show(MOUSE(ctx));
    return JS_UNDEFINED;
}

static JSValue js_mouse_hide(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    dtr_mouse_hide(MOUSE(ctx));
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_mouse_funcs[] = {
    JS_CFUNC_DEF("x", 0, js_mouse_x),
    JS_CFUNC_DEF("y", 0, js_mouse_y),
    JS_CFUNC_DEF("dx", 0, js_mouse_dx),
    JS_CFUNC_DEF("dy", 0, js_mouse_dy),
    JS_CFUNC_DEF("wheel", 0, js_mouse_wheel),
    JS_CFUNC_DEF("btn", 1, js_mouse_btn),
    JS_CFUNC_DEF("btnp", 1, js_mouse_btnp),
    JS_CFUNC_DEF("show", 0, js_mouse_show),
    JS_CFUNC_DEF("hide", 0, js_mouse_hide),
};

void dtr_mouse_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_mouse_funcs, countof(js_mouse_funcs));

    JS_SetPropertyStr(ctx, ns, "LEFT", JS_NewInt32(ctx, DTR_MOUSE_LEFT));
    JS_SetPropertyStr(ctx, ns, "MIDDLE", JS_NewInt32(ctx, DTR_MOUSE_MIDDLE));
    JS_SetPropertyStr(ctx, ns, "RIGHT", JS_NewInt32(ctx, DTR_MOUSE_RIGHT));

    JS_SetPropertyStr(ctx, global, "mouse", ns);
}
