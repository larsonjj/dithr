/**
 * \file            evt_api.c
 * \brief           JS evt namespace — event bus on/off/once/emit
 */

#include "../event.h"
#include "api_common.h"

#define EVT(ctx) (mvn_api_get_console(ctx)->events)

static JSValue js_evt_on(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *name;
    int32_t     handle;

    (void)this_val;
    if (argc < 2 || !JS_IsFunction(ctx, argv[1])) {
        return JS_NewInt32(ctx, -1);
    }

    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_NewInt32(ctx, -1);
    }

    handle = mvn_event_on(EVT(ctx), name, argv[1]);
    JS_FreeCString(ctx, name);
    return JS_NewInt32(ctx, handle);
}

static JSValue js_evt_once(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *name;
    int32_t     handle;

    (void)this_val;
    if (argc < 2 || !JS_IsFunction(ctx, argv[1])) {
        return JS_NewInt32(ctx, -1);
    }

    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_NewInt32(ctx, -1);
    }

    handle = mvn_event_once(EVT(ctx), name, argv[1]);
    JS_FreeCString(ctx, name);
    return JS_NewInt32(ctx, handle);
}

static JSValue js_evt_off(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_event_off(EVT(ctx), mvn_api_opt_int(ctx, argc, argv, 0, -1));
    return JS_UNDEFINED;
}

static JSValue js_evt_emit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *name;
    JSValue     payload;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_UNDEFINED;
    }

    if (argc >= 2) {
        payload = JS_DupValue(ctx, argv[1]);
    } else {
        payload = JS_UNDEFINED;
    }

    mvn_event_emit(EVT(ctx), name, payload);
    JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_evt_funcs[] = {
    JS_CFUNC_DEF("on", 2, js_evt_on),
    JS_CFUNC_DEF("once", 2, js_evt_once),
    JS_CFUNC_DEF("off", 1, js_evt_off),
    JS_CFUNC_DEF("emit", 2, js_evt_emit),
};

void mvn_evt_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_evt_funcs, countof(js_evt_funcs));
    JS_SetPropertyStr(ctx, global, "evt", ns);
}
