/**
 * \file            touch_api.c
 * \brief           JS touch namespace — multi-finger position, pressure
 */

#include "../touch.h"
#include "api_common.h"

#define TOUCH(ctx) (dtr_api_get_console(ctx)->touch)

static JSValue js_touch_count(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, dtr_touch_count(TOUCH(ctx)));
}

static JSValue js_touch_active(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx, dtr_touch_active(TOUCH(ctx), dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_touch_pressed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx, dtr_touch_pressed(TOUCH(ctx), dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue
js_touch_released(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx, dtr_touch_released(TOUCH(ctx), dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_touch_x(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, dtr_touch_x(TOUCH(ctx), dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_touch_y(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, dtr_touch_y(TOUCH(ctx), dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_touch_dx(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, dtr_touch_dx(TOUCH(ctx), dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_touch_dy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, dtr_touch_dy(TOUCH(ctx), dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue
js_touch_pressure(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx,
                         dtr_touch_pressure(TOUCH(ctx), dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static const JSCFunctionListEntry js_touch_funcs[] = {
    JS_CFUNC_DEF("count", 0, js_touch_count),
    JS_CFUNC_DEF("active", 1, js_touch_active),
    JS_CFUNC_DEF("pressed", 1, js_touch_pressed),
    JS_CFUNC_DEF("released", 1, js_touch_released),
    JS_CFUNC_DEF("x", 1, js_touch_x),
    JS_CFUNC_DEF("y", 1, js_touch_y),
    JS_CFUNC_DEF("dx", 1, js_touch_dx),
    JS_CFUNC_DEF("dy", 1, js_touch_dy),
    JS_CFUNC_DEF("pressure", 1, js_touch_pressure),
};

void dtr_touch_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_touch_funcs, countof(js_touch_funcs));

    JS_SetPropertyStr(ctx, ns, "MAX_FINGERS", JS_NewInt32(ctx, DTR_MAX_FINGERS));

    JS_SetPropertyStr(ctx, global, "touch", ns);
}
