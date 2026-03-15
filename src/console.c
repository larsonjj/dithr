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

static bool prv_load_cart_assets(mvn_console_t *con);
static void prv_render_pause_overlay(mvn_console_t *con);
static void prv_render_error_overlay(mvn_console_t *con);

/* ------------------------------------------------------------------ */
/*  Create                                                             */
/* ------------------------------------------------------------------ */

mvn_console_t *mvn_console_create(const char *cart_path)
{
    mvn_console_t *con;
    char *         json_data;
    size_t         json_len;

    con = MVN_CALLOC(1, sizeof(mvn_console_t));
    if (con == NULL) {
        return NULL;
    }

    /* Store cart path for restart support */
    SDL_strlcpy(con->cart_path, cart_path, sizeof(con->cart_path));

    /* --- Cart (create + defaults) --- */
    con->cart = mvn_cart_create();
    if (con->cart == NULL) {
        MVN_FREE(con);
        return NULL;
    }
    mvn_cart_defaults(con->cart);

    /* --- JS runtime (temporary for JSON parsing) --- */
    con->runtime = mvn_runtime_create(con,
                                      (int32_t)(con->cart->runtime.mem_limit / (1024u * 1024u)),
                                      (int32_t)(con->cart->runtime.stack_limit / 1024u));
    if (con->runtime == NULL) {
        mvn_cart_destroy(con->cart);
        MVN_FREE(con);
        return NULL;
    }

    /* --- Load and parse cart.json --- */
    json_data = (char *)SDL_LoadFile(cart_path, &json_len);
    if (json_data != NULL) {
        if (!mvn_cart_parse(con->cart, con->runtime->ctx, json_data, json_len)) {
            SDL_Log("Warning: cart parse failed, using defaults");
        }
        SDL_free(json_data);

        if (!mvn_cart_validate(con->cart)) {
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
        mvn_runtime_destroy(con->runtime);
        mvn_cart_destroy(con->cart);
        MVN_FREE(con);
        return NULL;
    }

    con->renderer = SDL_CreateRenderer(con->window, NULL);
    if (con->renderer == NULL) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(con->window);
        mvn_runtime_destroy(con->runtime);
        mvn_cart_destroy(con->cart);
        MVN_FREE(con);
        return NULL;
    }

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
        SDL_DestroyRenderer(con->renderer);
        SDL_DestroyWindow(con->window);
        mvn_runtime_destroy(con->runtime);
        mvn_cart_destroy(con->cart);
        MVN_FREE(con);
        return NULL;
    }
    SDL_SetTextureScaleMode(con->screen_tex, SDL_SCALEMODE_NEAREST);

    /* --- Graphics subsystem --- */
    con->graphics = mvn_gfx_create(con->fb_width, con->fb_height);
    if (con->graphics == NULL) {
        SDL_DestroyTexture(con->screen_tex);
        SDL_DestroyRenderer(con->renderer);
        SDL_DestroyWindow(con->window);
        mvn_runtime_destroy(con->runtime);
        mvn_cart_destroy(con->cart);
        MVN_FREE(con);
        return NULL;
    }

    /* --- Audio subsystem --- */
    con->audio = mvn_audio_create(
        con->cart->audio.channels, con->cart->audio.frequency, con->cart->audio.buffer_size);
    if (con->audio == NULL) {
        SDL_Log("Warning: audio subsystem failed to initialise — audio disabled");
    }

    /* --- Input subsystems --- */
    con->keys     = mvn_key_create();
    con->mouse    = mvn_mouse_create();
    con->gamepads = mvn_gamepad_create();
    con->input    = mvn_input_create();

    /* --- Mouse coordinate mapping (handled by SDL_ConvertEventToRenderCoordinates) --- */

    /* --- Event bus --- */
    con->events = mvn_event_create(con->runtime->ctx);

    /* --- PostFX --- */
    con->postfx = mvn_postfx_create(con->fb_width, con->fb_height);
    if (con->cart->meta.default_postfx[0] != '\0') {
        mvn_postfx_use(con->postfx, con->cart->meta.default_postfx);
    }

    /* --- Cart ref to JS context --- */
    con->cart->ctx = con->runtime->ctx;

    /* --- Load assets (sprites, maps, sfx, music) --- */
    prv_load_cart_assets(con);

    /* --- Load persisted save data (dslots + kv) --- */
    mvn_cart_persist_load(con->cart);

    /* --- Register all JS APIs --- */
    mvn_api_register_all(con->runtime);

    /* --- Apply default input mappings from cart.json --- */
    for (int32_t mi = 0; mi < con->cart->input.mapping_count; ++mi) {
        mvn_cart_input_mapping_t *mapping;
        mvn_binding_t             bindings[MVN_INPUT_MAX_BINDINGS];
        int32_t                   valid;

        mapping = &con->cart->input.mappings[mi];
        valid   = 0;

        for (int32_t bi = 0; bi < mapping->bind_count && valid < MVN_INPUT_MAX_BINDINGS; ++bi) {
            if (mvn_input_parse_binding(mapping->bindings[bi], &bindings[valid])) {
                ++valid;
            } else {
                SDL_Log("Unknown input binding: %s", mapping->bindings[bi]);
            }
        }

        if (valid > 0) {
            mvn_input_map(con->input, mapping->action, bindings, valid);
        }
    }

    /* --- Evaluate cart JS source --- */
    if (con->cart->code != NULL && con->cart->code_len > 0) {
        mvn_runtime_eval(con->runtime, con->cart->code, con->cart->code_len, "main.js");
    }

    /* --- Fire sys:cart_load --- */
    mvn_event_emit(con->events, "sys:cart_load", JS_UNDEFINED);
    mvn_event_flush(con->events);

    /* --- Call _init() --- */
    mvn_runtime_call(con->runtime, con->runtime->atom_init);

    /* --- Ready --- */
    con->running     = true;
    con->paused      = false;
    con->has_error   = false;
    con->fullscreen  = false;
    con->frame_count = 0;
    con->time        = 0.0f;
    con->delta       = 0.0f;
    con->time_prev   = SDL_GetPerformanceCounter();

    return con;
}

/* ------------------------------------------------------------------ */
/*  Per-event handler (called by SDL_AppEvent)                         */
/* ------------------------------------------------------------------ */

void mvn_console_event(mvn_console_t *con, SDL_Event *event)
{
    /* Let SDL handle logical-presentation coordinate conversion (letterbox, etc.) */
    SDL_ConvertEventToRenderCoordinates(con->renderer, event);

    switch (event->type) {
        case SDL_EVENT_QUIT:
            con->running = false;
            break;

        case SDL_EVENT_KEY_DOWN: {
            mvn_key_t key;

            /* Hardcoded pause toggle: Escape */
            if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
                con->paused = !con->paused;
                if (con->paused) {
                    mvn_event_emit(con->events, "sys:pause", JS_UNDEFINED);
                } else {
                    mvn_event_emit(con->events, "sys:resume", JS_UNDEFINED);
                }
                break;
            }
            /* Fullscreen toggle: F11 */
            if (event->key.scancode == SDL_SCANCODE_F11) {
                con->fullscreen = !con->fullscreen;
                SDL_SetWindowFullscreen(con->window, con->fullscreen);
                mvn_event_emit(con->events, "sys:fullscreen", JS_UNDEFINED);
                break;
            }

            key = mvn_key_from_scancode(event->key.scancode);
            if (key != MVN_KEY_NONE) {
                mvn_key_set(con->keys, key, true);
            }
            break;
        }

        case SDL_EVENT_KEY_UP: {
            mvn_key_t key;

            key = mvn_key_from_scancode(event->key.scancode);
            if (key != MVN_KEY_NONE) {
                mvn_key_set(con->keys, key, false);
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
            if (event->button.button >= 1 && event->button.button <= MVN_MOUSE_BTN_COUNT) {
                con->mouse->btn_current[event->button.button - 1] = true;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.button >= 1 && event->button.button <= MVN_MOUSE_BTN_COUNT) {
                con->mouse->btn_current[event->button.button - 1] = false;
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            con->mouse->wheel_x += event->wheel.x;
            con->mouse->wheel += event->wheel.y;
            break;

        case SDL_EVENT_GAMEPAD_ADDED:
            mvn_gamepad_on_added(con->gamepads, event->gdevice.which);
            break;

        case SDL_EVENT_GAMEPAD_REMOVED:
            mvn_gamepad_on_removed(con->gamepads, event->gdevice.which);
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            mvn_event_emit(con->events, "sys:focus_lost", JS_UNDEFINED);
            break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            mvn_event_emit(con->events, "sys:focus_gained", JS_UNDEFINED);
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            mvn_event_emit(con->events, "sys:resize", JS_UNDEFINED);
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
                mvn_event_emit(con->events, "text:input", payload);
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

void mvn_console_iterate(mvn_console_t *con)
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

    /* Reset per-frame mouse deltas at the start of each frame */
    con->mouse->dx      = 0.0f;
    con->mouse->dy      = 0.0f;
    con->mouse->wheel_x = 0.0f;
    con->mouse->wheel   = 0.0f;

    if (con->paused) {
        prv_render_pause_overlay(con);
        return;
    }

    if (con->runtime->error_active) {
        prv_render_error_overlay(con);
        return;
    }

    /* Input update */
    mvn_key_update(con->keys);
    mvn_mouse_update(con->mouse);
    mvn_gamepad_update(con->gamepads);
    mvn_input_update(con->input, con->keys, con->gamepads);

    /* Event bus flush */
    mvn_event_flush(con->events);

    /* JS _update(dt) */
    {
        JSValue dt_arg = JS_NewFloat64(con->runtime->ctx, (double)con->delta);
        mvn_runtime_call_argv(con->runtime, con->runtime->atom_update, 1, &dt_arg);
        JS_FreeValue(con->runtime->ctx, dt_arg);
    }

    /* JS _draw */
    mvn_runtime_call(con->runtime, con->runtime->atom_draw);

    /* PostFX */
    mvn_gfx_flip(con->graphics);
    mvn_postfx_apply(con->postfx, con->graphics->pixels, con->fb_width, con->fb_height);

    /* Upload to texture and present */
    SDL_UpdateTexture(con->screen_tex,
                      NULL,
                      con->graphics->pixels,
                      con->fb_width * (int32_t)sizeof(uint32_t));
    SDL_RenderClear(con->renderer);
    SDL_RenderTexture(con->renderer, con->screen_tex, NULL, NULL);
    SDL_RenderPresent(con->renderer);

    /* Drain microtasks */
    mvn_runtime_drain_jobs(con->runtime);

    ++con->frame_count;
}

/* ------------------------------------------------------------------ */
/*  Destroy                                                            */
/* ------------------------------------------------------------------ */

void mvn_console_destroy(mvn_console_t *con)
{
    if (con == NULL) {
        return;
    }

    /* Fire sys:quit before any cleanup */
    if (con->events != NULL && con->runtime != NULL && con->runtime->ctx != NULL) {
        mvn_event_emit(con->events, "sys:quit", JS_UNDEFINED);
        mvn_event_flush(con->events);
    }

    /* Fire sys:cart_unload */
    if (con->events != NULL && con->runtime != NULL && con->runtime->ctx != NULL) {
        mvn_event_emit(con->events, "sys:cart_unload", JS_UNDEFINED);
        mvn_event_flush(con->events);
    }

    /* Subsystems */
    mvn_postfx_destroy(con->postfx);
    mvn_event_destroy(con->events);
    mvn_input_destroy(con->input);
    mvn_gamepad_destroy(con->gamepads);
    mvn_mouse_destroy(con->mouse);
    mvn_key_destroy(con->keys);
    mvn_audio_destroy(con->audio);
    mvn_gfx_destroy(con->graphics);
    mvn_cart_persist_save(con->cart);
    mvn_cart_destroy(con->cart);
    mvn_runtime_destroy(con->runtime);

    /* SDL */
    if (con->screen_tex != NULL) {
        SDL_DestroyTexture(con->screen_tex);
    }
    if (con->renderer != NULL) {
        SDL_DestroyRenderer(con->renderer);
    }
    if (con->window != NULL) {
        SDL_DestroyWindow(con->window);
    }

    MVN_FREE(con);
}

/* ------------------------------------------------------------------ */
/*  Internal: load cart assets                                         */
/* ------------------------------------------------------------------ */

static bool prv_load_cart_assets(mvn_console_t *con)
{
    mvn_cart_t *cart;
    char        path_buf[1024];

    cart = con->cart;

    /* Sprites: load PNG → RGBA → quantise to palette sheet */
    if (cart->sprite_rgba != NULL) {
        mvn_gfx_load_sheet(con->graphics,
                           cart->sprite_rgba,
                           cart->sprite_w,
                           cart->sprite_h,
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
                mvn_import_tiled(path_buf, &cart->maps[idx], con->runtime->ctx);
            } else if (src_len > 5 && SDL_strcmp(src + src_len - 5, ".ldtk") == 0) {
                mvn_import_ldtk(path_buf, &cart->maps[idx], con->runtime->ctx);
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

static void prv_render_pause_overlay(mvn_console_t *con)
{
    mvn_graphics_t *gfx;
    int32_t         center_x;
    int32_t         center_y;

    gfx = con->graphics;

    /* Dim the existing framebuffer */
    for (int32_t idx = 0; idx < con->fb_width * con->fb_height; ++idx) {
        gfx->framebuffer[idx] = 0; /* Black fill */
    }

    center_x = con->fb_width / 2 - 16;
    center_y = con->fb_height / 2 - 3;
    mvn_gfx_print(gfx, "PAUSED", center_x, center_y, 7);

    /* Upload to screen */
    mvn_gfx_flip(gfx);
    SDL_UpdateTexture(
        con->screen_tex, NULL, gfx->pixels, con->fb_width * (int32_t)sizeof(uint32_t));
    SDL_RenderClear(con->renderer);
    SDL_RenderTexture(con->renderer, con->screen_tex, NULL, NULL);
    SDL_RenderPresent(con->renderer);
}

/* ------------------------------------------------------------------ */
/*  Internal: error overlay                                            */
/* ------------------------------------------------------------------ */

static void prv_render_error_overlay(mvn_console_t *con)
{
    mvn_graphics_t *gfx;

    gfx = con->graphics;

    /* Red-tinted background */
    SDL_memset(gfx->framebuffer, 0, (size_t)(con->fb_width * con->fb_height));

    mvn_gfx_rectfill(gfx, 0, 0, con->fb_width - 1, 12, 8);
    mvn_gfx_print(gfx, "RUNTIME ERROR", 2, 2, 7);
    mvn_gfx_print(gfx, con->runtime->error_msg, 2, 16, 7);

    mvn_gfx_flip(gfx);
    SDL_UpdateTexture(
        con->screen_tex, NULL, gfx->pixels, con->fb_width * (int32_t)sizeof(uint32_t));
    SDL_RenderClear(con->renderer);
    SDL_RenderTexture(con->renderer, con->screen_tex, NULL, NULL);
    SDL_RenderPresent(con->renderer);
}
