# Architecture

This document describes the high-level design of dithr for anyone reading or
contributing to the engine source code.

## Overview

dithr is a **fantasy console**: a small, opinionated game runtime with
fixed-size framebuffer rendering, a built-in sprite/tilemap pipeline, and a
JavaScript scripting layer powered by QuickJS-NG. The engine is written in C11
and uses SDL3 for windowing, rendering, input, and audio.

```
                     ┌──────────────┐
                     │   cart.json  │  game assets + JS code
                     └──────┬───────┘
                            │
                     ┌──────▼───────┐
                     │   Console    │  top-level owner
                     └──┬──┬──┬──┬─┘
                        │  │  │  │
           ┌────────────┘  │  │  └────────────┐
           ▼               ▼  ▼               ▼
      ┌─────────┐   ┌─────────────┐   ┌───────────┐
      │ Runtime  │   │  Graphics   │   │   Audio   │
      │ (QuickJS)│   │ (Software)  │   │ (SDL_mixer)│
      └─────────┘   └─────────────┘   └───────────┘
           │               │
      ┌────┴────┐    ┌─────┴──────┐
      │ JS API  │    │  PostFX    │
      │ Bindings│    │ (Software) │
      └─────────┘    └────────────┘
```

## Lifecycle — SDL_MAIN_USE_CALLBACKS

The engine uses SDL's callback-driven architecture instead of a traditional
`main()` loop. SDL provides the real entry point; the engine implements four
callbacks:

| Callback         | File     | Engine function         |
| ---------------- | -------- | ----------------------- |
| `SDL_AppInit`    | `main.c` | `dtr_console_create()`  |
| `SDL_AppEvent`   | `main.c` | `dtr_console_event()`   |
| `SDL_AppIterate` | `main.c` | `dtr_console_iterate()` |
| `SDL_AppQuit`    | `main.c` | `dtr_console_destroy()` |

This means:

- There is **no `SDL_Init` / `SDL_Quit`** in engine code — SDL owns the
  process lifecycle.
- There is **no `SDL_Delay`** framerate cap — SDL manages timing.
- **Restart** is handled inside `SDL_AppIterate` by destroying and recreating
  the console.

## Console (`console.h` / `console.c`)

`dtr_console_t` is the top-level struct that owns every subsystem:

```c
typedef struct dtr_console {
    SDL_Window *  window;
    SDL_Renderer *renderer;
    SDL_Texture * screen_tex;

    dtr_runtime_t *      runtime;    // JS engine
    dtr_graphics_t *     graphics;   // software framebuffer
    dtr_audio_t *        audio;      // SDL3_mixer wrapper
    dtr_key_state_t *    keys;       // keyboard
    dtr_mouse_state_t *  mouse;      // mouse
    dtr_gamepad_state_t *gamepads;   // gamepads (up to 4)
    dtr_input_state_t *  input;      // virtual action map
    dtr_event_bus_t *    events;     // JS event bus
    dtr_cart_t *         cart;       // loaded cart data
    dtr_postfx_t *       postfx;    // post-processing

    // timing, flags, dimensions...
} dtr_console_t;
```

### Frame lifecycle

Each frame proceeds as:

1. **Event phase** — `dtr_console_event()` is called once per SDL event.
   Keyboard, mouse, and gamepad state is updated. The renderer's coordinate
   system handles letterbox mapping via `SDL_ConvertEventToRenderCoordinates`.
2. **Iterate phase** — `dtr_console_iterate()`:
    - Copies current input state to previous (for `btnp` detection).
    - Calls the JS `_update(dt)` callback.
    - Calls the JS `_draw()` callback.
    - Applies post-processing (if any effects are active).
    - Uploads the framebuffer to an SDL texture and presents it.
    - Flushes the event bus.

## Runtime (`runtime.h` / `runtime.c`)

Wraps QuickJS-NG with:

- **`dtr_runtime_eval`** — evaluate a JS source string.
- **`dtr_runtime_call`** — call a global JS function by cached `JSAtom`.
- **`dtr_runtime_drain_jobs`** — execute pending microtasks (Promises).
- **Error overlay** — on exception, the error message and line number are
  captured and rendered on-screen. Further JS calls are blocked until
  `dtr_runtime_clear_error` is called.

The console pointer is stored as the QuickJS context opaque, allowing every
API binding to reach the full engine state via
`JS_GetContextOpaque(ctx)`.

## Graphics (`graphics.h` / `graphics.c`)

All rendering is **software**. The graphics subsystem owns:

- A flat `uint8_t` framebuffer (`fb_width * fb_height`).
- A 256-entry palette (each entry is `{ r, g, b, a }`).
- A spritesheet image decoded by SDL3_image into an indexed pixel buffer.
- Per-sprite flag bytes.
- Draw state: current colour, camera offset, clip rectangle, cursor position,
  draw palette (colour remapping), transparency mask, and 4×4 fill pattern.

The `gfx` JS namespace maps directly to `dtr_gfx_*` C functions. The API
resolves a colour argument of `-1` to the current draw colour, so omitting a
colour parameter uses whatever was last set with `gfx.color()`.

## Input

Three layers of input:

1. **Raw state** — `dtr_key_state_t`, `dtr_mouse_state_t`,
   `dtr_gamepad_state_t` track current and previous frame state for `btn`
   (held) and `btnp` (just pressed) queries.
2. **Virtual actions** — `dtr_input_state_t` maps named actions (e.g.
   `"jump"`) to one or more bindings (keyboard key, gamepad button, gamepad
   axis, or mouse button). Bindings are defined in `cart.json` or at runtime
   via `input.map()`.
3. **JS API** — `key.*`, `mouse.*`, `pad.*`, and `input.*` namespaces expose
   all three layers to game code.

## Audio (`audio.h` / `audio.c`)

Wraps SDL3_mixer 3.2.0:

- **Sound effects** (`sfx` namespace) — loaded from cart assets, played on
  numbered channels with volume control.
- **Music** (`mus` namespace) — streamed playback with crossfade support.

## Event Bus (`event.h` / `event.c`)

A lightweight pub/sub system:

- Up to 128 registered handlers and 64 queued events per frame.
- Events are **queued** by `emit()` and **dispatched** during `flush()` at
  frame end — this prevents mutation during iteration.
- `on()` returns a handle for later `off()`. `once()` auto-removes after
  first fire.

## Post-Processing (`postfx.h` / `postfx.c`)

A stack of software post-processing effects applied to the framebuffer after
`_draw()`. Built-in effects: CRT, scanlines, bloom, chromatic aberration. Each
effect has configurable intensity. The stack can be saved/restored.

## Cart System (`cart.h` / `cart.c`)

A cart is a `cart.json` file that describes the game:

- **Metadata** — title, author, version.
- **Display** — framebuffer dimensions, window scale.
- **Sprites** — path to spritesheet PNG, tile dimensions.
- **Audio** — channel count.
- **Input** — default action-to-binding mappings.
- **Maps** — Tiled-exported tilemap data.
- **Code** — path to the main JS file.

See [cart-format.md](cart-format.md) for the full schema.

## Memory Management

All allocations go through `DTR_MALLOC`, `DTR_CALLOC`, `DTR_REALLOC`, and
`DTR_FREE`. These macros are defined in two layers:

- **`console_defs.h`** — maps them to standard C `malloc`/`free`. This header
  is SDL-free and can be included by modules that don't need SDL (e.g.
  `graphics.h`).
- **`console.h`** — includes `console_defs.h`, then overrides the macros to
  use SDL's allocator (`SDL_malloc`, `SDL_free`, etc.). Any code that includes
  `console.h` gets SDL-backed allocation.

This split keeps `graphics.c` completely self-contained — it can be compiled
and tested without linking SDL.

## File Layout

```
src/
├── main.c              SDL_MAIN_USE_CALLBACKS entry point
├── console_defs.h      SDL-free constants, forward types, stdlib memory macros
├── console.c/h         Top-level state, create/event/iterate/destroy
├── runtime.c/h         QuickJS-NG wrapper
├── graphics.c/h        Software framebuffer renderer (includes console_defs.h, not console.h)
├── audio.c/h           SDL3_mixer wrapper
├── input.c/h           Virtual input mapping
├── event.c/h           Event bus
├── mouse.c/h           Mouse state
├── gamepad.c/h         Gamepad state (up to 4)
├── cart.c/h            Cart loading and persistence
├── cart_import.c/h     Asset import (sprites, maps, audio)
├── postfx.c/h          Post-processing pipeline
├── font.h              Built-in bitmap font data
└── api/
    ├── api_common.h    Shared helper macros for API bindings
    ├── api_register.c  Registers all namespaces on the JS global
    ├── gfx_api.c       gfx.* namespace
    ├── map_api.c       map.* namespace
    ├── key_api.c       key.* namespace
    ├── mouse_api.c     mouse.* namespace
    ├── pad_api.c       pad.* namespace
    ├── input_api.c     input.* namespace
    ├── evt_api.c       evt.* namespace
    ├── sfx_api.c       sfx.* namespace
    ├── mus_api.c       mus.* namespace
    ├── postfx_api.c    postfx.* namespace
    ├── math_api.c      math.* namespace
    ├── cart_api.c      cart.* namespace
    └── sys_api.c       sys.* namespace
```
