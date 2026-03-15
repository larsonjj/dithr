/**
 * \file            mus_api.c
 * \brief           JS mus namespace — music playback
 */

#include "../audio.h"
#include "api_common.h"

#define AUD(ctx) (mvn_api_get_console(ctx)->audio)

static JSValue js_mus_play(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_mus_play(AUD(ctx),
                 mvn_api_opt_int(ctx, argc, argv, 0, 0),
                 mvn_api_opt_int(ctx, argc, argv, 1, 0),
                 (uint32_t)mvn_api_opt_int(ctx, argc, argv, 2, 0));
    return JS_UNDEFINED;
}

static JSValue js_mus_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_mus_stop(AUD(ctx), mvn_api_opt_int(ctx, argc, argv, 0, 0));
    return JS_UNDEFINED;
}

static JSValue js_mus_volume(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_mus_volume(AUD(ctx), (float)mvn_api_opt_float(ctx, argc, argv, 0, 1.0));
    return JS_UNDEFINED;
}

static JSValue js_mus_getVolume(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewFloat64(ctx, mvn_mus_get_volume(AUD(ctx)));
}

static JSValue js_mus_playing(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewBool(ctx, mvn_mus_playing(AUD(ctx)));
}

static const JSCFunctionListEntry js_mus_funcs[] = {
    JS_CFUNC_DEF("play", 3, js_mus_play),
    JS_CFUNC_DEF("stop", 1, js_mus_stop),
    JS_CFUNC_DEF("volume", 1, js_mus_volume),
    JS_CFUNC_DEF("getVolume", 0, js_mus_getVolume),
    JS_CFUNC_DEF("playing", 0, js_mus_playing),
};

void mvn_mus_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_mus_funcs, countof(js_mus_funcs));
    JS_SetPropertyStr(ctx, global, "mus", ns);
}
