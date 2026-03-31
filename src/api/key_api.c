/**
 * \file            key_api.c
 * \brief           JS key namespace — keyboard state queries
 */

#include "../input.h"
#include "api_common.h"

#define KEYS(ctx) (dtr_api_get_console(ctx)->keys)

static JSValue js_key_btn(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx,
                      dtr_key_btn(KEYS(ctx), (dtr_key_t)dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_key_btnp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx,
                      dtr_key_btnp(KEYS(ctx), (dtr_key_t)dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_key_btnr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx,
                      dtr_key_btnr(KEYS(ctx), (dtr_key_t)dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_key_name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewString(ctx, dtr_key_name((dtr_key_t)dtr_api_opt_int(ctx, argc, argv, 0, 0)));
}

/* ---- Key constant properties ------------------------------------------ */

static const JSCFunctionListEntry js_key_funcs[] = {
    JS_CFUNC_DEF("btn", 1, js_key_btn),
    JS_CFUNC_DEF("btnp", 1, js_key_btnp),
    JS_CFUNC_DEF("btnr", 1, js_key_btnr),
    JS_CFUNC_DEF("name", 1, js_key_name),
};

void dtr_key_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_key_funcs, countof(js_key_funcs));

    /* Expose key constants: key.UP, key.DOWN, etc. */
    JS_SetPropertyStr(ctx, ns, "UP", JS_NewInt32(ctx, DTR_KEY_UP));
    JS_SetPropertyStr(ctx, ns, "DOWN", JS_NewInt32(ctx, DTR_KEY_DOWN));
    JS_SetPropertyStr(ctx, ns, "LEFT", JS_NewInt32(ctx, DTR_KEY_LEFT));
    JS_SetPropertyStr(ctx, ns, "RIGHT", JS_NewInt32(ctx, DTR_KEY_RIGHT));
    JS_SetPropertyStr(ctx, ns, "Z", JS_NewInt32(ctx, DTR_KEY_Z));
    JS_SetPropertyStr(ctx, ns, "X", JS_NewInt32(ctx, DTR_KEY_X));
    JS_SetPropertyStr(ctx, ns, "C", JS_NewInt32(ctx, DTR_KEY_C));
    JS_SetPropertyStr(ctx, ns, "V", JS_NewInt32(ctx, DTR_KEY_V));
    JS_SetPropertyStr(ctx, ns, "SPACE", JS_NewInt32(ctx, DTR_KEY_SPACE));
    JS_SetPropertyStr(ctx, ns, "ENTER", JS_NewInt32(ctx, DTR_KEY_ENTER));
    JS_SetPropertyStr(ctx, ns, "ESCAPE", JS_NewInt32(ctx, DTR_KEY_ESCAPE));
    JS_SetPropertyStr(ctx, ns, "LSHIFT", JS_NewInt32(ctx, DTR_KEY_LSHIFT));
    JS_SetPropertyStr(ctx, ns, "RSHIFT", JS_NewInt32(ctx, DTR_KEY_RSHIFT));
    JS_SetPropertyStr(ctx, ns, "A", JS_NewInt32(ctx, DTR_KEY_A));
    JS_SetPropertyStr(ctx, ns, "B", JS_NewInt32(ctx, DTR_KEY_B));
    JS_SetPropertyStr(ctx, ns, "D", JS_NewInt32(ctx, DTR_KEY_D));
    JS_SetPropertyStr(ctx, ns, "E", JS_NewInt32(ctx, DTR_KEY_E));
    JS_SetPropertyStr(ctx, ns, "F", JS_NewInt32(ctx, DTR_KEY_F));
    JS_SetPropertyStr(ctx, ns, "G", JS_NewInt32(ctx, DTR_KEY_G));
    JS_SetPropertyStr(ctx, ns, "H", JS_NewInt32(ctx, DTR_KEY_H));
    JS_SetPropertyStr(ctx, ns, "I", JS_NewInt32(ctx, DTR_KEY_I));
    JS_SetPropertyStr(ctx, ns, "J", JS_NewInt32(ctx, DTR_KEY_J));
    JS_SetPropertyStr(ctx, ns, "K", JS_NewInt32(ctx, DTR_KEY_K));
    JS_SetPropertyStr(ctx, ns, "L", JS_NewInt32(ctx, DTR_KEY_L));
    JS_SetPropertyStr(ctx, ns, "M", JS_NewInt32(ctx, DTR_KEY_M));
    JS_SetPropertyStr(ctx, ns, "N", JS_NewInt32(ctx, DTR_KEY_N));
    JS_SetPropertyStr(ctx, ns, "O", JS_NewInt32(ctx, DTR_KEY_O));
    JS_SetPropertyStr(ctx, ns, "P", JS_NewInt32(ctx, DTR_KEY_P));
    JS_SetPropertyStr(ctx, ns, "Q", JS_NewInt32(ctx, DTR_KEY_Q));
    JS_SetPropertyStr(ctx, ns, "R", JS_NewInt32(ctx, DTR_KEY_R));
    JS_SetPropertyStr(ctx, ns, "S", JS_NewInt32(ctx, DTR_KEY_S));
    JS_SetPropertyStr(ctx, ns, "T", JS_NewInt32(ctx, DTR_KEY_T));
    JS_SetPropertyStr(ctx, ns, "U", JS_NewInt32(ctx, DTR_KEY_U));
    JS_SetPropertyStr(ctx, ns, "W", JS_NewInt32(ctx, DTR_KEY_W));
    JS_SetPropertyStr(ctx, ns, "Y", JS_NewInt32(ctx, DTR_KEY_Y));
    JS_SetPropertyStr(ctx, ns, "NUM0", JS_NewInt32(ctx, DTR_KEY_0));
    JS_SetPropertyStr(ctx, ns, "NUM1", JS_NewInt32(ctx, DTR_KEY_1));
    JS_SetPropertyStr(ctx, ns, "NUM2", JS_NewInt32(ctx, DTR_KEY_2));
    JS_SetPropertyStr(ctx, ns, "NUM3", JS_NewInt32(ctx, DTR_KEY_3));
    JS_SetPropertyStr(ctx, ns, "NUM4", JS_NewInt32(ctx, DTR_KEY_4));
    JS_SetPropertyStr(ctx, ns, "NUM5", JS_NewInt32(ctx, DTR_KEY_5));
    JS_SetPropertyStr(ctx, ns, "NUM6", JS_NewInt32(ctx, DTR_KEY_6));
    JS_SetPropertyStr(ctx, ns, "NUM7", JS_NewInt32(ctx, DTR_KEY_7));
    JS_SetPropertyStr(ctx, ns, "NUM8", JS_NewInt32(ctx, DTR_KEY_8));
    JS_SetPropertyStr(ctx, ns, "NUM9", JS_NewInt32(ctx, DTR_KEY_9));
    JS_SetPropertyStr(ctx, ns, "F1", JS_NewInt32(ctx, DTR_KEY_F1));
    JS_SetPropertyStr(ctx, ns, "F2", JS_NewInt32(ctx, DTR_KEY_F2));
    JS_SetPropertyStr(ctx, ns, "F3", JS_NewInt32(ctx, DTR_KEY_F3));
    JS_SetPropertyStr(ctx, ns, "F4", JS_NewInt32(ctx, DTR_KEY_F4));
    JS_SetPropertyStr(ctx, ns, "F5", JS_NewInt32(ctx, DTR_KEY_F5));
    JS_SetPropertyStr(ctx, ns, "F6", JS_NewInt32(ctx, DTR_KEY_F6));
    JS_SetPropertyStr(ctx, ns, "F7", JS_NewInt32(ctx, DTR_KEY_F7));
    JS_SetPropertyStr(ctx, ns, "F8", JS_NewInt32(ctx, DTR_KEY_F8));
    JS_SetPropertyStr(ctx, ns, "F9", JS_NewInt32(ctx, DTR_KEY_F9));
    JS_SetPropertyStr(ctx, ns, "F10", JS_NewInt32(ctx, DTR_KEY_F10));
    JS_SetPropertyStr(ctx, ns, "F11", JS_NewInt32(ctx, DTR_KEY_F11));
    JS_SetPropertyStr(ctx, ns, "F12", JS_NewInt32(ctx, DTR_KEY_F12));
    JS_SetPropertyStr(ctx, ns, "BACKSPACE", JS_NewInt32(ctx, DTR_KEY_BACKSPACE));
    JS_SetPropertyStr(ctx, ns, "DELETE", JS_NewInt32(ctx, DTR_KEY_DELETE));
    JS_SetPropertyStr(ctx, ns, "TAB", JS_NewInt32(ctx, DTR_KEY_TAB));
    JS_SetPropertyStr(ctx, ns, "HOME", JS_NewInt32(ctx, DTR_KEY_HOME));
    JS_SetPropertyStr(ctx, ns, "END", JS_NewInt32(ctx, DTR_KEY_END));
    JS_SetPropertyStr(ctx, ns, "PAGEUP", JS_NewInt32(ctx, DTR_KEY_PAGEUP));
    JS_SetPropertyStr(ctx, ns, "PAGEDOWN", JS_NewInt32(ctx, DTR_KEY_PAGEDOWN));
    JS_SetPropertyStr(ctx, ns, "LCTRL", JS_NewInt32(ctx, DTR_KEY_LCTRL));
    JS_SetPropertyStr(ctx, ns, "RCTRL", JS_NewInt32(ctx, DTR_KEY_RCTRL));
    JS_SetPropertyStr(ctx, ns, "LALT", JS_NewInt32(ctx, DTR_KEY_LALT));
    JS_SetPropertyStr(ctx, ns, "RALT", JS_NewInt32(ctx, DTR_KEY_RALT));
    JS_SetPropertyStr(ctx, ns, "GRAVE", JS_NewInt32(ctx, DTR_KEY_GRAVE));

    JS_SetPropertyStr(ctx, global, "key", ns);
}
