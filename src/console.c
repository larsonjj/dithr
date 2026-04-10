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
#include "touch.h"
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

#if DEV_BUILD
    SDL_free(con->prev_code);
    con->prev_code     = NULL;
    con->prev_code_len = 0;
#endif
    dtr_postfx_destroy(con->postfx);
    dtr_event_destroy(con->events);
    dtr_input_destroy(con->input);
    dtr_touch_destroy(con->touch);
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
/*  Hot-reload: directory mtime scanning                               */
/* ------------------------------------------------------------------ */

#if DEV_BUILD

typedef struct {
    const char *dir;
    int64_t     max_mtime;
} prv_dir_scan_t;

static SDL_EnumerationResult SDLCALL prv_scan_js_cb(void       *userdata,
                                                    const char *dirname,
                                                    const char *fname)
{
    prv_dir_scan_t *scan;
    const char     *ext;
    char            full[1024];
    SDL_PathInfo    info;

    scan = (prv_dir_scan_t *)userdata;

    /* Skip editor temp / backup files */
    if (fname[0] == '.') {
        return SDL_ENUM_CONTINUE;
    }
    ext = SDL_strrchr(fname, '.');
    if (ext != NULL) {
        if (SDL_strcasecmp(ext, ".tmp") == 0 || SDL_strcasecmp(ext, ".swp") == 0 ||
            SDL_strcasecmp(ext, ".bak") == 0 || SDL_strcasecmp(ext, ".crswap") == 0) {
            return SDL_ENUM_CONTINUE;
        }
    }
    {
        size_t name_len;

        name_len = SDL_strlen(fname);
        if (name_len > 0 && fname[name_len - 1] == '~') {
            return SDL_ENUM_CONTINUE;
        }
    }

    SDL_snprintf(full, sizeof(full), "%s%s", dirname, fname);

    /* Recurse into subdirectories */
    if (SDL_GetPathInfo(full, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
        char subdir[1024];

        SDL_snprintf(subdir, sizeof(subdir), "%s/", full);
        SDL_EnumerateDirectory(subdir, prv_scan_js_cb, userdata);
        return SDL_ENUM_CONTINUE;
    }

    if (ext == NULL || SDL_strcasecmp(ext, ".js") != 0) {
        return SDL_ENUM_CONTINUE;
    }

    if (SDL_GetPathInfo(full, &info)) {
        if (info.modify_time > scan->max_mtime) {
            scan->max_mtime = info.modify_time;
        }
    }

    return SDL_ENUM_CONTINUE;
}

/**
 * \brief           Poll for .js file changes and manage debounce timer.
 *
 * Called from iterate() both in normal and error states.  When a file
 * change is detected and the debounce window has elapsed, sets
 * con->reload = true.
 */
static void prv_poll_file_changes(dtr_console_t *con)
{
    uint64_t ticks;
    double   elapsed_ms;

    if (con->watch_path[0] == '\0') {
        return;
    }

    ticks = SDL_GetPerformanceCounter();
    elapsed_ms =
        (double)(ticks - con->watch_last_poll) / (double)SDL_GetPerformanceFrequency() * 1000.0;

    if (elapsed_ms >= 200.0) {
        bool changed;

        changed              = false;
        con->watch_last_poll = ticks;

        if (con->watch_dir[0] != '\0') {
            prv_dir_scan_t scan;

            scan.dir       = con->watch_dir;
            scan.max_mtime = con->watch_mtime;
            SDL_EnumerateDirectory(con->watch_dir, prv_scan_js_cb, &scan);
            changed = (scan.max_mtime != con->watch_mtime);
        } else {
            SDL_PathInfo info;

            if (SDL_GetPathInfo(con->watch_path, &info)) {
                changed = (info.modify_time != con->watch_mtime);
            }
        }

        if (changed && !con->reload_pending) {
            con->reload_pending     = true;
            con->reload_detect_time = ticks;
        }
    }

    /* Debounce: fire reload after 300 ms of detected change */
    if (con->reload_pending) {
        double debounce_ms;

        debounce_ms = (double)(SDL_GetPerformanceCounter() - con->reload_detect_time) /
                      (double)SDL_GetPerformanceFrequency() * 1000.0;
        if (debounce_ms >= 300.0) {
            SDL_Log("hot-reload: change detected, reloading");
            con->reload         = true;
            con->reload_pending = false;
        }
    }
}

#endif /* DEV_BUILD */

/* ------------------------------------------------------------------ */
/*  Create                                                             */
/* ------------------------------------------------------------------ */

dtr_console_t *dtr_console_create(const char *cart_path)
{
    dtr_console_t *con;
    char          *json_data;
    size_t         json_len;

    con = DTR_CALLOC(1, sizeof(dtr_console_t));
    if (con == NULL) {
        return NULL;
    }

    /* Default PRNG seed (xorshift64 needs non-zero) */
    con->rng_state = 1;

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

    /* --- Update SDL app metadata from cart --- */
    SDL_SetAppMetadata(con->cart->meta.title[0] != '\0' ? con->cart->meta.title : "dithr",
                       con->cart->meta.version[0] != '\0' ? con->cart->meta.version :
                                                            CONSOLE_VERSION,
                       con->cart->meta.id[0] != '\0' ? con->cart->meta.id : "com.example.tbd");

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
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
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

    {
        SDL_RendererLogicalPresentation lp;
        switch (con->cart->display.scale_mode) {
            case 1:
                lp = SDL_LOGICAL_PRESENTATION_LETTERBOX;
                break;
            case 2:
                lp = SDL_LOGICAL_PRESENTATION_STRETCH;
                break;
            case 3:
                lp = SDL_LOGICAL_PRESENTATION_OVERSCAN;
                break;
            default:
                lp = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
                break;
        }
        SDL_SetRenderLogicalPresentation(con->renderer, con->fb_width, con->fb_height, lp);
    }

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
    if (con->cart->audio.channels > 0) {
        con->audio = dtr_audio_create(
            con->cart->audio.channels, con->cart->audio.frequency, con->cart->audio.buffer_size);
        if (con->audio == NULL) {
            SDL_Log("Warning: audio subsystem failed to initialise — audio disabled");
        }
    }

    /* --- Input subsystems --- */
    con->keys  = dtr_key_create();
    con->mouse = dtr_mouse_create();

    /* Ensure gamepad subsystem is initialised (SDL callbacks only auto-init EVENTS) */
    if (!SDL_WasInit(SDL_INIT_GAMEPAD)) {
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            SDL_Log("Warning: SDL_InitSubSystem(GAMEPAD) failed: %s", SDL_GetError());
        }
    }
    con->gamepads = dtr_gamepad_create();
    con->touch    = dtr_touch_create(con->fb_width, con->fb_height);
    con->input    = dtr_input_create();

    /* --- Mouse coordinate mapping (handled by SDL_ConvertEventToRenderCoordinates) --- */

    /* --- Event bus --- */
    con->events = dtr_event_create(con->runtime->ctx);

    /* --- PostFX --- */
    con->postfx = dtr_postfx_create(con->fb_width, con->fb_height);
    if (con->cart->meta.default_postfx[0] != '\0') {
        dtr_postfx_use(con->postfx, con->cart->meta.default_postfx);
    }

    /* --- Tween pool --- */
    dtr_tween_init(&con->tween);

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
        dtr_runtime_eval(con->runtime, con->cart->code, con->cart->code_len, con->cart->code_path);
    }

    /* --- Fire sys:cart_load --- */
    dtr_event_emit(con->events, "sys:cart_load", JS_UNDEFINED);
    dtr_event_flush(con->events);

    /* --- Call _init() --- */
    dtr_runtime_call(con->runtime, con->runtime->atom_init);

    /* --- Ready --- */
    con->running     = true;
    con->paused      = false;
    con->fullscreen  = false;
    con->frame_count = 0;
    con->time        = 0.0f;
    con->delta       = 0.0f;
    con->time_prev   = SDL_GetPerformanceCounter();

#if DEV_BUILD
    /* Set up file watcher for the JS source directory */
    con->watch_dir[0]        = '\0';
    con->watch_path[0]       = '\0';
    con->watch_mtime         = 0;
    con->watch_last_poll     = SDL_GetPerformanceCounter();
    con->reload_toast        = 0.0f;
    con->reload_toast_failed = false;
    con->reload_pending      = false;
    con->reload_detect_time  = 0;
    con->reload_count        = 0;
    con->prev_code           = NULL;
    con->prev_code_len       = 0;
    if (con->cart->code_path[0] != '\0') {
        SDL_PathInfo info;

        SDL_snprintf(con->watch_path,
                     sizeof(con->watch_path),
                     "%s%s",
                     con->cart->base_path,
                     con->cart->code_path);
        if (SDL_GetPathInfo(con->watch_path, &info)) {
            con->watch_mtime = info.modify_time;

            /* Extract directory from the full code path */
            SDL_strlcpy(con->watch_dir, con->watch_path, sizeof(con->watch_dir));
            {
                char *last_sep;

                last_sep = SDL_strrchr(con->watch_dir, '/');
                if (last_sep == NULL) {
                    last_sep = SDL_strrchr(con->watch_dir, '\\');
                }
                if (last_sep != NULL) {
                    *(last_sep + 1) = '\0';
                } else {
                    con->watch_dir[0] = '\0';
                }
            }

            /* Scan all .js files so watch_mtime covers the whole directory,
               preventing a spurious reload on the first poll cycle. */
            if (con->watch_dir[0] != '\0') {
                prv_dir_scan_t scan;

                scan.dir       = con->watch_dir;
                scan.max_mtime = con->watch_mtime;
                SDL_EnumerateDirectory(con->watch_dir, prv_scan_js_cb, &scan);
                con->watch_mtime = scan.max_mtime;
                SDL_Log("hot-reload: watching directory %s", con->watch_dir);
            } else {
                SDL_Log("hot-reload: watching %s", con->watch_path);
            }
        } else {
            SDL_Log("hot-reload: WARNING: cannot access %s — file watching disabled",
                    con->watch_path);
            con->watch_path[0] = '\0';
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
#ifdef __APPLE__
            /* Suppress the QUIT only when it was generated by the
             * macOS Cmd+W menu shortcut (WINDOW_CLOSE_REQUESTED
             * already set the flag).  Cmd+Q and the close-button
             * path leave the flag at 0, so they still quit. */
            if (con->mac_cmd_close_frames > 0) {
                break;
            }
#endif
            con->running = false;
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
#ifdef __APPLE__
            /* On macOS, Cmd+W fires the menu-bar Close action which
             * sends WINDOW_CLOSE_REQUESTED.  The W key-down was
             * already delivered by SDL but a key-up arriving in the
             * same frame clears it before JS sees the press.  Set a
             * flag so we can re-inject the press before JS _update. */
            if (SDL_GetModState() & SDL_KMOD_GUI) {
                con->mac_cmd_close_frames = 2;
                break;
            }
#endif
            break;

        case SDL_EVENT_KEY_DOWN: {
            dtr_key_t key;

            /* Copy error to clipboard: Ctrl+C while error overlay is shown */
            if (con->runtime->error_active && event->key.scancode == SDL_SCANCODE_C &&
                (event->key.mod & SDL_KMOD_CTRL)) {
                char clip_buf[2048];
                if (con->runtime->error_line > 0) {
                    SDL_snprintf(clip_buf,
                                 sizeof(clip_buf),
                                 "line %d: %s",
                                 con->runtime->error_line,
                                 con->runtime->error_msg);
                } else {
                    SDL_strlcpy(clip_buf, con->runtime->error_msg, sizeof(clip_buf));
                }
                SDL_SetClipboardText(clip_buf);
                break;
            }

            /* Hardcoded pause toggle: Escape */
            if (event->key.scancode == SDL_SCANCODE_ESCAPE && con->cart->display.pause_key) {
                con->paused = !con->paused;
                if (con->paused) {
                    dtr_event_emit(con->events, "sys:pause", JS_UNDEFINED);
                } else {
                    dtr_event_emit(con->events, "sys:resume", JS_UNDEFINED);
                }
                break;
            }
#if DEV_BUILD
            /* Manual hot-reload: F5 */
            if (event->key.scancode == SDL_SCANCODE_F5) {
                con->reload = true;
                break;
            }
            /* Manual asset-reload: F6 */
            if (event->key.scancode == SDL_SCANCODE_F6) {
                SDL_Log("asset-reload: F6 pressed");
                dtr_console_reload_assets(con);
                break;
            }
            /* Undo last reload: F4 */
            if (event->key.scancode == SDL_SCANCODE_F4) {
                if (con->prev_code != NULL) {
                    /* Swap current and previous code, then force a reload */
                    char  *tmp_code;
                    size_t tmp_len;

                    tmp_code = con->cart->code;
                    tmp_len  = con->cart->code_len;

                    con->cart->code     = con->prev_code;
                    con->cart->code_len = con->prev_code_len;

                    con->prev_code     = tmp_code;
                    con->prev_code_len = tmp_len;

                    /* Write the reverted code to disk so file watch doesn't re-trigger */
                    {
                        char full[1024];

                        SDL_snprintf(
                            full, sizeof(full), "%s%s", con->cart->base_path, con->cart->code_path);
                        if (!SDL_SaveFile(full, con->cart->code, con->cart->code_len)) {
                            SDL_Log(
                                "hot-reload: undo — failed to write %s: %s", full, SDL_GetError());
                        }
                    }

                    con->reload = true;
                    SDL_Log("hot-reload: undo — reverting to previous code");
                } else {
                    SDL_Log("hot-reload: undo — no previous code available");
                }
                break;
            }
#endif
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
                int idx                      = event->button.button - 1;
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
            dtr_gamepad_on_button(con->gamepads, event->gbutton.which, event->gbutton.button, true);
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            dtr_gamepad_on_button(
                con->gamepads, event->gbutton.which, event->gbutton.button, false);
            break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            dtr_gamepad_on_axis(
                con->gamepads, event->gaxis.which, event->gaxis.axis, event->gaxis.value);
            break;

        case SDL_EVENT_FINGER_DOWN:
            dtr_touch_on_down(
                con->touch, event->tfinger.fingerID, event->tfinger.x, event->tfinger.y,
                event->tfinger.pressure);
            break;

        case SDL_EVENT_FINGER_MOTION:
            dtr_touch_on_motion(
                con->touch, event->tfinger.fingerID, event->tfinger.x, event->tfinger.y,
                event->tfinger.dx, event->tfinger.dy, event->tfinger.pressure);
            break;

        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            dtr_touch_on_up(con->touch, event->tfinger.fingerID);
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
                /* dtr_event_emit takes ownership of payload — do NOT free here */
                dtr_event_emit(con->events, "text:input", payload);
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

#if DEV_BUILD
        /* Keep polling for file changes so auto-reload works during errors */
        prv_poll_file_changes(con);
#endif
        return;
    }

#ifdef __APPLE__
    /* Re-inject the W key that the macOS menu-close key-up cleared. */
    if (con->mac_cmd_close_frames == 2) {
        dtr_key_set(con->keys, DTR_KEY_W, true);
        con->mac_cmd_close_frames = 1;
    }
#endif

    /* Event bus flush */
    dtr_event_flush(con->events);

    /* JS _update(dt) */
    {
        uint64_t t0     = SDL_GetPerformanceCounter();
        JSValue  dt_arg = JS_NewFloat64(con->runtime->ctx, (double)con->delta);
        dtr_runtime_call_argv(con->runtime, con->runtime->atom_update, 1, &dt_arg);
        JS_FreeValue(con->runtime->ctx, dt_arg);
        con->update_ms =
            (float)((double)(SDL_GetPerformanceCounter() - t0) / (double)freq * 1000.0);
    }

    /* JS _draw */
    {
        uint64_t t0 = SDL_GetPerformanceCounter();
        dtr_runtime_call(con->runtime, con->runtime->atom_draw);
        con->draw_ms = (float)((double)(SDL_GetPerformanceCounter() - t0) / (double)freq * 1000.0);
    }

#if DEV_BUILD
    /* Reload toast overlay */
    if (con->reload_toast > 0.0f) {
        con->reload_toast -= con->delta;
        {
            dtr_graphics_t *gfx;
            int32_t         save_cx;
            int32_t         save_cy;

            gfx           = con->graphics;
            save_cx       = gfx->camera_x;
            save_cy       = gfx->camera_y;
            gfx->camera_x = 0;
            gfx->camera_y = 0;
            dtr_gfx_clip_reset(gfx);

            {
                char    toast_buf[32];
                int32_t toast_w;
                uint8_t toast_bg;

                if (con->reload_toast_failed) {
                    SDL_snprintf(toast_buf, sizeof(toast_buf), "RELOAD FAILED");
                    toast_bg = 8; /* red */
                } else {
                    SDL_snprintf(
                        toast_buf, sizeof(toast_buf), "RELOADED (%d)", (int)con->reload_count);
                    toast_bg = 11; /* green */
                }
                toast_w = (int32_t)SDL_strlen(toast_buf) * 5 + 3;
                dtr_gfx_rectfill(gfx, 0, 0, toast_w, 8, toast_bg);
                dtr_gfx_print(gfx, toast_buf, 2, 1, 0);
            }

            gfx->camera_x = save_cx;
            gfx->camera_y = save_cy;
        }
    }
#endif

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
            dtr_postfx_apply(con->postfx, (uint32_t *)locked, con->fb_width, con->fb_height);
        } else {
            dtr_gfx_flip(con->graphics);
            dtr_gfx_transition_update(con->graphics);
            dtr_postfx_apply(con->postfx, con->graphics->pixels, con->fb_width, con->fb_height);
            for (int32_t y = 0; y < con->fb_height; ++y) {
                SDL_memcpy((uint8_t *)locked + (ptrdiff_t)y * pitch,
                           con->graphics->pixels + (ptrdiff_t)y * con->fb_width,
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

#ifdef __APPLE__
    /* Clear the re-injected W press so it doesn't stick. */
    if (con->mac_cmd_close_frames == 1) {
        con->keys->current[DTR_KEY_W] = false;
        con->mac_cmd_close_frames     = 0;
    }
#endif

    /* Input update — must happen AFTER JS _update/_draw so that btnp()
       edge detection (current && !previous) is visible during the frame. */
    dtr_key_update(con->keys, con->delta);
    dtr_mouse_update(con->mouse);

    /* Reset per-frame mouse deltas after JS has consumed them */
    con->mouse->dx      = 0.0f;
    con->mouse->dy      = 0.0f;
    con->mouse->wheel_x = 0.0f;
    con->mouse->wheel   = 0.0f;
    dtr_touch_update(con->touch);
    dtr_gamepad_update(con->gamepads);
    dtr_input_update(con->input, con->keys, con->gamepads);

    ++con->frame_count;

#if DEV_BUILD
    /* Hot-reload: poll JS source directory for changes (every 200 ms) */
    prv_poll_file_changes(con);
#endif
}

/* ------------------------------------------------------------------ */
/*  Hot reload (JS only)                                               */
/* ------------------------------------------------------------------ */

bool dtr_console_reload(dtr_console_t *con)
{
    char          *saved_json = NULL;
    char          *new_code   = NULL;
    size_t         new_len    = 0;
    char           code_full[1024];
    dtr_runtime_t *new_rt = NULL;
    uint64_t       t_start;

    /* PostFX snapshot for auto-preservation */
    dtr_postfx_entry_t saved_pfx[DTR_POSTFX_MAX_STACK];
    int32_t            saved_pfx_count = 0;

    /* Camera / draw state snapshot */
    int32_t  saved_cam_x    = 0;
    int32_t  saved_cam_y    = 0;
    uint8_t  saved_color    = 0;
    int32_t  saved_clip_x   = 0;
    int32_t  saved_clip_y   = 0;
    int32_t  saved_clip_w   = 0;
    int32_t  saved_clip_h   = 0;
    int32_t  saved_cursor_x = 0;
    int32_t  saved_cursor_y = 0;
    uint16_t saved_fill_pat = 0;

    /* Palette state snapshot */
    uint8_t saved_draw_pal[CONSOLE_PALETTE_SIZE];
    uint8_t saved_screen_pal[CONSOLE_PALETTE_SIZE];
    bool    saved_transparent[CONSOLE_PALETTE_SIZE];

    /* Custom font snapshot */
    dtr_custom_font_t saved_custom_font = {0};

    if (con == NULL) {
        return false;
    }

    t_start = SDL_GetPerformanceCounter();

    /* ---- 1. Read new JS source from disk (before destroying anything) ---- */
    SDL_snprintf(code_full, sizeof(code_full), "%s%s", con->cart->base_path, con->cart->code_path);
    new_code = (char *)SDL_LoadFile(code_full, &new_len);
    if (new_code == NULL) {
        SDL_Log("hot-reload: failed to read %s", code_full);
#if DEV_BUILD
        con->reload_toast        = 1.5f;
        con->reload_toast_failed = true;
#endif
        return false;
    }

    /* ---- 2. Call _save() to capture game state (optional) ---- */
    if (con->runtime != NULL && con->runtime->ctx != NULL && !con->runtime->error_active) {
        JSValue global;
        JSValue func;

        global = JS_GetGlobalObject(con->runtime->ctx);
        func   = JS_GetProperty(con->runtime->ctx, global, con->runtime->atom_save);

        if (JS_IsFunction(con->runtime->ctx, func)) {
            JSValue result;

            result = JS_Call(con->runtime->ctx, func, global, 0, NULL);
            if (JS_IsException(result)) {
                SDL_Log("hot-reload: _save() threw an exception, state not preserved");
            } else if (!JS_IsUndefined(result)) {
                JSValue json_val;
                JSValue stringify;
                JSValue json_obj;

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

    /* ---- 2b. Snapshot camera / draw state ---- */
    if (con->graphics != NULL) {
        saved_cam_x    = con->graphics->camera_x;
        saved_cam_y    = con->graphics->camera_y;
        saved_color    = con->graphics->color;
        saved_clip_x   = con->graphics->clip_x;
        saved_clip_y   = con->graphics->clip_y;
        saved_clip_w   = con->graphics->clip_w;
        saved_clip_h   = con->graphics->clip_h;
        saved_cursor_x = con->graphics->cursor_x;
        saved_cursor_y = con->graphics->cursor_y;
        saved_fill_pat = con->graphics->fill_pattern;
        SDL_memcpy(saved_draw_pal, con->graphics->draw_pal, sizeof(saved_draw_pal));
        SDL_memcpy(saved_screen_pal, con->graphics->screen_pal, sizeof(saved_screen_pal));
        SDL_memcpy(saved_transparent, con->graphics->transparent, sizeof(saved_transparent));
        saved_custom_font = con->graphics->custom_font;
    }

    /* ---- 2c. Snapshot postfx stack ---- */
    if (con->postfx != NULL && con->postfx->count > 0) {
        saved_pfx_count = con->postfx->count;
        SDL_memcpy(
            saved_pfx, con->postfx->stack, (size_t)saved_pfx_count * sizeof(dtr_postfx_entry_t));
    }

    /* ---- 3. Create new runtime in temporary ---- */
    new_rt = dtr_runtime_create(con,
                                (int32_t)(con->cart->runtime.mem_limit / (1024u * 1024u)),
                                (int32_t)(con->cart->runtime.stack_limit / 1024u));
    if (new_rt == NULL) {
        SDL_Log("hot-reload: failed to create runtime");
        SDL_free(new_code);
        SDL_free(saved_json);
#if DEV_BUILD
        con->reload_toast        = 1.5f;
        con->reload_toast_failed = true;
#endif
        return false;
    }

    /* ---- 4. Register APIs and evaluate new code on temp runtime ---- */
    dtr_api_register_all(new_rt);

    if (!dtr_runtime_eval(new_rt, new_code, new_len, con->cart->code_path)) {
        SDL_Log("hot-reload: eval failed — keeping old code");
        dtr_runtime_destroy(new_rt);
        SDL_free(new_code);
        SDL_free(saved_json);
#if DEV_BUILD
        con->reload_toast        = 1.5f;
        con->reload_toast_failed = true;
#endif
        /* Old runtime + events stay intact; engine keeps running */
        return false;
    }

    /* ---- 5. Eval succeeded — commit: tear down old runtime, keep event bus ---- */
    if (con->events != NULL && con->runtime != NULL && con->runtime->ctx != NULL) {
        dtr_event_emit(con->events, "sys:cart_unload", JS_UNDEFINED);
        dtr_event_flush(con->events);
    }

    dtr_event_clear(con->events, new_rt->ctx);
    dtr_runtime_destroy(con->runtime);

    /* Stash previous code for undo */
#if DEV_BUILD
    SDL_free(con->prev_code);
    con->prev_code     = con->cart->code; /* transfer ownership */
    con->prev_code_len = con->cart->code_len;
#else
    SDL_free(con->cart->code);
#endif

    /* Replace cart code buffer */
    con->cart->code     = new_code;
    con->cart->code_len = new_len;

    /* Install new runtime */
    con->runtime   = new_rt;
    con->cart->ctx = con->runtime->ctx;

    /* ---- 6. Call _restore(state) if we have saved state ---- */
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
                if (JS_IsException(res)) {
                    SDL_Log("hot-reload: _restore() threw an exception");
                }
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

    /* ---- 6b. Restore postfx stack ---- */
    if (saved_pfx_count > 0 && con->postfx != NULL) {
        con->postfx->count = saved_pfx_count;
        SDL_memcpy(
            con->postfx->stack, saved_pfx, (size_t)saved_pfx_count * sizeof(dtr_postfx_entry_t));
    }

    /* ---- 6c. Restore camera / draw state ---- */
    if (con->graphics != NULL) {
        con->graphics->camera_x     = saved_cam_x;
        con->graphics->camera_y     = saved_cam_y;
        con->graphics->color        = saved_color;
        con->graphics->clip_x       = saved_clip_x;
        con->graphics->clip_y       = saved_clip_y;
        con->graphics->clip_w       = saved_clip_w;
        con->graphics->clip_h       = saved_clip_h;
        con->graphics->cursor_x     = saved_cursor_x;
        con->graphics->cursor_y     = saved_cursor_y;
        con->graphics->fill_pattern = saved_fill_pat;
        SDL_memcpy(con->graphics->draw_pal, saved_draw_pal, sizeof(saved_draw_pal));
        SDL_memcpy(con->graphics->screen_pal, saved_screen_pal, sizeof(saved_screen_pal));
        SDL_memcpy(con->graphics->transparent, saved_transparent, sizeof(saved_transparent));
        con->graphics->custom_font = saved_custom_font;

        /* Reset in-progress transition to avoid stale visuals */
        SDL_memset(&con->graphics->transition, 0, sizeof(con->graphics->transition));

        /* Reset draw list in case reload hit mid-batch */
        con->graphics->draw_list.active = false;
        con->graphics->draw_list.count  = 0;
    }

    /* ---- 7. Call _init() ---- */
    dtr_runtime_call(con->runtime, con->runtime->atom_init);

    /* ---- 8. Fire sys:cart_load ---- */
    dtr_event_emit(con->events, "sys:cart_load", JS_UNDEFINED);
    dtr_event_flush(con->events);

    /* ---- 9. Update watch mtime ---- */

#if DEV_BUILD
    {
        SDL_PathInfo info;

        if (SDL_GetPathInfo(con->watch_path, &info)) {
            con->watch_mtime = info.modify_time;
        }

        /* If watching a directory, scan all .js files for the max mtime */
        if (con->watch_dir[0] != '\0') {
            prv_dir_scan_t scan;

            scan.dir       = con->watch_dir;
            scan.max_mtime = con->watch_mtime;
            SDL_EnumerateDirectory(con->watch_dir, prv_scan_js_cb, &scan);
            con->watch_mtime = scan.max_mtime;
        }
    }
#endif

    /* ---- 10. Log result with timing ---- */
    {
        uint64_t t_end;
        double   elapsed_ms;

        t_end      = SDL_GetPerformanceCounter();
        elapsed_ms = (double)(t_end - t_start) / (double)SDL_GetPerformanceFrequency() * 1000.0;
        SDL_Log("hot-reload: success (%.1f ms)", elapsed_ms);
    }

#if DEV_BUILD
    ++con->reload_count;
    con->reload_toast        = 1.0f; /* show "RELOADED" for 1 second */
    con->reload_toast_failed = false;
#endif

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

    /* Sprites: load sheet from PNG or hex file */
    if (cart->sprite_sheet_path[0] != '\0') {
        size_t path_len;

        SDL_snprintf(path_buf, sizeof(path_buf), "%s%s", cart->base_path, cart->sprite_sheet_path);
        path_len = SDL_strlen(cart->sprite_sheet_path);

        if (path_len > 4 && SDL_strcmp(cart->sprite_sheet_path + path_len - 4, ".hex") == 0) {
            /* Hex-encoded palette-indexed sheet */
            size_t hex_len = 0;
            char  *hex     = (char *)SDL_LoadFile(path_buf, &hex_len);

            if (hex != NULL) {
                dtr_gfx_load_sheet_hex(con->graphics,
                                       hex,
                                       hex_len,
                                       cart->sprites.sheet_w,
                                       cart->sprites.sheet_h,
                                       cart->sprites.tile_w,
                                       cart->sprites.tile_h);
                SDL_free(hex);
            }
        } else {
            /* PNG → RGBA → quantise to palette sheet */
            int32_t w = 0, h = 0;

            DTR_FREE(cart->sprite_rgba);
            cart->sprite_rgba = NULL;

            cart->sprite_rgba   = dtr_import_png(path_buf, &w, &h);
            cart->sprite_rgba_w = w;
            cart->sprite_rgba_h = h;

            if (cart->sprite_rgba != NULL) {
                dtr_gfx_load_sheet(con->graphics,
                                   cart->sprite_rgba,
                                   cart->sprite_rgba_w,
                                   cart->sprite_rgba_h,
                                   cart->sprites.tile_w,
                                   cart->sprites.tile_h);
            }
        }
    }

    /* Sprite flags: load from hex if configured */
    if (cart->sprite_flags_path[0] != '\0') {
        size_t flags_len = 0;
        char  *flags_hex;

        SDL_snprintf(path_buf, sizeof(path_buf), "%s%s", cart->base_path, cart->sprite_flags_path);
        flags_hex = (char *)SDL_LoadFile(path_buf, &flags_len);
        if (flags_hex != NULL) {
            dtr_gfx_load_flags_hex(con->graphics, flags_hex, flags_len);
            SDL_free(flags_hex);
        }
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

            SDL_snprintf(path_buf, sizeof(path_buf), "%s%s", cart->base_path, cart->sfx_paths[idx]);
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

            SDL_snprintf(
                path_buf, sizeof(path_buf), "%s%s", cart->base_path, cart->music_paths[idx]);
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
/*  Asset hot reload                                                   */
/* ------------------------------------------------------------------ */

bool dtr_console_reload_assets(dtr_console_t *con)
{
    uint64_t t_start;
    uint64_t t_end;
    double   elapsed_ms;

    if (con == NULL || con->cart == NULL) {
        return false;
    }

    t_start = SDL_GetPerformanceCounter();

    /* Stop all playback before clearing audio assets (prevents use-after-free) */
    if (con->audio != NULL) {
        dtr_sfx_stop(con->audio, -1);
        dtr_mus_stop(con->audio, 0);
        dtr_audio_clear_music(con->audio);
        dtr_audio_clear_sfx(con->audio);
    }

    /* Free existing map data to avoid leaks */
    if (!con->cart->baked) {
        for (int32_t idx = 0; idx < con->cart->map_count; ++idx) {
            dtr_cart_free_map(con->cart, idx);
        }
    }

    prv_load_cart_assets(con);

    t_end      = SDL_GetPerformanceCounter();
    elapsed_ms = (double)(t_end - t_start) / (double)SDL_GetPerformanceFrequency() * 1000.0;
    SDL_Log("asset-reload: success (%.1f ms)", elapsed_ms);

    if (con->events != NULL) {
        dtr_event_emit(con->events, "sys:assets_reloaded", JS_UNDEFINED);
        dtr_event_flush(con->events);
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
                SDL_memcpy((uint8_t *)locked + (ptrdiff_t)y * pitch,
                           gfx->pixels + (ptrdiff_t)y * con->fb_width,
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
    char            line_buf[64];
    const char     *msg;
    int32_t         y_pos;
    int32_t         max_chars;

    gfx = con->graphics;

    /* Black background */
    SDL_memset(gfx->framebuffer, 0, (size_t)con->fb_width * (size_t)con->fb_height);

    /* Red header bar */
    dtr_gfx_rectfill(gfx, 0, 0, con->fb_width - 1, 10, 8);
    dtr_gfx_print(gfx, "RUNTIME ERROR", 2, 2, 7);

    /* Line number (right-aligned in header) */
    if (con->runtime->error_line > 0) {
        int32_t tw;

        SDL_snprintf(line_buf, sizeof(line_buf), "line %d", con->runtime->error_line);
        tw = dtr_gfx_text_width(gfx, line_buf);
        dtr_gfx_print(gfx, line_buf, con->fb_width - tw - 2, 2, 7);
    }

    /* Word-wrapped error message */
    max_chars = (con->fb_width - 4) / 4; /* 4px per char in default font */
    if (max_chars < 1) {
        max_chars = 1;
    }

    msg   = con->runtime->error_msg;
    y_pos = 14;

    while (*msg != '\0' && y_pos < con->fb_height - 8) {
        char    wrap_buf[256];
        int32_t len;
        int32_t chunk;

        len = (int32_t)SDL_strlen(msg);

        /* Find newline within range */
        chunk = (len < max_chars) ? len : max_chars;
        {
            int32_t ci;

            for (ci = 0; ci < chunk; ++ci) {
                if (msg[ci] == '\n') {
                    chunk = ci;
                    break;
                }
            }
        }

        if (chunk > (int32_t)(sizeof(wrap_buf) - 1)) {
            chunk = (int32_t)(sizeof(wrap_buf) - 1);
        }
        SDL_memcpy(wrap_buf, msg, (size_t)chunk);
        wrap_buf[chunk] = '\0';

        dtr_gfx_print(gfx, wrap_buf, 2, y_pos, 7);
        y_pos += 7;

        msg += chunk;
        if (*msg == '\n') {
            ++msg;
        }
    }

    /* Hint at bottom */
#ifdef __EMSCRIPTEN__
    dtr_gfx_print(gfx, "Ctrl+C copy", 2, con->fb_height - 8, 6);
#elif DEV_BUILD
    dtr_gfx_print(gfx, "Ctrl+C copy  F5 retry", 2, con->fb_height - 8, 6);
#else
    dtr_gfx_print(gfx, "Ctrl+C copy", 2, con->fb_height - 8, 6);
#endif

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
                SDL_memcpy((uint8_t *)locked + (ptrdiff_t)y * pitch,
                           gfx->pixels + (ptrdiff_t)y * con->fb_width,
                           (size_t)row_bytes);
            }
        }
        SDL_UnlockTexture(con->screen_tex);
    }
    SDL_RenderClear(con->renderer);
    SDL_RenderTexture(con->renderer, con->screen_tex, NULL, NULL);
    SDL_RenderPresent(con->renderer);
}
