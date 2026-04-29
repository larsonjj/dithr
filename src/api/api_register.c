/**
 * \file            api_register.c
 * \brief           Master registration — wires all API namespaces
 */

#include "api_common.h"

void dtr_api_register_all(dtr_runtime_t *rt)
{
    JSContext *ctx;
    JSValue    global;

    ctx    = rt->ctx;
    global = JS_GetGlobalObject(ctx);

    dtr_gfx_api_register(ctx, global);
    dtr_map_api_register(ctx, global);
    dtr_sfx_api_register(ctx, global);
    dtr_mus_api_register(ctx, global);
    dtr_key_api_register(ctx, global);
    dtr_mouse_api_register(ctx, global);
    dtr_pad_api_register(ctx, global);
    dtr_input_api_register(ctx, global);
    dtr_evt_api_register(ctx, global);
    dtr_postfx_api_register(ctx, global);
    dtr_math_api_register(ctx, global);
    dtr_cart_api_register(ctx, global);
    dtr_sys_api_register(ctx, global);
    dtr_col_api_register(ctx, global);
    dtr_cam_api_register(ctx, global);
    dtr_ui_api_register(ctx, global);
    dtr_tween_api_register(ctx, global);
    dtr_synth_api_register(ctx, global);
    dtr_touch_api_register(ctx, global);
    dtr_res_api_register(ctx, global);

    JS_FreeValue(ctx, global);
}
