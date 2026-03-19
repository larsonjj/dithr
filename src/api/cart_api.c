/**
 * \file            cart_api.c
 * \brief           JS cart namespace — persistence and cart metadata
 */

#include "../cart.h"
#include "api_common.h"

#define CART(ctx) (dtr_api_get_console(ctx)->cart)

/* ---- Persistence (string key-value) ----------------------------------- */

static JSValue js_cart_save(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *key;
    const char *value;

    (void)this_val;
    if (argc < 2) {
        return JS_UNDEFINED;
    }

    key   = JS_ToCString(ctx, argv[0]);
    value = JS_ToCString(ctx, argv[1]);

    if (key != NULL && value != NULL) {
        dtr_cart_save(CART(ctx), key, value);
    }

    if (key != NULL) {
        JS_FreeCString(ctx, key);
    }
    if (value != NULL) {
        JS_FreeCString(ctx, value);
    }
    return JS_UNDEFINED;
}

static JSValue js_cart_load(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *key;
    const char *result;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    key = JS_ToCString(ctx, argv[0]);
    if (key == NULL) {
        return JS_UNDEFINED;
    }

    result = dtr_cart_load_key(CART(ctx), key);
    JS_FreeCString(ctx, key);

    if (result != NULL) {
        return JS_NewString(ctx, result);
    }
    return JS_UNDEFINED;
}

static JSValue js_cart_has(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *key;
    bool        result;

    (void)this_val;
    if (argc < 1) {
        return JS_FALSE;
    }

    key = JS_ToCString(ctx, argv[0]);
    if (key == NULL) {
        return JS_FALSE;
    }

    result = dtr_cart_has_key(CART(ctx), key);
    JS_FreeCString(ctx, key);
    return JS_NewBool(ctx, result);
}

static JSValue js_cart_delete(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *key;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    key = JS_ToCString(ctx, argv[0]);
    if (key == NULL) {
        return JS_UNDEFINED;
    }

    dtr_cart_delete_key(CART(ctx), key);
    JS_FreeCString(ctx, key);
    return JS_UNDEFINED;
}

/* ---- Data slots (numeric) --------------------------------------------- */

static JSValue js_cart_dset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t slot;
    double  value;

    (void)this_val;
    slot  = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    value = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);

    dtr_cart_dset(CART(ctx), slot, value);
    return JS_UNDEFINED;
}

static JSValue js_cart_dget(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t slot;

    (void)this_val;
    slot = dtr_api_opt_int(ctx, argc, argv, 0, 0);
    return JS_NewFloat64(ctx, dtr_cart_dget(CART(ctx), slot));
}

/* ---- Meta queries ----------------------------------------------------- */

static JSValue js_cart_title(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewString(ctx, CART(ctx)->meta.title);
}

static JSValue js_cart_author(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewString(ctx, CART(ctx)->meta.author);
}

static JSValue js_cart_version(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewString(ctx, CART(ctx)->meta.version);
}

static const JSCFunctionListEntry js_cart_funcs[] = {
    JS_CFUNC_DEF("save", 2, js_cart_save),
    JS_CFUNC_DEF("load", 1, js_cart_load),
    JS_CFUNC_DEF("has", 1, js_cart_has),
    JS_CFUNC_DEF("delete", 1, js_cart_delete),
    JS_CFUNC_DEF("dset", 2, js_cart_dset),
    JS_CFUNC_DEF("dget", 1, js_cart_dget),
    JS_CFUNC_DEF("title", 0, js_cart_title),
    JS_CFUNC_DEF("author", 0, js_cart_author),
    JS_CFUNC_DEF("version", 0, js_cart_version),
};

void dtr_cart_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_cart_funcs, countof(js_cart_funcs));
    JS_SetPropertyStr(ctx, global, "cart", ns);
}
