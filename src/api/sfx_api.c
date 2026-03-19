/**
 * \file            sfx_api.c
 * \brief           JS sfx namespace — sound effect playback
 */

#include "../audio.h"
#include "api_common.h"

#define AUD(ctx) (dtr_api_get_console(ctx)->audio)

static JSValue js_sfx_play(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_sfx_play(AUD(ctx),
                 dtr_api_opt_int(ctx, argc, argv, 0, 0),
                 dtr_api_opt_int(ctx, argc, argv, 1, -1),
                 dtr_api_opt_int(ctx, argc, argv, 2, 0));
    return JS_UNDEFINED;
}

static JSValue js_sfx_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_sfx_stop(AUD(ctx), dtr_api_opt_int(ctx, argc, argv, 0, -1));
    return JS_UNDEFINED;
}

static JSValue js_sfx_volume(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_sfx_volume(AUD(ctx),
                   (float)dtr_api_opt_float(ctx, argc, argv, 0, 1.0),
                   dtr_api_opt_int(ctx, argc, argv, 1, -1));
    return JS_UNDEFINED;
}

static JSValue js_sfx_getVolume(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx, dtr_sfx_get_volume(AUD(ctx), dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_sfx_playing(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx, dtr_sfx_playing(AUD(ctx), dtr_api_opt_int(ctx, argc, argv, 0, -1)));
}

static const JSCFunctionListEntry js_sfx_funcs[] = {
    JS_CFUNC_DEF("play", 3, js_sfx_play),
    JS_CFUNC_DEF("stop", 1, js_sfx_stop),
    JS_CFUNC_DEF("volume", 2, js_sfx_volume),
    JS_CFUNC_DEF("getVolume", 1, js_sfx_getVolume),
    JS_CFUNC_DEF("playing", 1, js_sfx_playing),
};

void dtr_sfx_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_sfx_funcs, countof(js_sfx_funcs));
    JS_SetPropertyStr(ctx, global, "sfx", ns);
}
