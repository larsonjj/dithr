/**
 * \file            console.h
 * \brief           DTR Console — core state, compile-time defaults, memory macros
 */

#ifndef DTR_CONSOLE_H
#define DTR_CONSOLE_H

#include "console_defs.h"
#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Override the stdlib-based memory macros with SDL equivalents so that all  */
/* allocations go through SDL's allocator (custom allocator support, etc.).  */
#undef DTR_MALLOC
#undef DTR_CALLOC
#undef DTR_REALLOC
#undef DTR_FREE
#define DTR_MALLOC(size)        SDL_malloc(size)
#define DTR_CALLOC(nmemb, size) SDL_calloc((nmemb), (size))
#define DTR_REALLOC(ptr, size)  SDL_realloc((ptr), (size))
#define DTR_FREE(ptr)           SDL_free(ptr)

/* ------------------------------------------------------------------------ */
/*  Console — top-level state                                                */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Top-level console state that owns every subsystem
 */
typedef struct dtr_console {
    SDL_Window *  window;
    SDL_Renderer *renderer;
    SDL_Texture * screen_tex;

    dtr_runtime_t *      runtime;
    dtr_graphics_t *     graphics;
    dtr_audio_t *        audio;
    dtr_key_state_t *    keys;
    dtr_mouse_state_t *  mouse;
    dtr_gamepad_state_t *gamepads;
    dtr_input_state_t *  input;
    dtr_event_bus_t *    events;
    dtr_cart_t *         cart;
    dtr_postfx_t *       postfx;

    bool running;
    bool paused;
    bool has_error;
    bool fullscreen;
    bool restart;
    bool reload;  /**< Hot-reload: re-eval JS without tearing down subsystems */
    bool new_frame; /**< Set true at start of each iterate, cleared after resets */

    uint64_t frame_count;
    uint64_t time_prev;
    float    time;
    float    delta;
    int32_t  target_fps;

    /* Window dimensions (framebuffer * scale) */
    int32_t win_width;
    int32_t win_height;

    /* Framebuffer dimensions from cart or compiled default */
    int32_t fb_width;
    int32_t fb_height;

    /* Cart path for restart support */
    char cart_path[512];

#if DEV_BUILD
    /* Hot-reload: JS source file watching */
    char     watch_path[1024]; /**< Resolved path to the JS source file */
    int64_t  watch_mtime;      /**< Last known modify_time (SDL_Time ns) */
    uint64_t watch_last_poll;  /**< SDL_GetPerformanceCounter at last poll */
    float    reload_toast;     /**< Countdown for "RELOADED" toast overlay */
#endif
} dtr_console_t;

/* ------------------------------------------------------------------------ */
/*  Console lifecycle                                                        */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Create and initialise a console instance
 * \param[in]       cart_path: Path to cart.json or cart.baked.json
 * \return          Console pointer on success, NULL on failure
 */
dtr_console_t *dtr_console_create(const char *cart_path);

/**
 * \brief           Process a single SDL event
 * \param[in]       con: Console instance
 * \param[in]       event: SDL event to handle
 */
void dtr_console_event(dtr_console_t *con, SDL_Event *event);

/**
 * \brief           Run one frame (called each iteration by SDL)
 * \param[in]       con: Console instance
 */
void dtr_console_iterate(dtr_console_t *con);

/**
 * \brief           Tear down and free all resources
 * \param[in]       con: Console instance
 */
void dtr_console_destroy(dtr_console_t *con);

/**
 * \brief           Hot-reload JS code without tearing down subsystems.
 *
 * Destroys only the JS runtime and event bus, re-reads the JS source from
 * disk, creates a fresh runtime/context, re-registers APIs, and re-evals
 * the code.  Optionally calls _save() before teardown and _restore(state)
 * after re-eval so the cart can preserve game state across reloads.
 *
 * \param[in]       con: Console instance
 * \return          true on success, false on failure (console left in error state)
 */
bool dtr_console_reload(dtr_console_t *con);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_CONSOLE_H */
