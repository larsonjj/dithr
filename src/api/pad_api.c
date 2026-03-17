/**
 * \file            pad_api.c
 * \brief           JS pad namespace — gamepad state queries
 */

#include "../gamepad.h"
#include "api_common.h"

#define PADS(ctx) (mvn_api_get_console(ctx)->gamepads)

static JSValue js_pad_btn(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx,
                      mvn_gamepad_btn(PADS(ctx),
                                      (mvn_pad_btn_t)mvn_api_opt_int(ctx, argc, argv, 0, 0),
                                      mvn_api_opt_int(ctx, argc, argv, 1, 0)));
}

static JSValue js_pad_btnp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx,
                      mvn_gamepad_btnp(PADS(ctx),
                                       (mvn_pad_btn_t)mvn_api_opt_int(ctx, argc, argv, 0, 0),
                                       mvn_api_opt_int(ctx, argc, argv, 1, 0)));
}

static JSValue js_pad_axis(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewFloat64(ctx,
                         mvn_gamepad_axis(PADS(ctx),
                                          (mvn_pad_axis_t)mvn_api_opt_int(ctx, argc, argv, 0, 0),
                                          mvn_api_opt_int(ctx, argc, argv, 1, 0)));
}

static JSValue js_pad_connected(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewBool(ctx,
                      mvn_gamepad_connected(PADS(ctx), mvn_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_pad_count(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, mvn_gamepad_count(PADS(ctx)));
}

static JSValue js_pad_name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return JS_NewString(ctx, mvn_gamepad_name(PADS(ctx), mvn_api_opt_int(ctx, argc, argv, 0, 0)));
}

static JSValue js_pad_deadzone(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t index;

    (void)this_val;
    index = mvn_api_opt_int(ctx, argc, argv, 1, 0);

    if (argc >= 1 && !JS_IsUndefined(argv[0])) {
        mvn_gamepad_set_deadzone(
            PADS(ctx), (float)mvn_api_opt_float(ctx, argc, argv, 0, 0.15), index);
    }
    return JS_NewFloat64(ctx, mvn_gamepad_get_deadzone(PADS(ctx), index));
}

static JSValue js_pad_rumble(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    mvn_gamepad_rumble(PADS(ctx),
                       mvn_api_opt_int(ctx, argc, argv, 0, 0),
                       (uint16_t)mvn_api_opt_int(ctx, argc, argv, 1, 0xFFFF),
                       (uint16_t)mvn_api_opt_int(ctx, argc, argv, 2, 0xFFFF),
                       (uint32_t)mvn_api_opt_int(ctx, argc, argv, 3, 200));
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_pad_funcs[] = {
    JS_CFUNC_DEF("btn", 2, js_pad_btn),
    JS_CFUNC_DEF("btnp", 2, js_pad_btnp),
    JS_CFUNC_DEF("axis", 2, js_pad_axis),
    JS_CFUNC_DEF("connected", 1, js_pad_connected),
    JS_CFUNC_DEF("count", 0, js_pad_count),
    JS_CFUNC_DEF("name", 1, js_pad_name),
    JS_CFUNC_DEF("deadzone", 2, js_pad_deadzone),
    JS_CFUNC_DEF("rumble", 4, js_pad_rumble),
};

void mvn_pad_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_pad_funcs, countof(js_pad_funcs));

    /* Button constants */
    JS_SetPropertyStr(ctx, ns, "UP", JS_NewInt32(ctx, MVN_PAD_UP));
    JS_SetPropertyStr(ctx, ns, "DOWN", JS_NewInt32(ctx, MVN_PAD_DOWN));
    JS_SetPropertyStr(ctx, ns, "LEFT", JS_NewInt32(ctx, MVN_PAD_LEFT));
    JS_SetPropertyStr(ctx, ns, "RIGHT", JS_NewInt32(ctx, MVN_PAD_RIGHT));
    JS_SetPropertyStr(ctx, ns, "A", JS_NewInt32(ctx, MVN_PAD_A));
    JS_SetPropertyStr(ctx, ns, "B", JS_NewInt32(ctx, MVN_PAD_B));
    JS_SetPropertyStr(ctx, ns, "X", JS_NewInt32(ctx, MVN_PAD_X));
    JS_SetPropertyStr(ctx, ns, "Y", JS_NewInt32(ctx, MVN_PAD_Y));
    JS_SetPropertyStr(ctx, ns, "L1", JS_NewInt32(ctx, MVN_PAD_L1));
    JS_SetPropertyStr(ctx, ns, "R1", JS_NewInt32(ctx, MVN_PAD_R1));
    JS_SetPropertyStr(ctx, ns, "L2", JS_NewInt32(ctx, MVN_PAD_L2));
    JS_SetPropertyStr(ctx, ns, "R2", JS_NewInt32(ctx, MVN_PAD_R2));
    JS_SetPropertyStr(ctx, ns, "START", JS_NewInt32(ctx, MVN_PAD_START));
    JS_SetPropertyStr(ctx, ns, "SELECT", JS_NewInt32(ctx, MVN_PAD_SELECT));

    /* Axis constants */
    JS_SetPropertyStr(ctx, ns, "LX", JS_NewInt32(ctx, MVN_PAD_AXIS_LX));
    JS_SetPropertyStr(ctx, ns, "LY", JS_NewInt32(ctx, MVN_PAD_AXIS_LY));
    JS_SetPropertyStr(ctx, ns, "RX", JS_NewInt32(ctx, MVN_PAD_AXIS_RX));
    JS_SetPropertyStr(ctx, ns, "RY", JS_NewInt32(ctx, MVN_PAD_AXIS_RY));
    JS_SetPropertyStr(ctx, ns, "TL", JS_NewInt32(ctx, MVN_PAD_AXIS_L2));
    JS_SetPropertyStr(ctx, ns, "TR", JS_NewInt32(ctx, MVN_PAD_AXIS_R2));

    JS_SetPropertyStr(ctx, global, "pad", ns);
}
