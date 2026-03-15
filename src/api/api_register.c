/**
 * \file            api_register.c
 * \brief           Master registration — wires all API namespaces
 */

#include "api_common.h"

void mvn_api_register_all(mvn_runtime_t *rt)
{
    JSContext *ctx;
    JSValue    global;

    ctx    = rt->ctx;
    global = JS_GetGlobalObject(ctx);

    mvn_gfx_api_register(ctx, global);
    mvn_map_api_register(ctx, global);
    mvn_sfx_api_register(ctx, global);
    mvn_mus_api_register(ctx, global);
    mvn_key_api_register(ctx, global);
    mvn_mouse_api_register(ctx, global);
    mvn_pad_api_register(ctx, global);
    mvn_input_api_register(ctx, global);
    mvn_evt_api_register(ctx, global);
    mvn_postfx_api_register(ctx, global);
    mvn_math_api_register(ctx, global);
    mvn_cart_api_register(ctx, global);
    mvn_sys_api_register(ctx, global);

    JS_FreeValue(ctx, global);
}
