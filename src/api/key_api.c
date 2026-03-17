/**
 * \file            key_api.c
 * \brief           JS key namespace — keyboard state queries
 */

#include "../input.h"
#include "api_common.h"

#define KEYS(ctx) (mvn_api_get_console(ctx)->keys)

static JSValue js_key_btn(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx,
                      mvn_key_btn(KEYS(ctx), (mvn_key_t)mvn_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_key_btnp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx,
                      mvn_key_btnp(KEYS(ctx), (mvn_key_t)mvn_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_key_name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewString(ctx, mvn_key_name((mvn_key_t)mvn_api_opt_int(ctx, argc, argv, 0, 0)));
}

/* ---- Key constant properties ------------------------------------------ */

static const JSCFunctionListEntry js_key_funcs[] = {
    JS_CFUNC_DEF("btn", 1, js_key_btn),
    JS_CFUNC_DEF("btnp", 1, js_key_btnp),
    JS_CFUNC_DEF("name", 1, js_key_name),
};

void mvn_key_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_key_funcs, countof(js_key_funcs));

    /* Expose key constants: key.UP, key.DOWN, etc. */
    JS_SetPropertyStr(ctx, ns, "UP", JS_NewInt32(ctx, MVN_KEY_UP));
    JS_SetPropertyStr(ctx, ns, "DOWN", JS_NewInt32(ctx, MVN_KEY_DOWN));
    JS_SetPropertyStr(ctx, ns, "LEFT", JS_NewInt32(ctx, MVN_KEY_LEFT));
    JS_SetPropertyStr(ctx, ns, "RIGHT", JS_NewInt32(ctx, MVN_KEY_RIGHT));
    JS_SetPropertyStr(ctx, ns, "Z", JS_NewInt32(ctx, MVN_KEY_Z));
    JS_SetPropertyStr(ctx, ns, "X", JS_NewInt32(ctx, MVN_KEY_X));
    JS_SetPropertyStr(ctx, ns, "C", JS_NewInt32(ctx, MVN_KEY_C));
    JS_SetPropertyStr(ctx, ns, "V", JS_NewInt32(ctx, MVN_KEY_V));
    JS_SetPropertyStr(ctx, ns, "SPACE", JS_NewInt32(ctx, MVN_KEY_SPACE));
    JS_SetPropertyStr(ctx, ns, "ENTER", JS_NewInt32(ctx, MVN_KEY_ENTER));
    JS_SetPropertyStr(ctx, ns, "ESCAPE", JS_NewInt32(ctx, MVN_KEY_ESCAPE));
    JS_SetPropertyStr(ctx, ns, "LSHIFT", JS_NewInt32(ctx, MVN_KEY_LSHIFT));
    JS_SetPropertyStr(ctx, ns, "RSHIFT", JS_NewInt32(ctx, MVN_KEY_RSHIFT));
    JS_SetPropertyStr(ctx, ns, "A", JS_NewInt32(ctx, MVN_KEY_A));
    JS_SetPropertyStr(ctx, ns, "B", JS_NewInt32(ctx, MVN_KEY_B));
    JS_SetPropertyStr(ctx, ns, "D", JS_NewInt32(ctx, MVN_KEY_D));
    JS_SetPropertyStr(ctx, ns, "E", JS_NewInt32(ctx, MVN_KEY_E));
    JS_SetPropertyStr(ctx, ns, "F", JS_NewInt32(ctx, MVN_KEY_F));
    JS_SetPropertyStr(ctx, ns, "S", JS_NewInt32(ctx, MVN_KEY_S));
    JS_SetPropertyStr(ctx, ns, "W", JS_NewInt32(ctx, MVN_KEY_W));
    JS_SetPropertyStr(ctx, ns, "F1", JS_NewInt32(ctx, MVN_KEY_F1));
    JS_SetPropertyStr(ctx, ns, "F2", JS_NewInt32(ctx, MVN_KEY_F2));

    JS_SetPropertyStr(ctx, global, "key", ns);
}
