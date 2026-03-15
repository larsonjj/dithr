/**
 * \file            postfx_api.c
 * \brief           JS postfx namespace — post-processing pipeline
 */

#include "../postfx.h"
#include "api_common.h"

#define PFX(ctx) (mvn_api_get_console(ctx)->postfx)

static JSValue js_postfx_push(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_postfx_push(PFX(ctx),
                    (mvn_postfx_id_t)mvn_api_opt_int(ctx, argc, argv, 0, MVN_POSTFX_NONE),
                    (float)mvn_api_opt_float(ctx, argc, argv, 1, 1.0));
    return JS_UNDEFINED;
}

static JSValue js_postfx_pop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    mvn_postfx_pop(PFX(ctx));
    return JS_UNDEFINED;
}

static JSValue js_postfx_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    mvn_postfx_clear(PFX(ctx));
    return JS_UNDEFINED;
}

static JSValue js_postfx_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_postfx_set_param(PFX(ctx),
                         mvn_api_opt_int(ctx, argc, argv, 0, 0),
                         mvn_api_opt_int(ctx, argc, argv, 1, 0),
                         (float)mvn_api_opt_float(ctx, argc, argv, 2, 0.0));
    return JS_UNDEFINED;
}

static JSValue
js_postfx_supportsCustomShaders(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_FALSE;
}

static JSValue
js_postfx_loadShader(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    SDL_Log("postfx.loadShader: not implemented in Phase 1");
    return JS_FALSE;
}

static const JSCFunctionListEntry js_postfx_funcs[] = {
    JS_CFUNC_DEF("push", 2, js_postfx_push),
    JS_CFUNC_DEF("pop", 0, js_postfx_pop),
    JS_CFUNC_DEF("clear", 0, js_postfx_clear),
    JS_CFUNC_DEF("set", 3, js_postfx_set),
    JS_CFUNC_DEF("supportsCustomShaders", 0, js_postfx_supportsCustomShaders),
    JS_CFUNC_DEF("loadShader", 1, js_postfx_loadShader),
};

void mvn_postfx_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_postfx_funcs, countof(js_postfx_funcs));

    /* Effect ID constants */
    JS_SetPropertyStr(ctx, ns, "NONE", JS_NewInt32(ctx, MVN_POSTFX_NONE));
    JS_SetPropertyStr(ctx, ns, "CRT", JS_NewInt32(ctx, MVN_POSTFX_CRT));
    JS_SetPropertyStr(ctx, ns, "SCANLINES", JS_NewInt32(ctx, MVN_POSTFX_SCANLINES));
    JS_SetPropertyStr(ctx, ns, "BLOOM", JS_NewInt32(ctx, MVN_POSTFX_BLOOM));
    JS_SetPropertyStr(ctx, ns, "ABERRATION", JS_NewInt32(ctx, MVN_POSTFX_ABERRATION));

    JS_SetPropertyStr(ctx, global, "postfx", ns);
}
