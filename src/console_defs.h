/**
 * \file            console_defs.h
 * \brief           Compile-time constants and memory macros (no SDL dependency)
 *
 * This header is intentionally free of SDL includes so that modules which
 * only need the CONSOLE_* constants and DTR_MALLOC/FREE macros (e.g.
 * graphics) can be compiled and tested without linking SDL.
 */

#ifndef DTR_CONSOLE_DEFS_H
#define DTR_CONSOLE_DEFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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
#ifndef CONSOLE_FB_MAX_WIDTH
#define CONSOLE_FB_MAX_WIDTH 1920
#endif
#ifndef CONSOLE_FB_MAX_HEIGHT
#define CONSOLE_FB_MAX_HEIGHT 1080
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

/* Draw list */
#ifndef CONSOLE_MAX_DRAW_CMDS
#define CONSOLE_MAX_DRAW_CMDS 1024
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

/* Profiler */
#ifndef CONSOLE_MAX_PERF_MARKERS
#define CONSOLE_MAX_PERF_MARKERS 16
#endif
#define CONSOLE_PERF_LABEL_LEN 32

/* Dev */
#ifndef DEV_BUILD
#define DEV_BUILD 0
#endif

/* Array element count helper */
#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

/* ------------------------------------------------------------------------ */
/*  Memory macros — use stdlib so consumers aren't forced to link SDL        */
/* ------------------------------------------------------------------------ */

#ifndef DTR_MALLOC
#define DTR_MALLOC(size) malloc(size)
#endif
#ifndef DTR_CALLOC
#define DTR_CALLOC(nmemb, size) calloc((nmemb), (size))
#endif
#ifndef DTR_REALLOC
#define DTR_REALLOC(ptr, size) realloc((ptr), (size))
#endif
#ifndef DTR_FREE
#define DTR_FREE(ptr) free(ptr)
#endif

/* ------------------------------------------------------------------------ */
/*  Forward declarations (typedef once — subsystem headers define bodies)    */
/* ------------------------------------------------------------------------ */

typedef struct dtr_graphics      dtr_graphics_t;
typedef struct dtr_audio         dtr_audio_t;
typedef struct dtr_key_state     dtr_key_state_t;
typedef struct dtr_mouse_state   dtr_mouse_state_t;
typedef struct dtr_gamepad_state dtr_gamepad_state_t;
typedef struct dtr_input_state   dtr_input_state_t;
typedef struct dtr_event_bus     dtr_event_bus_t;
typedef struct dtr_cart          dtr_cart_t;
typedef struct dtr_postfx        dtr_postfx_t;
typedef struct dtr_runtime       dtr_runtime_t;
typedef struct dtr_tween         dtr_tween_t;
typedef struct dtr_res           dtr_res_t;

#endif /* DTR_CONSOLE_DEFS_H */
