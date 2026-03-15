/**
 * \file            api_common.h
 * \brief           Shared helpers for JS API bindings
 */

#ifndef MVN_API_COMMON_H
#define MVN_API_COMMON_H

#include "../console.h"
#include "../runtime.h"
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \brief           Retrieve the console pointer from a JS context
 */
static inline mvn_console_t *mvn_api_get_console(JSContext *ctx)
{
    return (mvn_console_t *)JS_GetContextOpaque(ctx);
}

/**
 * \brief           Read an optional int32 argument, returning dflt if absent
 */
static inline int32_t
mvn_api_opt_int(JSContext *ctx, int argc, JSValueConst *argv, int idx, int32_t dflt)
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
mvn_api_opt_float(JSContext *ctx, int argc, JSValueConst *argv, int idx, double dflt)
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
static inline const char *mvn_api_get_string(JSContext *ctx, JSValueConst val)
{
    return JS_ToCString(ctx, val);
}

/* Per-namespace registration functions */
void mvn_gfx_api_register(JSContext *ctx, JSValue global);
void mvn_map_api_register(JSContext *ctx, JSValue global);
void mvn_sfx_api_register(JSContext *ctx, JSValue global);
void mvn_mus_api_register(JSContext *ctx, JSValue global);
void mvn_key_api_register(JSContext *ctx, JSValue global);
void mvn_mouse_api_register(JSContext *ctx, JSValue global);
void mvn_pad_api_register(JSContext *ctx, JSValue global);
void mvn_input_api_register(JSContext *ctx, JSValue global);
void mvn_evt_api_register(JSContext *ctx, JSValue global);
void mvn_postfx_api_register(JSContext *ctx, JSValue global);
void mvn_math_api_register(JSContext *ctx, JSValue global);
void mvn_cart_api_register(JSContext *ctx, JSValue global);
void mvn_sys_api_register(JSContext *ctx, JSValue global);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MVN_API_COMMON_H */
