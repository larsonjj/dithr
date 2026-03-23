/**
 * \file            cam_api.c
 * \brief           JS camera helpers — set, get, reset, follow
 */

#include "api_common.h"
#include "../graphics.h"

/* ------------------------------------------------------------------ */
/*  cam.set(x, y) — set the camera position                           */
/* ------------------------------------------------------------------ */

static JSValue js_cam_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    dtr_gfx_camera(con->graphics,
                    dtr_api_opt_int(ctx, argc, argv, 0, 0),
                    dtr_api_opt_int(ctx, argc, argv, 1, 0));
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  cam.get() — return {x, y}                                          */
/* ------------------------------------------------------------------ */

static JSValue js_cam_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    JSValue        obj;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);
    obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, con->graphics->camera_x));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, con->graphics->camera_y));
    return obj;
}

/* ------------------------------------------------------------------ */
/*  cam.reset() — reset camera to (0, 0)                               */
/* ------------------------------------------------------------------ */

static JSValue js_cam_reset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);
    dtr_gfx_camera(con->graphics, 0, 0);
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  cam.follow(tx, ty, speed) — lerp camera toward target              */
/* ------------------------------------------------------------------ */

static JSValue js_cam_follow(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    double         tx;
    double         ty;
    double         speed;
    double         cx;
    double         cy;

    (void)this_val;
    con   = dtr_api_get_console(ctx);
    tx    = dtr_api_opt_float(ctx, argc, argv, 0, 0.0);
    ty    = dtr_api_opt_float(ctx, argc, argv, 1, 0.0);
    speed = dtr_api_opt_float(ctx, argc, argv, 2, 0.1);

    cx = (double)con->graphics->camera_x + (tx - (double)con->graphics->camera_x) * speed;
    cy = (double)con->graphics->camera_y + (ty - (double)con->graphics->camera_y) * speed;

    dtr_gfx_camera(con->graphics, (int32_t)cx, (int32_t)cy);
    return JS_UNDEFINED;
}

/* ---- Function list ---------------------------------------------------- */

static const JSCFunctionListEntry js_cam_funcs[] = {
    JS_CFUNC_DEF("set", 2, js_cam_set),
    JS_CFUNC_DEF("get", 0, js_cam_get),
    JS_CFUNC_DEF("reset", 0, js_cam_reset),
    JS_CFUNC_DEF("follow", 3, js_cam_follow),
};

void dtr_cam_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_cam_funcs, countof(js_cam_funcs));
    JS_SetPropertyStr(ctx, global, "cam", ns);
}
