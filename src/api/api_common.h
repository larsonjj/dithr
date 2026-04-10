/**
 * \file            api_common.h
 * \brief           Shared helpers for JS API bindings
 */

#ifndef DTR_API_COMMON_H
#define DTR_API_COMMON_H

#include "../console.h"
#include "../runtime.h"
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \brief           Retrieve the console pointer from a JS context
 */
static inline dtr_console_t *dtr_api_get_console(JSContext *ctx)
{
    return (dtr_console_t *)JS_GetContextOpaque(ctx);
}

/**
 * \brief           Read an optional int32 argument, returning dflt if absent
 */
static inline int32_t
dtr_api_opt_int(JSContext *ctx, int argc, JSValueConst *argv, int idx, int32_t dflt)
{
    int32_t val;

    if (idx >= argc) {
        return dflt;
    }
    if (JS_ToInt32(ctx, &val, argv[idx]) != 0) {
        return dflt;
    }
    return val;
}

/**
 * \brief           Read an optional float argument
 */
static inline double
dtr_api_opt_float(JSContext *ctx, int argc, JSValueConst *argv, int idx, double dflt)
{
    double val;

    if (idx >= argc) {
        return dflt;
    }
    if (JS_ToFloat64(ctx, &val, argv[idx]) != 0) {
        return dflt;
    }
    return val;
}

/**
 * \brief           Read a required string argument (must JS_FreeCString)
 */
static inline const char *dtr_api_get_string(JSContext *ctx, JSValueConst val)
{
    return JS_ToCString(ctx, val);
}

/* Per-namespace registration functions */
void dtr_gfx_api_register(JSContext *ctx, JSValue global);
void dtr_map_api_register(JSContext *ctx, JSValue global);
void dtr_sfx_api_register(JSContext *ctx, JSValue global);
void dtr_mus_api_register(JSContext *ctx, JSValue global);
void dtr_key_api_register(JSContext *ctx, JSValue global);
void dtr_mouse_api_register(JSContext *ctx, JSValue global);
void dtr_pad_api_register(JSContext *ctx, JSValue global);
void dtr_input_api_register(JSContext *ctx, JSValue global);
void dtr_evt_api_register(JSContext *ctx, JSValue global);
void dtr_postfx_api_register(JSContext *ctx, JSValue global);
void dtr_math_api_register(JSContext *ctx, JSValue global);
void dtr_cart_api_register(JSContext *ctx, JSValue global);
void dtr_sys_api_register(JSContext *ctx, JSValue global);
void dtr_col_api_register(JSContext *ctx, JSValue global);
void dtr_cam_api_register(JSContext *ctx, JSValue global);
void dtr_ui_api_register(JSContext *ctx, JSValue global);
void dtr_tween_api_register(JSContext *ctx, JSValue global);
void dtr_synth_api_register(JSContext *ctx, JSValue global);
void dtr_touch_api_register(JSContext *ctx, JSValue global);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_API_COMMON_H */
