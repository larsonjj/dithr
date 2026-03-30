/**
 * \file            sys_api.c
 * \brief           JS sys namespace — system info, timing, debug
 */

#include "api_common.h"
#include "../cart.h"
#include "../mouse.h"
#include "../audio.h"

/* ------------------------------------------------------------------ */
/*  Timing                                                             */
/* ------------------------------------------------------------------ */

static JSValue js_sys_time(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewFloat64(ctx, (double)dtr_api_get_console(ctx)->time);
}

static JSValue js_sys_delta(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewFloat64(ctx, (double)dtr_api_get_console(ctx)->delta);
}

static JSValue js_sys_frame(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewFloat64(ctx, (double)dtr_api_get_console(ctx)->frame_count);
}

static JSValue js_sys_fps(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);
    return JS_NewFloat64(ctx, 1.0 / (con->delta > 0.0f ? con->delta : 0.016f));
}

static JSValue
js_sys_target_fps(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, dtr_api_get_console(ctx)->target_fps);
}

static JSValue
js_sys_set_target_fps(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t fps;

    (void)this_val;
    fps = dtr_api_opt_int(ctx, argc, argv, 0, CONSOLE_TARGET_FPS);
    if (fps < 1) {
        fps = 1;
    }
    if (fps > 240) {
        fps = 240;
    }
    dtr_api_get_console(ctx)->target_fps = fps;
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  Display info                                                       */
/* ------------------------------------------------------------------ */

static JSValue js_sys_width(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, dtr_api_get_console(ctx)->fb_width);
}

static JSValue js_sys_height(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, dtr_api_get_console(ctx)->fb_height);
}

/* ------------------------------------------------------------------ */
/*  Identity                                                           */
/* ------------------------------------------------------------------ */

static JSValue js_sys_version(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewString(ctx, CONSOLE_VERSION);
}

static JSValue js_sys_platform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
#if defined(__EMSCRIPTEN__)
    return JS_NewString(ctx, "web");
#elif defined(_WIN32)
    return JS_NewString(ctx, "windows");
#elif defined(__APPLE__)
    return JS_NewString(ctx, "macos");
#elif defined(__linux__)
    return JS_NewString(ctx, "linux");
#else
    return JS_NewString(ctx, "unknown");
#endif
}

/* ------------------------------------------------------------------ */
/*  Logging                                                            */
/* ------------------------------------------------------------------ */

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

static JSValue js_sys_warn(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *msg;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    msg = JS_ToCString(ctx, argv[0]);
    if (msg != NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[cart] %s", msg);
        JS_FreeCString(ctx, msg);
    }
    return JS_UNDEFINED;
}

static JSValue js_sys_error(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *msg;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    msg = JS_ToCString(ctx, argv[0]);
    if (msg != NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[cart] %s", msg);
        JS_FreeCString(ctx, msg);
    }
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static JSValue js_sys_pause(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    dtr_api_get_console(ctx)->paused = true;
    return JS_UNDEFINED;
}

static JSValue js_sys_resume(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    dtr_api_get_console(ctx)->paused = false;
    return JS_UNDEFINED;
}

static JSValue js_sys_quit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    dtr_api_get_console(ctx)->running = false;
    return JS_UNDEFINED;
}

static JSValue js_sys_restart(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    (void)argc;
    (void)argv;
    con             = dtr_api_get_console(ctx);
    con->restart    = true;
    con->running    = false;
    return JS_UNDEFINED;
}

static JSValue js_sys_paused(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewBool(ctx, dtr_api_get_console(ctx)->paused);
}

static JSValue
js_sys_fullscreen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    bool           fs;

    (void)this_val;
    con = dtr_api_get_console(ctx);

    if (argc >= 1) {
        fs = JS_ToBool(ctx, argv[0]);
        SDL_SetWindowFullscreen(con->window, fs);
        con->fullscreen = fs;
    }
    return JS_NewBool(ctx, con->fullscreen);
}

static JSValue
js_sys_set_fullscreen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    bool           fs;

    (void)this_val;
    (void)argc;
    con = dtr_api_get_console(ctx);
    fs  = JS_ToBool(ctx, argv[0]);
    SDL_SetWindowFullscreen(con->window, fs);
    con->fullscreen = fs;
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  Config / limits                                                    */
/* ------------------------------------------------------------------ */

static JSValue js_sys_config(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    con = dtr_api_get_console(ctx);

    if (argc >= 1 && JS_IsString(argv[0])) {
        const char *path;
        JSValue     result;

        path   = JS_ToCString(ctx, argv[0]);
        result = JS_UNDEFINED;

        if (path != NULL) {
            if (SDL_strcmp(path, "display.width") == 0) {
                result = JS_NewInt32(ctx, con->cart->display.width);
            } else if (SDL_strcmp(path, "display.height") == 0) {
                result = JS_NewInt32(ctx, con->cart->display.height);
            } else if (SDL_strcmp(path, "display.scale") == 0) {
                result = JS_NewInt32(ctx, con->cart->display.scale);
            } else if (SDL_strcmp(path, "timing.fps") == 0) {
                result = JS_NewInt32(ctx, con->cart->timing.fps);
            } else if (SDL_strcmp(path, "sprites.tile_w") == 0) {
                result = JS_NewInt32(ctx, con->cart->sprites.tile_w);
            } else if (SDL_strcmp(path, "sprites.tile_h") == 0) {
                result = JS_NewInt32(ctx, con->cart->sprites.tile_h);
            } else if (SDL_strcmp(path, "audio.channels") == 0) {
                result = JS_NewInt32(ctx, con->cart->audio.channels);
            } else if (SDL_strcmp(path, "audio.frequency") == 0) {
                result = JS_NewInt32(ctx, con->cart->audio.frequency);
            }
            JS_FreeCString(ctx, path);
        }
        return result;
    }

    /* No path: return full config object */
    {
        JSValue obj;
        JSValue disp;
        JSValue tim;
        JSValue spr;
        JSValue aud;

        obj  = JS_NewObject(ctx);
        disp = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, disp, "width", JS_NewInt32(ctx, con->cart->display.width));
        JS_SetPropertyStr(ctx, disp, "height", JS_NewInt32(ctx, con->cart->display.height));
        JS_SetPropertyStr(ctx, disp, "scale", JS_NewInt32(ctx, con->cart->display.scale));
        JS_SetPropertyStr(ctx, obj, "display", disp);

        tim = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, tim, "fps", JS_NewInt32(ctx, con->cart->timing.fps));
        JS_SetPropertyStr(ctx, obj, "timing", tim);

        spr = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, spr, "tile_w", JS_NewInt32(ctx, con->cart->sprites.tile_w));
        JS_SetPropertyStr(ctx, spr, "tile_h", JS_NewInt32(ctx, con->cart->sprites.tile_h));
        JS_SetPropertyStr(ctx, obj, "sprites", spr);

        aud = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, aud, "channels", JS_NewInt32(ctx, con->cart->audio.channels));
        JS_SetPropertyStr(ctx, aud, "frequency", JS_NewInt32(ctx, con->cart->audio.frequency));
        JS_SetPropertyStr(ctx, aud, "buffer", JS_NewInt32(ctx, con->cart->audio.buffer_size));
        JS_SetPropertyStr(ctx, obj, "audio", aud);

        return obj;
    }
}

static JSValue js_sys_limit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *key;
    JSValue     result;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    key    = JS_ToCString(ctx, argv[0]);
    result = JS_UNDEFINED;

    if (key != NULL) {
        if (SDL_strcmp(key, "fb_width") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_FB_WIDTH);
        } else if (SDL_strcmp(key, "fb_height") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_FB_HEIGHT);
        } else if (SDL_strcmp(key, "palette_size") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_PALETTE_SIZE);
        } else if (SDL_strcmp(key, "max_sprites") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_MAX_SPRITES);
        } else if (SDL_strcmp(key, "max_maps") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_MAX_MAPS);
        } else if (SDL_strcmp(key, "max_map_layers") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_MAX_MAP_LAYERS);
        } else if (SDL_strcmp(key, "max_map_objects") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_MAX_MAP_OBJECTS);
        } else if (SDL_strcmp(key, "max_channels") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_MAX_CHANNELS);
        } else if (SDL_strcmp(key, "js_heap_mb") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_JS_HEAP_MB);
        } else if (SDL_strcmp(key, "js_stack_kb") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_JS_STACK_KB);
        } else if (SDL_strcmp(key, "target_fps") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_TARGET_FPS);
        }
        JS_FreeCString(ctx, key);
    }
    return result;
}

/* ------------------------------------------------------------------ */
/*  PICO-8 stat shim                                                   */
/* ------------------------------------------------------------------ */

static JSValue js_sys_stat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    int32_t        n;

    (void)this_val;
    con = dtr_api_get_console(ctx);
    n   = dtr_api_opt_int(ctx, argc, argv, 0, 0);

    switch (n) {
        case 0:
            /* Memory usage (approximate) */
            return JS_NewFloat64(ctx, 0.0);
        case 1:
            /* CPU usage (frame time / target) */
            return JS_NewFloat64(ctx,
                                 (double)(con->delta * (float)con->target_fps));
        case 7:
            /* FPS */
            return JS_NewFloat64(ctx,
                                 1.0 / (con->delta > 0.0f ? con->delta : 0.016f));
        case 30:
            /* key pressed */
            return JS_FALSE;
        case 31:
            /* key char */
            return JS_NewString(ctx, "");
        case 32:
            /* mouse x */
            return JS_NewFloat64(ctx, (double)con->mouse->x);
        case 33:
            /* mouse y */
            return JS_NewFloat64(ctx, (double)con->mouse->y);
        case 34:
            /* mouse buttons bitmask */
            return JS_NewInt32(ctx,
                               (con->mouse->btn_current[0] ? 1 : 0) |
                               (con->mouse->btn_current[1] ? 2 : 0) |
                               (con->mouse->btn_current[2] ? 4 : 0));
        case 36:
            /* mouse wheel */
            return JS_NewFloat64(ctx, (double)con->mouse->wheel);
        case 80:
            /* UTC year */
        case 90:
            /* Ticks */
            return JS_NewFloat64(ctx, (double)SDL_GetTicks());
        default:
            return JS_NewFloat64(ctx, 0.0);
    }
}

/* ------------------------------------------------------------------ */
/*  Master volume                                                      */
/* ------------------------------------------------------------------ */

static JSValue js_sys_volume(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    con = dtr_api_get_console(ctx);

    if (argc >= 1) {
        float vol = (float)dtr_api_opt_float(ctx, argc, argv, 0, 1.0);
        dtr_audio_set_master_volume(con->audio, vol);
    }
    return JS_NewFloat64(ctx, (double)dtr_audio_get_master_volume(con->audio));
}

/* ---- Function list ---------------------------------------------------- */

static JSValue
js_sys_text_input(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    bool           enabled;

    (void)this_val;
    con     = dtr_api_get_console(ctx);
    enabled = (argc >= 1) ? JS_ToBool(ctx, argv[0]) : true;

    if (enabled) {
        SDL_StartTextInput(con->window);
    } else {
        SDL_StopTextInput(con->window);
    }
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/*  Clipboard                                                          */
/* ------------------------------------------------------------------ */

static JSValue
js_sys_clipboard_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    char   *text;
    JSValue result;

    (void)this_val;
    (void)argc;
    (void)argv;
    text = SDL_GetClipboardText();
    if (text == NULL) {
        return JS_NewString(ctx, "");
    }
    result = JS_NewString(ctx, text);
    SDL_free(text);
    return result;
}

static JSValue
js_sys_clipboard_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *text;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }
    text = JS_ToCString(ctx, argv[0]);
    if (text != NULL) {
        SDL_SetClipboardText(text);
        JS_FreeCString(ctx, text);
    }
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_sys_funcs[] = {
    JS_CFUNC_DEF("time", 0, js_sys_time),
    JS_CFUNC_DEF("delta", 0, js_sys_delta),
    JS_CFUNC_DEF("frame", 0, js_sys_frame),
    JS_CFUNC_DEF("fps", 0, js_sys_fps),
    JS_CFUNC_DEF("target_fps", 0, js_sys_target_fps),
    JS_CFUNC_DEF("set_target_fps", 1, js_sys_set_target_fps),
    JS_CFUNC_DEF("width", 0, js_sys_width),
    JS_CFUNC_DEF("height", 0, js_sys_height),
    JS_CFUNC_DEF("version", 0, js_sys_version),
    JS_CFUNC_DEF("platform", 0, js_sys_platform),
    JS_CFUNC_DEF("log", 1, js_sys_log),
    JS_CFUNC_DEF("warn", 1, js_sys_warn),
    JS_CFUNC_DEF("error", 1, js_sys_error),
    JS_CFUNC_DEF("pause", 0, js_sys_pause),
    JS_CFUNC_DEF("resume", 0, js_sys_resume),
    JS_CFUNC_DEF("quit", 0, js_sys_quit),
    JS_CFUNC_DEF("restart", 0, js_sys_restart),
    JS_CFUNC_DEF("paused", 0, js_sys_paused),
    JS_CFUNC_DEF("fullscreen", 1, js_sys_fullscreen),
    JS_CFUNC_DEF("set_fullscreen", 1, js_sys_set_fullscreen),
    JS_CFUNC_DEF("config", 1, js_sys_config),
    JS_CFUNC_DEF("limit", 1, js_sys_limit),
    JS_CFUNC_DEF("stat", 1, js_sys_stat),
    JS_CFUNC_DEF("textInput", 1, js_sys_text_input),
    JS_CFUNC_DEF("volume", 1, js_sys_volume),
    JS_CFUNC_DEF("clipboardGet", 0, js_sys_clipboard_get),
    JS_CFUNC_DEF("clipboardSet", 1, js_sys_clipboard_set),
};

void dtr_sys_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_sys_funcs, countof(js_sys_funcs));
    JS_SetPropertyStr(ctx, global, "sys", ns);
}
