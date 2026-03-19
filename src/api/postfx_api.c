/**
 * \file            postfx_api.c
 * \brief           JS postfx namespace — post-processing pipeline
 */

#include "../postfx.h"
#include "api_common.h"

#define PFX(ctx) (dtr_api_get_console(ctx)->postfx)

/* ------------------------------------------------------------------ */
/*  Low-level stack manipulation (push / pop / clear / set)            */
/* ------------------------------------------------------------------ */

static JSValue js_postfx_push(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t id_val;
    float   str_val;
    (void)this_val;
    id_val  = dtr_api_opt_int(ctx, argc, argv, 0, DTR_POSTFX_NONE);
    str_val = (float)dtr_api_opt_float(ctx, argc, argv, 1, 1.0);
    dtr_postfx_push(PFX(ctx), (dtr_postfx_id_t)id_val, str_val);
    return JS_UNDEFINED;
}

static JSValue js_postfx_pop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    dtr_postfx_pop(PFX(ctx));
    return JS_UNDEFINED;
}

static JSValue js_postfx_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    dtr_postfx_clear(PFX(ctx));
    return JS_UNDEFINED;
}

static JSValue js_postfx_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    dtr_postfx_set_param(PFX(ctx),
                         dtr_api_opt_int(ctx, argc, argv, 0, 0),
                         dtr_api_opt_int(ctx, argc, argv, 1, 0),
                         (float)dtr_api_opt_float(ctx, argc, argv, 2, 0.0));
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  postfx.use(name)  — set active effect by name, return chain obj   */
/* ------------------------------------------------------------------ */

static JSValue
js_postfx_chain_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t index;
    JSValue val;

    val = JS_GetPropertyStr(ctx, this_val, "_idx");
    JS_ToInt32(ctx, &index, val);
    JS_FreeValue(ctx, val);

    dtr_postfx_set_param(PFX(ctx),
                         index,
                         dtr_api_opt_int(ctx, argc, argv, 0, 0),
                         (float)dtr_api_opt_float(ctx, argc, argv, 1, 0.0));

    return JS_DupValue(ctx, this_val);
}

static JSValue js_postfx_use(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *name;
    int32_t     index;
    JSValue     chain;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_UNDEFINED;
    }

    index = dtr_postfx_use(PFX(ctx), name);
    JS_FreeCString(ctx, name);

    if (index < 0) {
        return JS_UNDEFINED;
    }

    /* Return a chain object { _idx, set() } */
    chain = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, chain, "_idx", JS_NewInt32(ctx, index));
    JS_SetPropertyStr(ctx, chain, "set",
                      JS_NewCFunction(ctx, js_postfx_chain_set, "set", 2));
    return chain;
}

/* ------------------------------------------------------------------ */
/*  postfx.stack(names)  — apply an array of effects at once           */
/* ------------------------------------------------------------------ */

static JSValue js_postfx_stack(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue  arr;
    JSValue  len_val;
    int64_t  len;

    (void)this_val;
    if (argc < 1 || !JS_IsArray(argv[0])) {
        return JS_UNDEFINED;
    }

    dtr_postfx_clear(PFX(ctx));

    arr     = argv[0];
    len_val = JS_GetPropertyStr(ctx, arr, "length");
    JS_ToInt64(ctx, &len, len_val);
    JS_FreeValue(ctx, len_val);

    for (int64_t idx = 0; idx < len; ++idx) {
        JSValue     elem;
        const char *name;

        elem = JS_GetPropertyUint32(ctx, arr, (uint32_t)idx);
        name = JS_ToCString(ctx, elem);
        JS_FreeValue(ctx, elem);

        if (name != NULL) {
            dtr_postfx_id_t id;

            id = dtr_postfx_id_from_name(name);
            if (id != DTR_POSTFX_NONE) {
                dtr_postfx_push(PFX(ctx), id, 1.0f);
            }
            JS_FreeCString(ctx, name);
        }
    }

    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  postfx.available()  — return array of built-in effect names        */
/* ------------------------------------------------------------------ */

static JSValue
js_postfx_available(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char **names;
    int32_t      count;
    JSValue      arr;

    (void)this_val;
    (void)argc;
    (void)argv;
    names = dtr_postfx_available(&count);
    arr   = JS_NewArray(ctx);

    for (int32_t idx = 0; idx < count; ++idx) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)idx, JS_NewString(ctx, names[idx]));
    }
    return arr;
}

/* ------------------------------------------------------------------ */
/*  postfx.save() / postfx.restore()  — snapshot effect stack          */
/* ------------------------------------------------------------------ */

static dtr_postfx_entry_t s_saved_stack[DTR_POSTFX_MAX_STACK];
static int32_t            s_saved_count = 0;

static JSValue js_postfx_save(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_postfx_t *pfx;

    (void)this_val;
    (void)argc;
    (void)argv;
    pfx = PFX(ctx);

    s_saved_count = pfx->count;
    SDL_memcpy(s_saved_stack, pfx->stack,
               (size_t)pfx->count * sizeof(dtr_postfx_entry_t));
    return JS_UNDEFINED;
}

static JSValue
js_postfx_restore(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_postfx_t *pfx;

    (void)this_val;
    (void)argc;
    (void)argv;
    pfx = PFX(ctx);

    pfx->count = s_saved_count;
    SDL_memcpy(pfx->stack, s_saved_stack,
               (size_t)s_saved_count * sizeof(dtr_postfx_entry_t));
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  Stubs for Phase 2                                                  */
/* ------------------------------------------------------------------ */

static JSValue
js_postfx_define(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    SDL_Log("postfx.define: custom effects not available in Phase 1");
    return JS_FALSE;
}

static JSValue
js_postfx_useShader(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    SDL_Log("postfx.useShader: not implemented in Phase 1");
    return JS_FALSE;
}

/* ---- Function list ---------------------------------------------------- */

static const JSCFunctionListEntry js_postfx_funcs[] = {
    JS_CFUNC_DEF("push", 2, js_postfx_push),
    JS_CFUNC_DEF("pop", 0, js_postfx_pop),
    JS_CFUNC_DEF("clear", 0, js_postfx_clear),
    JS_CFUNC_DEF("set", 3, js_postfx_set),
    JS_CFUNC_DEF("use", 1, js_postfx_use),
    JS_CFUNC_DEF("stack", 1, js_postfx_stack),
    JS_CFUNC_DEF("available", 0, js_postfx_available),
    JS_CFUNC_DEF("save", 0, js_postfx_save),
    JS_CFUNC_DEF("restore", 0, js_postfx_restore),
    JS_CFUNC_DEF("define", 2, js_postfx_define),
    JS_CFUNC_DEF("useShader", 1, js_postfx_useShader),
};

void dtr_postfx_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_postfx_funcs, countof(js_postfx_funcs));

    /* Effect ID constants */
    JS_SetPropertyStr(ctx, ns, "NONE", JS_NewInt32(ctx, DTR_POSTFX_NONE));
    JS_SetPropertyStr(ctx, ns, "CRT", JS_NewInt32(ctx, DTR_POSTFX_CRT));
    JS_SetPropertyStr(ctx, ns, "SCANLINES", JS_NewInt32(ctx, DTR_POSTFX_SCANLINES));
    JS_SetPropertyStr(ctx, ns, "BLOOM", JS_NewInt32(ctx, DTR_POSTFX_BLOOM));
    JS_SetPropertyStr(ctx, ns, "ABERRATION", JS_NewInt32(ctx, DTR_POSTFX_ABERRATION));

    JS_SetPropertyStr(ctx, global, "postfx", ns);
}
