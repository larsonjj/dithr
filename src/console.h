/**
 * \file            console.h
 * \brief           MVN Console — core state, compile-time defaults, memory macros
 */

#ifndef MVN_CONSOLE_H
#define MVN_CONSOLE_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ------------------------------------------------------------------------ */
/*  Compile-time defaults — every value overridable via CMake cache          */
/* ------------------------------------------------------------------------ */

/* Display */
#ifndef CONSOLE_FB_WIDTH
#define CONSOLE_FB_WIDTH 320
#endif
#ifndef CONSOLE_FB_HEIGHT
#define CONSOLE_FB_HEIGHT 180
#endif
#ifndef CONSOLE_PALETTE_SIZE
#define CONSOLE_PALETTE_SIZE 256
#endif
#ifndef CONSOLE_WINDOW_SCALE
#define CONSOLE_WINDOW_SCALE 3
#endif

/* Sprites */
#ifndef CONSOLE_MAX_SPRITES
#define CONSOLE_MAX_SPRITES 256
#endif
#ifndef CONSOLE_SPRITE_FLAGS
#define CONSOLE_SPRITE_FLAGS 8
#endif

/* Maps */
#ifndef CONSOLE_MAX_MAPS
#define CONSOLE_MAX_MAPS 32
#endif
#ifndef CONSOLE_MAX_MAP_LAYERS
#define CONSOLE_MAX_MAP_LAYERS 8
#endif
#ifndef CONSOLE_MAX_MAP_OBJECTS
#define CONSOLE_MAX_MAP_OBJECTS 512
#endif

/* Audio */
#ifndef CONSOLE_MAX_CHANNELS
#define CONSOLE_MAX_CHANNELS 16
#endif
#ifndef CONSOLE_AUDIO_FREQ
#define CONSOLE_AUDIO_FREQ 44100
#endif
#ifndef CONSOLE_AUDIO_BUFFER
#define CONSOLE_AUDIO_BUFFER 2048
#endif

/* Runtime */
#ifndef CONSOLE_TARGET_FPS
#define CONSOLE_TARGET_FPS 60
#endif
#ifndef CONSOLE_JS_HEAP_MB
#define CONSOLE_JS_HEAP_MB 64
#endif
#ifndef CONSOLE_JS_STACK_KB
#define CONSOLE_JS_STACK_KB 512
#endif
#ifndef CONSOLE_VERSION
#define CONSOLE_VERSION "0.1.0"
#endif

/* Convenience aliases used by subsystem code */
#ifndef CONSOLE_DEFAULT_SCALE
#define CONSOLE_DEFAULT_SCALE CONSOLE_WINDOW_SCALE
#endif
#ifndef CONSOLE_FPS
#define CONSOLE_FPS CONSOLE_TARGET_FPS
#endif
#ifndef CONSOLE_TILE_W
#define CONSOLE_TILE_W 8
#endif
#ifndef CONSOLE_TILE_H
#define CONSOLE_TILE_H 8
#endif
#ifndef CONSOLE_AUDIO_CHANNELS
#define CONSOLE_AUDIO_CHANNELS CONSOLE_MAX_CHANNELS
#endif
#ifndef CONSOLE_MAP_SLOTS
#define CONSOLE_MAP_SLOTS CONSOLE_MAX_MAPS
#endif
#ifndef CONSOLE_JS_MEM_MB
#define CONSOLE_JS_MEM_MB CONSOLE_JS_HEAP_MB
#endif

/* Dev */
#ifndef DEV_BUILD
#define DEV_BUILD 0
#endif

/* Array element count helper */
#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

/* ------------------------------------------------------------------------ */
/*  Memory macros — thin wrappers around SDL allocation                      */
/* ------------------------------------------------------------------------ */

#define MVN_MALLOC(size)        SDL_malloc(size)
#define MVN_CALLOC(nmemb, size) SDL_calloc((nmemb), (size))
#define MVN_REALLOC(ptr, size)  SDL_realloc((ptr), (size))
#define MVN_FREE(ptr)           SDL_free(ptr)

/* ------------------------------------------------------------------------ */
/*  Forward declarations                                                     */
/* ------------------------------------------------------------------------ */

typedef struct mvn_graphics      mvn_graphics_t;
typedef struct mvn_audio         mvn_audio_t;
typedef struct mvn_key_state     mvn_key_state_t;
typedef struct mvn_mouse_state   mvn_mouse_state_t;
typedef struct mvn_gamepad_state mvn_gamepad_state_t;
typedef struct mvn_input_state   mvn_input_state_t;
typedef struct mvn_event_bus     mvn_event_bus_t;
typedef struct mvn_cart          mvn_cart_t;
typedef struct mvn_postfx        mvn_postfx_t;
typedef struct mvn_runtime       mvn_runtime_t;

/* ------------------------------------------------------------------------ */
/*  Console — top-level state                                                */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Top-level console state that owns every subsystem
 */
typedef struct mvn_console {
    SDL_Window *  window;
    SDL_Renderer *renderer;
    SDL_Texture * screen_tex;

    mvn_runtime_t *      runtime;
    mvn_graphics_t *     graphics;
    mvn_audio_t *        audio;
    mvn_key_state_t *    keys;
    mvn_mouse_state_t *  mouse;
    mvn_gamepad_state_t *gamepads;
    mvn_input_state_t *  input;
    mvn_event_bus_t *    events;
    mvn_cart_t *         cart;
    mvn_postfx_t *       postfx;

    bool running;
    bool paused;
    bool has_error;
    bool fullscreen;
    bool restart;
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
} mvn_console_t;

/* ------------------------------------------------------------------------ */
/*  Console lifecycle                                                        */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Create and initialise a console instance
 * \param[in]       cart_path: Path to cart.json or cart.baked.json
 * \return          Console pointer on success, NULL on failure
 */
mvn_console_t *mvn_console_create(const char *cart_path);

/**
 * \brief           Process a single SDL event
 * \param[in]       con: Console instance
 * \param[in]       event: SDL event to handle
 */
void mvn_console_event(mvn_console_t *con, SDL_Event *event);

/**
 * \brief           Run one frame (called each iteration by SDL)
 * \param[in]       con: Console instance
 */
void mvn_console_iterate(mvn_console_t *con);

/**
 * \brief           Tear down and free all resources
 * \param[in]       con: Console instance
 */
void mvn_console_destroy(mvn_console_t *con);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MVN_CONSOLE_H */
