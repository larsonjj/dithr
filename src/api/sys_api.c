/**
 * \file            sys_api.c
 * \brief           JS sys namespace — system info, timing, debug
 */

#include "../audio.h"
#include "../cart.h"
#include "../graphics.h"
#include "../mouse.h"
#include "api_common.h"

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
/*  Fixed-update timing                                                */
/* ------------------------------------------------------------------ */

static JSValue
js_sys_fixed_delta(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewFloat64(ctx, (double)dtr_api_get_console(ctx)->fixed_dt);
}

static JSValue
js_sys_target_ups(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);
    return JS_NewInt32(ctx, (int32_t)(1.0f / con->fixed_dt + 0.5f));
}

static JSValue
js_sys_set_target_ups(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int32_t ups;

    (void)this_val;
    ups = dtr_api_opt_int(ctx, argc, argv, 0, CONSOLE_TARGET_FPS);
    if (ups < 15) {
        ups = 15;
    }
    if (ups > 240) {
        ups = 240;
    }
    dtr_api_get_console(ctx)->fixed_dt = 1.0f / (float)ups;
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
    con          = dtr_api_get_console(ctx);
    con->restart = true;
    con->running = false;
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
            result = JS_NewInt32(ctx, dtr_api_get_console(ctx)->fb_width);
        } else if (SDL_strcmp(key, "fb_height") == 0) {
            result = JS_NewInt32(ctx, dtr_api_get_console(ctx)->fb_height);
        } else if (SDL_strcmp(key, "palette_size") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_PALETTE_SIZE);
        } else if (SDL_strcmp(key, "max_sprites") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_MAX_SPRITES);
        } else if (SDL_strcmp(key, "max_maps") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_MAX_MAPS);
        } else if (SDL_strcmp(key, "max_map_layers") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_MAX_MAP_LAYERS);
        } else if (SDL_strcmp(key, "max_map_objects") == 0) { // NOLINT(bugprone-branch-clone)
            result = JS_NewInt32(ctx, CONSOLE_MAX_MAP_OBJECTS);
        } else if (SDL_strcmp(key, "max_channels") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_MAX_CHANNELS);
        } else if (SDL_strcmp(key, "js_heap_mb") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_JS_HEAP_MB);
        } else if (SDL_strcmp(key, "js_stack_kb") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_JS_STACK_KB);
        } else if (SDL_strcmp(key, "targetFps") == 0) {
            result = JS_NewInt32(ctx, CONSOLE_TARGET_FPS);
        }
        JS_FreeCString(ctx, key);
    }
    return result;
}

/* ------------------------------------------------------------------ */
/*  System stat query                                                  */
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
            return JS_NewFloat64(ctx, (double)(con->delta * (float)con->target_fps));
        case 7:
            /* FPS */
            return JS_NewFloat64(ctx, 1.0 / (con->delta > 0.0f ? con->delta : 0.016f));
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

static JSValue js_sys_perf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    JSValue        obj;
    JSValue        markers;

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);

    obj = JS_NewObject(ctx);
    JS_SetPropertyStr(
        ctx, obj, "cpu", JS_NewFloat64(ctx, (double)(con->delta * (float)con->target_fps)));
    JS_SetPropertyStr(
        ctx, obj, "fixed_update_ms", JS_NewFloat64(ctx, (double)con->fixed_update_ms));
    JS_SetPropertyStr(ctx, obj, "update_ms", JS_NewFloat64(ctx, (double)con->update_ms));
    JS_SetPropertyStr(ctx, obj, "draw_ms", JS_NewFloat64(ctx, (double)con->draw_ms));
    JS_SetPropertyStr(
        ctx, obj, "fps", JS_NewFloat64(ctx, 1.0 / (con->delta > 0.0f ? con->delta : 0.016f)));
    JS_SetPropertyStr(ctx, obj, "frame", JS_NewFloat64(ctx, (double)con->frame_count));

    markers = JS_NewObject(ctx);
    for (int32_t i = 0; i < con->perf_marker_count; ++i) {
        if (!con->perf_markers[i].active) {
            JS_SetPropertyStr(ctx,
                              markers,
                              con->perf_markers[i].label,
                              JS_NewFloat64(ctx, (double)con->perf_markers[i].elapsed_ms));
        }
    }
    JS_SetPropertyStr(ctx, obj, "markers", markers);
    return obj;
}

/* ------------------------------------------------------------------ */
/*  Perf markers                                                       */
/* ------------------------------------------------------------------ */

static JSValue
js_sys_perf_begin(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    const char *   label;
    int32_t        idx;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    con   = dtr_api_get_console(ctx);
    label = JS_ToCString(ctx, argv[0]);
    if (label == NULL) {
        return JS_UNDEFINED;
    }

    /* Reuse existing slot with the same label, or allocate a new one */
    idx = -1;
    for (int32_t i = 0; i < con->perf_marker_count; ++i) {
        if (SDL_strcmp(con->perf_markers[i].label, label) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (con->perf_marker_count >= CONSOLE_MAX_PERF_MARKERS) {
            JS_FreeCString(ctx, label);
            return JS_UNDEFINED;
        }
        idx = con->perf_marker_count++;
        SDL_strlcpy(con->perf_markers[idx].label, label, CONSOLE_PERF_LABEL_LEN);
    }
    JS_FreeCString(ctx, label);

    con->perf_markers[idx].start      = SDL_GetPerformanceCounter();
    con->perf_markers[idx].elapsed_ms = 0.0f;
    con->perf_markers[idx].active     = true;
    return JS_UNDEFINED;
}

static JSValue js_sys_perf_end(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t *con;
    const char *   label;
    uint64_t       now;
    double         freq;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    con   = dtr_api_get_console(ctx);
    label = JS_ToCString(ctx, argv[0]);
    if (label == NULL) {
        return JS_UNDEFINED;
    }

    now  = SDL_GetPerformanceCounter();
    freq = (double)SDL_GetPerformanceFrequency();

    for (int32_t i = 0; i < con->perf_marker_count; ++i) {
        if (SDL_strcmp(con->perf_markers[i].label, label) == 0 && con->perf_markers[i].active) {
            con->perf_markers[i].elapsed_ms =
                (float)((double)(now - con->perf_markers[i].start) / freq * 1000.0);
            con->perf_markers[i].active = false;
            JS_FreeCString(ctx, label);
            return JS_NewFloat64(ctx, (double)con->perf_markers[i].elapsed_ms);
        }
    }

    JS_FreeCString(ctx, label);
    return JS_UNDEFINED;
}

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
    char *  text;
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

/* ------------------------------------------------------------------ */
/*  File I/O (sandboxed to cart directory)                             */
/* ------------------------------------------------------------------ */

/**
 * \brief           Resolve a relative path against the cart base and validate
 *                  it stays within the sandbox.
 * \return          true if the resolved path is safe; full is populated.
 */
static bool prv_resolve_sandboxed(JSContext *ctx, const char *rel, char *full, size_t full_size)
{
    dtr_console_t *con;
    const char *   base;
    char           norm_base[1024];
    size_t         base_len;
    char *         pos;

    con  = dtr_api_get_console(ctx);
    base = con->cart->base_path;

    /* Reject absolute paths and explicit traversal */
    if (rel[0] == '/' || rel[0] == '\\') {
        return false;
    }
    if (SDL_strstr(rel, "..") != NULL) {
        return false;
    }

    SDL_snprintf(full, full_size, "%s/%s", base, rel);

    /* Normalise backslashes to forward slashes */
    for (pos = full; *pos != '\0'; ++pos) {
        if (*pos == '\\') {
            *pos = '/';
        }
    }

    /* Normalise base_path the same way for comparison */
    SDL_strlcpy(norm_base, base, sizeof(norm_base));
    for (pos = norm_base; *pos != '\0'; ++pos) {
        if (*pos == '\\') {
            *pos = '/';
        }
    }

    /* Final check: resolved path must start with base_path */
    base_len = SDL_strlen(norm_base);
    if (SDL_strncmp(full, norm_base, base_len) != 0) {
        return false;
    }

    return true;
}

static JSValue js_sys_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *rel;
    char        full[1024];
    void *      data;
    size_t      len;
    JSValue     result;

    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }
    rel = JS_ToCString(ctx, argv[0]);
    if (rel == NULL) {
        return JS_UNDEFINED;
    }

    if (!prv_resolve_sandboxed(ctx, rel, full, sizeof(full))) {
        JS_FreeCString(ctx, rel);
        return JS_ThrowRangeError(ctx, "readFile: path outside cart directory");
    }
    JS_FreeCString(ctx, rel);

    data = SDL_LoadFile(full, &len);
    if (data == NULL) {
        return JS_UNDEFINED;
    }

    result = JS_NewStringLen(ctx, (const char *)data, len);
    SDL_free(data);
    return result;
}

static JSValue
js_sys_write_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *rel;
    const char *content;
    size_t      content_len;
    char        full[1024];

    (void)this_val;
    if (argc < 2) {
        return JS_FALSE;
    }
    rel = JS_ToCString(ctx, argv[0]);
    if (rel == NULL) {
        return JS_FALSE;
    }

    if (!prv_resolve_sandboxed(ctx, rel, full, sizeof(full))) {
        JS_FreeCString(ctx, rel);
        return JS_ThrowRangeError(ctx, "writeFile: path outside cart directory");
    }
    JS_FreeCString(ctx, rel);

    content = JS_ToCStringLen(ctx, &content_len, argv[1]);
    if (content == NULL) {
        return JS_FALSE;
    }

    if (!SDL_SaveFile(full, content, content_len)) {
        JS_FreeCString(ctx, content);
        return JS_FALSE;
    }

    JS_FreeCString(ctx, content);
    return JS_TRUE;
}

static JSValue
js_sys_list_files(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *rel;
    char        full[1024];
    int32_t     count;
    char **     files;
    JSValue     arr;

    (void)this_val;

    rel = "";
    if (argc >= 1) {
        rel = JS_ToCString(ctx, argv[0]);
        if (rel == NULL) {
            return JS_NewArray(ctx);
        }
    }

    if (rel[0] != '\0') {
        if (!prv_resolve_sandboxed(ctx, rel, full, sizeof(full))) {
            if (argc >= 1) {
                JS_FreeCString(ctx, rel);
            }
            return JS_ThrowRangeError(ctx, "listFiles: path outside cart directory");
        }
    } else {
        SDL_snprintf(full, sizeof(full), "%s/", dtr_api_get_console(ctx)->cart->base_path);
    }
    if (argc >= 1) {
        JS_FreeCString(ctx, rel);
    }

    files = SDL_GlobDirectory(full, NULL, 0, &count);
    arr   = JS_NewArray(ctx);
    if (files != NULL) {
        for (int32_t idx = 0; idx < count; ++idx) {
            JS_SetPropertyUint32(ctx, arr, (uint32_t)idx, JS_NewString(ctx, files[idx]));
        }
        SDL_free((void *)files);
    }
    return arr;
}

/* ------------------------------------------------------------------ */
/*  Directory listing                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    JSContext * ctx;
    JSValue     arr;
    uint32_t    idx;
    const char *base;
} prv_list_dirs_ctx_t;

static SDL_EnumerationResult
prv_list_dirs_cb(void *userdata, const char *dirname, const char *fname)
{
    prv_list_dirs_ctx_t *ld;
    char                 full[1024];
    SDL_PathInfo         info;

    ld = (prv_list_dirs_ctx_t *)userdata;
    (void)dirname;

    SDL_snprintf(full, sizeof(full), "%s%s", ld->base, fname);
    if (SDL_GetPathInfo(full, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
        JS_SetPropertyUint32(ld->ctx, ld->arr, ld->idx, JS_NewString(ld->ctx, fname));
        ++ld->idx;
    }
    return SDL_ENUM_CONTINUE;
}

static JSValue js_sys_list_dirs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *        rel;
    char                full[1024];
    prv_list_dirs_ctx_t ld;

    (void)this_val;

    rel = "";
    if (argc >= 1) {
        rel = JS_ToCString(ctx, argv[0]);
        if (rel == NULL) {
            return JS_NewArray(ctx);
        }
    }

    if (rel[0] != '\0') {
        if (!prv_resolve_sandboxed(ctx, rel, full, sizeof(full))) {
            if (argc >= 1) {
                JS_FreeCString(ctx, rel);
            }
            return JS_ThrowRangeError(ctx, "listDirs: path outside cart directory");
        }
    } else {
        SDL_snprintf(full, sizeof(full), "%s/", dtr_api_get_console(ctx)->cart->base_path);
    }
    if (argc >= 1) {
        JS_FreeCString(ctx, rel);
    }

    ld.ctx  = ctx;
    ld.arr  = JS_NewArray(ctx);
    ld.idx  = 0;
    ld.base = full;

    SDL_EnumerateDirectory(full, prv_list_dirs_cb, &ld);
    return ld.arr;
}

/* ------------------------------------------------------------------ */
/*  Built-in FPS widget                                                */
/* ------------------------------------------------------------------ */

#define FPS_HIST_LEN 50

static JSValue js_sys_draw_fps(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    dtr_console_t * con;
    dtr_graphics_t *g;
    float           cur_fps;
    float           target;
    int32_t         wx;
    int32_t         wy;
    int32_t         ww;
    int32_t         gh;
    int32_t         save_cam_x;
    int32_t         save_cam_y;
    char            buf[16];

    (void)this_val;
    (void)argc;
    (void)argv;
    con = dtr_api_get_console(ctx);
    g   = con->graphics;

    /* Update smoothed FPS */
    cur_fps = con->delta > 0.0f ? 1.0f / con->delta : 60.0f;
    con->fps_smooth += (cur_fps - con->fps_smooth) * 0.05f;
    con->fps_history[con->fps_hist_idx] = con->fps_smooth;
    con->fps_hist_idx                   = (con->fps_hist_idx + 1) % FPS_HIST_LEN;

    /* Save and reset camera for screen-space drawing */
    save_cam_x  = g->camera_x;
    save_cam_y  = g->camera_y;
    g->camera_x = 0;
    g->camera_y = 0;

    target = (float)con->target_fps;
    wx     = con->fb_width - FPS_HIST_LEN - 4;
    wy     = 0;
    ww     = FPS_HIST_LEN + 4;
    gh     = 16;

    /* Background */
    dtr_gfx_rectfill(g, wx, wy, wx + ww - 1, wy + 8 + gh + 1, 0);

    /* FPS label */
    SDL_snprintf(buf, sizeof(buf), "%d FPS", (int32_t)con->fps_smooth);
    dtr_gfx_print(g, buf, wx + 2, wy + 1, 7);

    /* Graph border */
    dtr_gfx_rect(g, wx + 1, wy + 8, wx + ww - 2, wy + 8 + gh, 5);

    /* Graph lines */
    for (int32_t idx = 1; idx < FPS_HIST_LEN; ++idx) {
        int32_t i0;
        int32_t i1;
        float   v0;
        float   v1;
        int32_t y0;
        int32_t y1;
        uint8_t clr;

        i0 = (con->fps_hist_idx + idx - 1) % FPS_HIST_LEN;
        i1 = (con->fps_hist_idx + idx) % FPS_HIST_LEN;

        v0 = con->fps_history[i0] / target;
        v1 = con->fps_history[i1] / target;
        if (v0 < 0.0f) {
            v0 = 0.0f;
        }
        if (v0 > 1.0f) {
            v0 = 1.0f;
        }
        if (v1 < 0.0f) {
            v1 = 0.0f;
        }
        if (v1 > 1.0f) {
            v1 = 1.0f;
        }

        y0  = wy + 8 + gh - 1 - (int32_t)(v0 * (float)(gh - 2));
        y1  = wy + 8 + gh - 1 - (int32_t)(v1 * (float)(gh - 2));
        clr = v1 > 0.9f ? 11 : v1 > 0.5f ? 9 : 8;
        dtr_gfx_line(g, wx + 2 + idx - 1, y0, wx + 2 + idx, y1, clr);
    }

    /* Restore camera */
    g->camera_x = save_cam_x;
    g->camera_y = save_cam_y;

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_sys_funcs[] = {
    JS_CFUNC_DEF("time", 0, js_sys_time),
    JS_CFUNC_DEF("delta", 0, js_sys_delta),
    JS_CFUNC_DEF("frame", 0, js_sys_frame),
    JS_CFUNC_DEF("fps", 0, js_sys_fps),
    JS_CFUNC_DEF("targetFps", 0, js_sys_target_fps),
    JS_CFUNC_DEF("setTargetFps", 1, js_sys_set_target_fps),
    JS_CFUNC_DEF("fixedDelta", 0, js_sys_fixed_delta),
    JS_CFUNC_DEF("targetUps", 0, js_sys_target_ups),
    JS_CFUNC_DEF("setTargetUps", 1, js_sys_set_target_ups),
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
    JS_CFUNC_DEF("setFullscreen", 1, js_sys_set_fullscreen),
    JS_CFUNC_DEF("config", 1, js_sys_config),
    JS_CFUNC_DEF("limit", 1, js_sys_limit),
    JS_CFUNC_DEF("stat", 1, js_sys_stat),
    JS_CFUNC_DEF("perf", 0, js_sys_perf),
    JS_CFUNC_DEF("perfBegin", 1, js_sys_perf_begin),
    JS_CFUNC_DEF("perfEnd", 1, js_sys_perf_end),
    JS_CFUNC_DEF("textInput", 1, js_sys_text_input),
    JS_CFUNC_DEF("volume", 1, js_sys_volume),
    JS_CFUNC_DEF("clipboardGet", 0, js_sys_clipboard_get),
    JS_CFUNC_DEF("clipboardSet", 1, js_sys_clipboard_set),
    JS_CFUNC_DEF("readFile", 1, js_sys_read_file),
    JS_CFUNC_DEF("writeFile", 2, js_sys_write_file),
    JS_CFUNC_DEF("listFiles", 1, js_sys_list_files),
    JS_CFUNC_DEF("listDirs", 1, js_sys_list_dirs),
    JS_CFUNC_DEF("drawFps", 0, js_sys_draw_fps),
};

void dtr_sys_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_sys_funcs, countof(js_sys_funcs));
    JS_SetPropertyStr(ctx, global, "sys", ns);
}
