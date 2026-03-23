/**
 * \file            console.c
 * \brief           Console lifecycle — create, run, destroy
 */

#include "console.h"

#include "audio.h"
#include "cart.h"
#include "cart_import.h"
#include "event.h"
#include "gamepad.h"
#include "graphics.h"
#include "input.h"
#include "mouse.h"
#include "postfx.h"
#include "runtime.h"

#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Forward declarations for internal helpers                          */
/* ------------------------------------------------------------------ */

static void prv_console_cleanup(dtr_console_t *con);
static bool prv_load_cart_assets(dtr_console_t *con);
static void prv_render_pause_overlay(dtr_console_t *con);
static void prv_render_error_overlay(dtr_console_t *con);

/**
 * \brief           Free all subsystem resources without event emission or
 *                  persistence save.  Safe to call when any subset of fields
 *                  is still NULL (e.g. during a partially-constructed console).
 */
static void prv_console_cleanup(dtr_console_t *con)
{
    if (con == NULL) {
        return;
    }

    dtr_postfx_destroy(con->postfx);
    dtr_event_destroy(con->events);
    dtr_input_destroy(con->input);
    dtr_gamepad_destroy(con->gamepads);
    dtr_mouse_destroy(con->mouse);
    dtr_key_destroy(con->keys);
    dtr_audio_destroy(con->audio);
    dtr_gfx_destroy(con->graphics);
    dtr_cart_destroy(con->cart);
    dtr_runtime_destroy(con->runtime);

    if (con->screen_tex != NULL) {
        SDL_DestroyTexture(con->screen_tex);
    }
    if (con->renderer != NULL) {
        SDL_DestroyRenderer(con->renderer);
    }
    if (con->window != NULL) {
        SDL_DestroyWindow(con->window);
    }

    DTR_FREE(con);
}

/* ------------------------------------------------------------------ */
/*  Create                                                             */
/* ------------------------------------------------------------------ */

dtr_console_t *dtr_console_create(const char *cart_path)
{
    dtr_console_t *con;
    char *         json_data;
    size_t         json_len;

    con = DTR_CALLOC(1, sizeof(dtr_console_t));
    if (con == NULL) {
        return NULL;
    }

    /* Store cart path for restart support */
    SDL_strlcpy(con->cart_path, cart_path, sizeof(con->cart_path));

    /* --- Cart (create + defaults) --- */
    con->cart = dtr_cart_create();
    if (con->cart == NULL) {
        prv_console_cleanup(con);
        return NULL;
    }
    dtr_cart_defaults(con->cart);

    /* --- JS runtime (temporary for JSON parsing) --- */
    con->runtime = dtr_runtime_create(con,
                                      (int32_t)(con->cart->runtime.mem_limit / (1024u * 1024u)),
                                      (int32_t)(con->cart->runtime.stack_limit / 1024u));
    if (con->runtime == NULL) {
        prv_console_cleanup(con);
        return NULL;
    }

    /* --- Load and parse cart.json --- */
    json_data = (char *)SDL_LoadFile(cart_path, &json_len);
    if (json_data != NULL) {
        if (!dtr_cart_parse(con->cart, con->runtime->ctx, json_data, json_len)) {
            SDL_Log("Warning: cart parse failed, using defaults");
        }
        SDL_free(json_data);

        if (!dtr_cart_validate(con->cart)) {
            SDL_Log("Warning: cart values clamped to compiled ceilings");
        }

        /* Store base path for relative asset resolution */
        {
            const char *sep;
            size_t      base_len;

            sep = SDL_strrchr(cart_path, '/');
            if (sep == NULL) {
                sep = SDL_strrchr(cart_path, '\\');
            }
            if (sep != NULL) {
                base_len = (size_t)(sep - cart_path + 1);
                if (base_len >= sizeof(con->cart->base_path)) {
                    base_len = sizeof(con->cart->base_path) - 1;
                }
                SDL_memcpy(con->cart->base_path, cart_path, base_len);
                con->cart->base_path[base_len] = '\0';
            } else {
                con->cart->base_path[0] = '\0';
            }
        }
    } else {
        SDL_Log("No cart file at '%s', using defaults", cart_path);
    }

    /* --- Apply framebuffer dimensions --- */
    con->fb_width   = con->cart->display.width;
    con->fb_height  = con->cart->display.height;
    con->win_width  = con->fb_width * con->cart->display.scale;
    con->win_height = con->fb_height * con->cart->display.scale;
    con->target_fps = con->cart->timing.fps;

    /* --- Window + renderer --- */
    con->window = SDL_CreateWindow(con->cart->display.window_title[0] != '\0' ?
                                       con->cart->display.window_title :
                                       con->cart->meta.title,
                                   con->win_width,
                                   con->win_height,
                                   SDL_WINDOW_RESIZABLE);
    if (con->window == NULL) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        prv_console_cleanup(con);
        return NULL;
    }

    con->renderer = SDL_CreateRenderer(con->window, NULL);
    if (con->renderer == NULL) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        prv_console_cleanup(con);
        return NULL;
    }

    SDL_SetRenderVSync(con->renderer, 0);

    SDL_SetRenderLogicalPresentation(
        con->renderer, con->fb_width, con->fb_height, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    /* Screen texture: streaming, nearest-neighbor for pixel-perfect */
    con->screen_tex = SDL_CreateTexture(con->renderer,
                                        SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        con->fb_width,
                                        con->fb_height);
    if (con->screen_tex == NULL) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        prv_console_cleanup(con);
        return NULL;
    }
    SDL_SetTextureScaleMode(con->screen_tex, SDL_SCALEMODE_NEAREST);

    /* --- Graphics subsystem --- */
    con->graphics = dtr_gfx_create(con->fb_width, con->fb_height);
    if (con->graphics == NULL) {
        prv_console_cleanup(con);
        return NULL;
    }

    /* --- Audio subsystem --- */
    con->audio = dtr_audio_create(
        con->cart->audio.channels, con->cart->audio.frequency, con->cart->audio.buffer_size);
    if (con->audio == NULL) {
        SDL_Log("Warning: audio subsystem failed to initialise — audio disabled");
    }

    /* --- Input subsystems --- */
    con->keys     = dtr_key_create();
    con->mouse    = dtr_mouse_create();

    /* Ensure gamepad subsystem is initialised (SDL callbacks only auto-init EVENTS) */
    if (!SDL_WasInit(SDL_INIT_GAMEPAD)) {
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    }
    con->gamepads = dtr_gamepad_create();
    con->input    = dtr_input_create();

    /* --- Mouse coordinate mapping (handled by SDL_ConvertEventToRenderCoordinates) --- */

    /* --- Event bus --- */
    con->events = dtr_event_create(con->runtime->ctx);

    /* --- PostFX --- */
    con->postfx = dtr_postfx_create(con->fb_width, con->fb_height);
    if (con->cart->meta.default_postfx[0] != '\0') {
        dtr_postfx_use(con->postfx, con->cart->meta.default_postfx);
    }

    /* --- Cart ref to JS context --- */
    con->cart->ctx = con->runtime->ctx;

    /* --- Load assets (sprites, maps, sfx, music) --- */
    prv_load_cart_assets(con);

    /* --- Load persisted save data (dslots + kv) --- */
    dtr_cart_persist_load(con->cart);

    /* --- Register all JS APIs --- */
    dtr_api_register_all(con->runtime);

    /* --- Apply default input mappings from cart.json --- */
    for (int32_t mi = 0; mi < con->cart->input.mapping_count; ++mi) {
        dtr_cart_input_mapping_t *mapping;
        dtr_binding_t             bindings[DTR_INPUT_MAX_BINDINGS];
        int32_t                   valid;

        mapping = &con->cart->input.mappings[mi];
        valid   = 0;

        for (int32_t bi = 0; bi < mapping->bind_count && valid < DTR_INPUT_MAX_BINDINGS; ++bi) {
            if (dtr_input_parse_binding(mapping->bindings[bi], &bindings[valid])) {
                ++valid;
            } else {
                SDL_Log("Unknown input binding: %s", mapping->bindings[bi]);
            }
        }

        if (valid > 0) {
            dtr_input_map(con->input, mapping->action, bindings, valid);
        }
    }

    /* --- Evaluate cart JS source --- */
    if (con->cart->code != NULL && con->cart->code_len > 0) {
        dtr_runtime_eval(con->runtime, con->cart->code, con->cart->code_len, "main.js");
    }

    /* --- Fire sys:cart_load --- */
    dtr_event_emit(con->events, "sys:cart_load", JS_UNDEFINED);
    dtr_event_flush(con->events);

    /* --- Call _init() --- */
    dtr_runtime_call(con->runtime, con->runtime->atom_init);

    /* --- Ready --- */
    con->running     = true;
    con->paused      = false;
    con->has_error   = false;
    con->fullscreen  = false;
    con->frame_count = 0;
    con->time        = 0.0f;
    con->delta       = 0.0f;
    con->time_prev   = SDL_GetPerformanceCounter();

#if DEV_BUILD
    /* Set up file watcher for the JS source */
    con->watch_path[0] = '\0';
    con->watch_mtime   = 0;
    con->watch_timer   = 0.5f;
    if (con->cart->code_path[0] != '\0') {
        SDL_PathInfo info;

        SDL_snprintf(con->watch_path, sizeof(con->watch_path),
                     "%s%s", con->cart->base_path, con->cart->code_path);
        if (SDL_GetPathInfo(con->watch_path, &info)) {
            con->watch_mtime = info.modify_time;
            SDL_Log("hot-reload: watching %s", con->watch_path);
        }
    }
#endif

    return con;
}

/* ------------------------------------------------------------------ */
/*  Per-event handler (called by SDL_AppEvent)                         */
/* ------------------------------------------------------------------ */

void dtr_console_event(dtr_console_t *con, SDL_Event *event)
{
    /* Let SDL handle logical-presentation coordinate conversion (letterbox, etc.) */
    SDL_ConvertEventToRenderCoordinates(con->renderer, event);

    switch (event->type) {
        case SDL_EVENT_QUIT:
            con->running = false;
            break;

        case SDL_EVENT_KEY_DOWN: {
            dtr_key_t key;

            /* Hardcoded pause toggle: Escape */
            if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
                con->paused = !con->paused;
                if (con->paused) {
                    dtr_event_emit(con->events, "sys:pause", JS_UNDEFINED);
                } else {
                    dtr_event_emit(con->events, "sys:resume", JS_UNDEFINED);
                }
                break;
            }
            /* Fullscreen toggle: F11 */
            if (event->key.scancode == SDL_SCANCODE_F11) {
                con->fullscreen = !con->fullscreen;
                SDL_SetWindowFullscreen(con->window, con->fullscreen);
                dtr_event_emit(con->events, "sys:fullscreen", JS_UNDEFINED);
                break;
            }

            key = dtr_key_from_scancode(event->key.scancode);
            if (key != DTR_KEY_NONE) {
                dtr_key_set(con->keys, key, true);
            }
            break;
        }

        case SDL_EVENT_KEY_UP: {
            dtr_key_t key;

            key = dtr_key_from_scancode(event->key.scancode);
            if (key != DTR_KEY_NONE) {
                dtr_key_set(con->keys, key, false);
            }
            break;
        }

        case SDL_EVENT_MOUSE_MOTION:
            con->mouse->x = event->motion.x;
            con->mouse->y = event->motion.y;
            con->mouse->dx += event->motion.xrel;
            con->mouse->dy += event->motion.yrel;
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event->button.button >= 1 && event->button.button <= DTR_MOUSE_BTN_COUNT) {
                int idx = event->button.button - 1;
                con->mouse->btn_current[idx] = true;
                con->mouse->btn_pressed[idx] = true;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.button >= 1 && event->button.button <= DTR_MOUSE_BTN_COUNT) {
                con->mouse->btn_current[event->button.button - 1] = false;
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            con->mouse->wheel_x += event->wheel.x;
            con->mouse->wheel += event->wheel.y;
            break;

        case SDL_EVENT_GAMEPAD_ADDED:
            dtr_gamepad_on_added(con->gamepads, event->gdevice.which);
            break;

        case SDL_EVENT_GAMEPAD_REMOVED:
            dtr_gamepad_on_removed(con->gamepads, event->gdevice.which);
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            dtr_gamepad_on_button(con->gamepads, event->gbutton.which,
                                  event->gbutton.button, true);
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            dtr_gamepad_on_button(con->gamepads, event->gbutton.which,
                                  event->gbutton.button, false);
            break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            dtr_gamepad_on_axis(con->gamepads, event->gaxis.which,
                                event->gaxis.axis, event->gaxis.value);
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            dtr_event_emit(con->events, "sys:focus_lost", JS_UNDEFINED);
            break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            dtr_event_emit(con->events, "sys:focus_gained", JS_UNDEFINED);
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            dtr_event_emit(con->events, "sys:resize", JS_UNDEFINED);
            /* Update cached window size */
            {
                int32_t win_w;
                int32_t win_h;

                SDL_GetWindowSize(con->window, &win_w, &win_h);
                con->win_width  = win_w;
                con->win_height = win_h;
            }
            break;

        case SDL_EVENT_TEXT_INPUT:
            if (event->text.text != NULL) {
                JSContext *ctx;
                JSValue    payload;

                ctx     = con->runtime->ctx;
                payload = JS_NewString(ctx, event->text.text);
                dtr_event_emit(con->events, "text:input", payload);
                JS_FreeValue(ctx, payload);
            }
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/*  Per-frame iteration (called by SDL_AppIterate)                     */
/* ------------------------------------------------------------------ */

void dtr_console_iterate(dtr_console_t *con)
{
    uint64_t now;
    float    freq;

    /* Timing */
    freq           = (float)SDL_GetPerformanceFrequency();
    now            = SDL_GetPerformanceCounter();
    con->delta     = (float)(now - con->time_prev) / freq;
    con->time_prev = now;
    con->time += con->delta;

    /* Mark start of a new frame */
    con->new_frame = true;

    if (con->paused) {
        prv_render_pause_overlay(con);
        return;
    }

    if (con->runtime->error_active) {
        prv_render_error_overlay(con);
        return;
    }

    /* Event bus flush */
    dtr_event_flush(con->events);

    /* JS _update(dt) */
    {
        JSValue dt_arg = JS_NewFloat64(con->runtime->ctx, (double)con->delta);
        dtr_runtime_call_argv(con->runtime, con->runtime->atom_update, 1, &dt_arg);
        JS_FreeValue(con->runtime->ctx, dt_arg);
    }

    /* JS _draw */
    dtr_runtime_call(con->runtime, con->runtime->atom_draw);

    /* Flip + transition + postfx directly into the streaming texture */
    {
        void   *locked;
        int     pitch;
        int32_t row_bytes;

        row_bytes = con->fb_width * (int32_t)sizeof(uint32_t);
        SDL_LockTexture(con->screen_tex, NULL, &locked, &pitch);
        if (pitch == row_bytes) {
            dtr_gfx_flip_to(con->graphics, (uint32_t *)locked);
            dtr_gfx_transition_update_buf(con->graphics, (uint32_t *)locked);
            dtr_postfx_apply(con->postfx, (uint32_t *)locked,
                             con->fb_width, con->fb_height);
        } else {
            dtr_gfx_flip(con->graphics);
            dtr_gfx_transition_update(con->graphics);
            dtr_postfx_apply(con->postfx, con->graphics->pixels,
                             con->fb_width, con->fb_height);
            for (int32_t y = 0; y < con->fb_height; ++y) {
                SDL_memcpy((uint8_t *)locked + y * pitch,
                           con->graphics->pixels + y * con->fb_width,
                           (size_t)row_bytes);
            }
        }
        SDL_UnlockTexture(con->screen_tex);
    }

    /* Present */
    SDL_RenderClear(con->renderer);
    SDL_RenderTexture(con->renderer, con->screen_tex, NULL, NULL);
    SDL_RenderPresent(con->renderer);

    /* Drain microtasks */
    dtr_runtime_drain_jobs(con->runtime);

    /* Input update — must happen AFTER JS _update/_draw so that btnp()
       edge detection (current && !previous) is visible during the frame. */
    dtr_key_update(con->keys);
    dtr_mouse_update(con->mouse);

    /* Reset per-frame mouse deltas after JS has consumed them */
    con->mouse->dx      = 0.0f;
    con->mouse->dy      = 0.0f;
    con->mouse->wheel_x = 0.0f;
    con->mouse->wheel   = 0.0f;
    dtr_gamepad_update(con->gamepads);
    dtr_input_update(con->input, con->keys, con->gamepads);

    ++con->frame_count;

#if DEV_BUILD
    /* Hot-reload: poll JS source file for changes */
    if (con->watch_path[0] != '\0') {
        con->watch_timer -= con->delta;
        if (con->watch_timer <= 0.0f) {
            SDL_PathInfo info;

            con->watch_timer = 0.5f; /* poll every 500 ms */
            if (SDL_GetPathInfo(con->watch_path, &info)) {
                if (info.modify_time != con->watch_mtime) {
                    SDL_Log("hot-reload: %s changed, reloading", con->watch_path);
                    con->reload = true;
                }
            }
        }
    }
#endif
}

/* ------------------------------------------------------------------ */
/*  Hot reload (JS only)                                               */
/* ------------------------------------------------------------------ */

bool dtr_console_reload(dtr_console_t *con)
{
    char   *saved_json = NULL;
    char   *new_code   = NULL;
    size_t  new_len    = 0;
    char    code_full[1024];

    if (con == NULL) {
        return false;
    }

    /* ---- 1. Call _save() to capture game state (optional) ---- */
    if (con->runtime != NULL && con->runtime->ctx != NULL && !con->runtime->error_active) {
        JSValue global;
        JSValue func;

        global = JS_GetGlobalObject(con->runtime->ctx);
        func   = JS_GetProperty(con->runtime->ctx, global, con->runtime->atom_save);

        if (JS_IsFunction(con->runtime->ctx, func)) {
            JSValue result;

            result = JS_Call(con->runtime->ctx, func, global, 0, NULL);
            if (!JS_IsException(result) && !JS_IsUndefined(result)) {
                JSValue  json_val;
                JSValue  stringify;
                JSValue  json_obj;

                json_obj  = JS_GetPropertyStr(con->runtime->ctx, global, "JSON");
                stringify = JS_GetPropertyStr(con->runtime->ctx, json_obj, "stringify");
                json_val  = JS_Call(con->runtime->ctx, stringify, json_obj, 1, &result);

                if (JS_IsString(json_val)) {
                    const char *str;
                    str = JS_ToCString(con->runtime->ctx, json_val);
                    if (str != NULL) {
                        saved_json = SDL_strdup(str);
                        JS_FreeCString(con->runtime->ctx, str);
                    }
                }

                JS_FreeValue(con->runtime->ctx, json_val);
                JS_FreeValue(con->runtime->ctx, stringify);
                JS_FreeValue(con->runtime->ctx, json_obj);
            }
            JS_FreeValue(con->runtime->ctx, result);
        }

        JS_FreeValue(con->runtime->ctx, func);
        JS_FreeValue(con->runtime->ctx, global);
    }

    /* ---- 2. Fire sys:cart_unload ---- */
    if (con->events != NULL && con->runtime != NULL && con->runtime->ctx != NULL) {
        dtr_event_emit(con->events, "sys:cart_unload", JS_UNDEFINED);
        dtr_event_flush(con->events);
    }

    /* ---- 3. Destroy event bus + runtime (JS context goes away) ---- */
    dtr_event_destroy(con->events);
    con->events = NULL;

    dtr_runtime_destroy(con->runtime);
    con->runtime = NULL;

    /* ---- 4. Re-read JS source from disk ---- */
    SDL_snprintf(code_full, sizeof(code_full),
                 "%s%s", con->cart->base_path, con->cart->code_path);
    new_code = (char *)SDL_LoadFile(code_full, &new_len);
    if (new_code == NULL) {
        SDL_Log("hot-reload: failed to read %s", code_full);
        SDL_free(saved_json);
        return false;
    }

    /* Replace cart code buffer */
    SDL_free(con->cart->code);
    con->cart->code     = new_code;
    con->cart->code_len = new_len;

    /* ---- 5. Create fresh runtime + context ---- */
    con->runtime = dtr_runtime_create(
        con,
        (int32_t)(con->cart->runtime.mem_limit / (1024u * 1024u)),
        (int32_t)(con->cart->runtime.stack_limit / 1024u));
    if (con->runtime == NULL) {
        SDL_Log("hot-reload: failed to create runtime");
        SDL_free(saved_json);
        return false;
    }

    /* ---- 6. Recreate event bus ---- */
    con->events = dtr_event_create(con->runtime->ctx);

    /* ---- 7. Update cart context reference ---- */
    con->cart->ctx = con->runtime->ctx;

    /* ---- 8. Register all JS APIs ---- */
    dtr_api_register_all(con->runtime);

    /* ---- 9. Evaluate JS source ---- */
    if (!dtr_runtime_eval(con->runtime, con->cart->code, con->cart->code_len, "main.js")) {
        SDL_Log("hot-reload: eval failed");
        SDL_free(saved_json);
        return false;
    }

    /* ---- 10. Call _restore(state) if we have saved state ---- */
    if (saved_json != NULL) {
        JSValue global;
        JSValue func;

        global = JS_GetGlobalObject(con->runtime->ctx);
        func   = JS_GetProperty(con->runtime->ctx, global, con->runtime->atom_restore);

        if (JS_IsFunction(con->runtime->ctx, func)) {
            JSValue parsed;
            JSValue json_obj;
            JSValue parse_fn;

            json_obj = JS_GetPropertyStr(con->runtime->ctx, global, "JSON");
            parse_fn = JS_GetPropertyStr(con->runtime->ctx, json_obj, "parse");
            {
                JSValue json_str;

                json_str = JS_NewString(con->runtime->ctx, saved_json);
                parsed   = JS_Call(con->runtime->ctx, parse_fn, json_obj, 1, &json_str);
                JS_FreeValue(con->runtime->ctx, json_str);
            }

            if (!JS_IsException(parsed)) {
                JSValue res;

                res = JS_Call(con->runtime->ctx, func, global, 1, &parsed);
                JS_FreeValue(con->runtime->ctx, res);
            }

            JS_FreeValue(con->runtime->ctx, parsed);
            JS_FreeValue(con->runtime->ctx, parse_fn);
            JS_FreeValue(con->runtime->ctx, json_obj);
        }

        JS_FreeValue(con->runtime->ctx, func);
        JS_FreeValue(con->runtime->ctx, global);
        SDL_free(saved_json);
    }

    /* ---- 11. Call _init() ---- */
    dtr_runtime_call(con->runtime, con->runtime->atom_init);

    /* ---- 12. Fire sys:cart_load ---- */
    dtr_event_emit(con->events, "sys:cart_load", JS_UNDEFINED);
    dtr_event_flush(con->events);

    /* ---- 13. Clear error state + update watch mtime ---- */
    con->has_error = false;

#if DEV_BUILD
    {
        SDL_PathInfo info;
        if (SDL_GetPathInfo(con->watch_path, &info)) {
            con->watch_mtime = info.modify_time;
        }
    }
#endif

    SDL_Log("hot-reload: success");
    return true;
}

/* ------------------------------------------------------------------ */
/*  Destroy                                                            */
/* ------------------------------------------------------------------ */

void dtr_console_destroy(dtr_console_t *con)
{
    if (con == NULL) {
        return;
    }

    /* Fire sys:quit before any cleanup */
    if (con->events != NULL && con->runtime != NULL && con->runtime->ctx != NULL) {
        dtr_event_emit(con->events, "sys:quit", JS_UNDEFINED);
        dtr_event_flush(con->events);
    }

    /* Fire sys:cart_unload */
    if (con->events != NULL && con->runtime != NULL && con->runtime->ctx != NULL) {
        dtr_event_emit(con->events, "sys:cart_unload", JS_UNDEFINED);
        dtr_event_flush(con->events);
    }

    /* Persist before teardown */
    dtr_cart_persist_save(con->cart);

    prv_console_cleanup(con);
}

/* ------------------------------------------------------------------ */
/*  Internal: load cart assets                                         */
/* ------------------------------------------------------------------ */

static bool prv_load_cart_assets(dtr_console_t *con)
{
    dtr_cart_t *cart;
    char        path_buf[1024];

    cart = con->cart;

    /* Sprites: load PNG → RGBA → quantise to palette sheet */
    if (cart->sprite_sheet_path[0] != '\0') {
        int32_t w = 0, h = 0;

        DTR_FREE(cart->sprite_rgba);
        cart->sprite_rgba = NULL;

        SDL_snprintf(path_buf, sizeof(path_buf), "%s%s",
                     cart->base_path, cart->sprite_sheet_path);
        cart->sprite_rgba   = dtr_import_png(path_buf, &w, &h);
        cart->sprite_rgba_w = w;
        cart->sprite_rgba_h = h;
    }
    if (cart->sprite_rgba != NULL) {
        dtr_gfx_load_sheet(con->graphics,
                           cart->sprite_rgba,
                           cart->sprite_rgba_w,
                           cart->sprite_rgba_h,
                           cart->sprites.tile_w,
                           cart->sprites.tile_h);
    }

    /* Maps: already parsed during cart_parse for baked carts */
    /* For dev carts, import from source files */
    if (!cart->baked) {
        for (int32_t idx = 0; idx < cart->map_count; ++idx) {
            const char *src;
            size_t      src_len;

            src     = cart->map_paths[idx];
            src_len = SDL_strlen(src);

            /* Build full path */
            SDL_snprintf(path_buf, sizeof(path_buf), "%s%s", cart->base_path, src);

            /* Detect format by extension */
            if (src_len > 4 && SDL_strcmp(src + src_len - 4, ".tmj") == 0) {
                dtr_import_tiled(path_buf, &cart->maps[idx], con->runtime->ctx);
            } else if (src_len > 5 && SDL_strcmp(src + src_len - 5, ".ldtk") == 0) {
                dtr_import_ldtk(path_buf, &cart->maps[idx], con->runtime->ctx);
            }
        }
    }

    /* SFX files */
    for (int32_t idx = 0; idx < cart->sfx_count; ++idx) {
        if (cart->sfx_paths[idx][0] != '\0') {
            size_t   len;
            uint8_t *data;

            SDL_snprintf(path_buf, sizeof(path_buf), "%s%s",
                         cart->base_path, cart->sfx_paths[idx]);
            data = (uint8_t *)SDL_LoadFile(path_buf, &len);
            if (data != NULL) {
                dtr_audio_load_sfx(con->audio, data, len);
                SDL_free(data);
            } else {
                SDL_Log("Failed to load sfx: %s", path_buf);
            }
        }
    }

    /* Music files */
    for (int32_t idx = 0; idx < cart->music_count; ++idx) {
        if (cart->music_paths[idx][0] != '\0') {
            size_t   len;
            uint8_t *data;

            SDL_snprintf(path_buf, sizeof(path_buf), "%s%s",
                         cart->base_path, cart->music_paths[idx]);
            data = (uint8_t *)SDL_LoadFile(path_buf, &len);
            if (data != NULL) {
                dtr_audio_load_music(con->audio, data, len);
                SDL_free(data);
            } else {
                SDL_Log("Failed to load music: %s", path_buf);
            }
        }
    }

    /* JS source code */
    if (cart->code == NULL && cart->code_path[0] != '\0') {
        size_t len;

        SDL_snprintf(path_buf, sizeof(path_buf), "%s%s", cart->base_path, cart->code_path);
        cart->code = (char *)SDL_LoadFile(path_buf, &len);
        if (cart->code != NULL) {
            cart->code_len = len;
        } else {
            SDL_Log("Failed to load code: %s", path_buf);
        }
    }

    return true;
}

/* ------------------------------------------------------------------ */
/*  Internal: pause overlay                                            */
/* ------------------------------------------------------------------ */

static void prv_render_pause_overlay(dtr_console_t *con)
{
    dtr_graphics_t *gfx;
    int32_t         center_x;
    int32_t         center_y;

    gfx = con->graphics;

    /* Dim the existing framebuffer */
    for (int32_t idx = 0; idx < con->fb_width * con->fb_height; ++idx) {
        gfx->framebuffer[idx] = 0; /* Black fill */
    }

    center_x = con->fb_width / 2 - 16;
    center_y = con->fb_height / 2 - 3;
    dtr_gfx_print(gfx, "PAUSED", center_x, center_y, 7);

    /* Upload to screen */
    {
        void   *locked;
        int     pitch;
        int32_t row_bytes;

        row_bytes = con->fb_width * (int32_t)sizeof(uint32_t);
        SDL_LockTexture(con->screen_tex, NULL, &locked, &pitch);
        if (pitch == row_bytes) {
            dtr_gfx_flip_to(gfx, (uint32_t *)locked);
        } else {
            dtr_gfx_flip(gfx);
            for (int32_t y = 0; y < con->fb_height; ++y) {
                SDL_memcpy((uint8_t *)locked + y * pitch,
                           gfx->pixels + y * con->fb_width,
                           (size_t)row_bytes);
            }
        }
        SDL_UnlockTexture(con->screen_tex);
    }
    SDL_RenderClear(con->renderer);
    SDL_RenderTexture(con->renderer, con->screen_tex, NULL, NULL);
    SDL_RenderPresent(con->renderer);
}

/* ------------------------------------------------------------------ */
/*  Internal: error overlay                                            */
/* ------------------------------------------------------------------ */

static void prv_render_error_overlay(dtr_console_t *con)
{
    dtr_graphics_t *gfx;

    gfx = con->graphics;

    /* Red-tinted background */
    SDL_memset(gfx->framebuffer, 0, (size_t)(con->fb_width * con->fb_height));

    dtr_gfx_rectfill(gfx, 0, 0, con->fb_width - 1, 12, 8);
    dtr_gfx_print(gfx, "RUNTIME ERROR", 2, 2, 7);
    dtr_gfx_print(gfx, con->runtime->error_msg, 2, 16, 7);

    {
        void   *locked;
        int     pitch;
        int32_t row_bytes;

        row_bytes = con->fb_width * (int32_t)sizeof(uint32_t);
        SDL_LockTexture(con->screen_tex, NULL, &locked, &pitch);
        if (pitch == row_bytes) {
            dtr_gfx_flip_to(gfx, (uint32_t *)locked);
        } else {
            dtr_gfx_flip(gfx);
            for (int32_t y = 0; y < con->fb_height; ++y) {
                SDL_memcpy((uint8_t *)locked + y * pitch,
                           gfx->pixels + y * con->fb_width,
                           (size_t)row_bytes);
            }
        }
        SDL_UnlockTexture(con->screen_tex);
    }
    SDL_RenderClear(con->renderer);
    SDL_RenderTexture(con->renderer, con->screen_tex, NULL, NULL);
    SDL_RenderPresent(con->renderer);
}
