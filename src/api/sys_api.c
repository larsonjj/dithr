/**
 * \file            sys_api.c
 * \brief           JS sys namespace — system info, timing, debug
 */

#include "api_common.h"

static JSValue js_sys_fps(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    mvn_console_t *con;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = mvn_api_get_console(ctx);
    return JS_NewFloat64(ctx, 1.0 / (con->delta > 0.0f ? con->delta : 0.016f));
}

static JSValue js_sys_dt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewFloat64(ctx, mvn_api_get_console(ctx)->delta);
}

static JSValue js_sys_time(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewFloat64(ctx, (double)SDL_GetTicks() / 1000.0);
}

static JSValue js_sys_ticks(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewFloat64(ctx, (double)SDL_GetTicks());
}

static JSValue js_sys_screenW(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, mvn_api_get_console(ctx)->fb_width);
}

static JSValue js_sys_screenH(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, mvn_api_get_console(ctx)->fb_height);
}

static JSValue js_sys_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *msg;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    msg = JS_ToCString(ctx, argv[0]);
    if (msg != NULL) {
        SDL_Log("[cart] %s", msg);
        JS_FreeCString(ctx, msg);
    }
    return JS_UNDEFINED;
}

static JSValue js_sys_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    mvn_api_get_console(ctx)->running = false;
    return JS_UNDEFINED;
}

static JSValue js_sys_pause(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    mvn_api_get_console(ctx)->paused = true;
    return JS_UNDEFINED;
}

static JSValue js_sys_resume(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    mvn_api_get_console(ctx)->paused = false;
    return JS_UNDEFINED;
}

static JSValue
js_sys_fullscreen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    mvn_console_t *con;
    bool           fs;

    (void)this_val;
    con = mvn_api_get_console(ctx);

    if (argc >= 1) {
        fs = JS_ToBool(ctx, argv[0]);
        SDL_SetWindowFullscreen(con->window, fs);
    }
    return JS_NewBool(ctx, (SDL_GetWindowFlags(con->window) & SDL_WINDOW_FULLSCREEN) != 0);
}

static JSValue js_sys_title(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    mvn_console_t *con;

    (void)this_val;
    con = mvn_api_get_console(ctx);

    if (argc >= 1) {
        const char *title;

        title = JS_ToCString(ctx, argv[0]);
        if (title != NULL) {
            SDL_SetWindowTitle(con->window, title);
            JS_FreeCString(ctx, title);
        }
    }
    return JS_NewString(ctx, SDL_GetWindowTitle(con->window));
}

static const JSCFunctionListEntry js_sys_funcs[] = {
    JS_CFUNC_DEF("fps", 0, js_sys_fps),
    JS_CFUNC_DEF("dt", 0, js_sys_dt),
    JS_CFUNC_DEF("time", 0, js_sys_time),
    JS_CFUNC_DEF("ticks", 0, js_sys_ticks),
    JS_CFUNC_DEF("screenW", 0, js_sys_screenW),
    JS_CFUNC_DEF("screenH", 0, js_sys_screenH),
    JS_CFUNC_DEF("log", 1, js_sys_log),
    JS_CFUNC_DEF("exit", 0, js_sys_exit),
    JS_CFUNC_DEF("pause", 0, js_sys_pause),
    JS_CFUNC_DEF("resume", 0, js_sys_resume),
    JS_CFUNC_DEF("fullscreen", 1, js_sys_fullscreen),
    JS_CFUNC_DEF("title", 1, js_sys_title),
};

void mvn_sys_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_sys_funcs, countof(js_sys_funcs));
    JS_SetPropertyStr(ctx, global, "sys", ns);
}
