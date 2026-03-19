# Building

dithr uses CMake with presets for easy configuration. All library dependencies
(SDL3, SDL3_mixer, SDL3_image, QuickJS-NG) are fetched automatically via
`FetchContent` — no manual dependency management needed.

## Prerequisites

| Tool           | Minimum version                   | Notes                                                           |
| -------------- | --------------------------------- | --------------------------------------------------------------- |
| CMake          | 3.22.1                            |                                                                 |
| C compiler     | MSVC 2019+, GCC 10+, or Clang 13+ | C11 support required                                            |
| Git            | any                               | Used by FetchContent to clone dependencies                      |
| Ninja          | 1.10+                             | Required for WASM presets; recommended for all builds           |
| Node.js        | 18+                               | Optional — only needed for cart tooling and the WASM dev server |
| Emscripten SDK | 3.x                               | Optional — only needed for WASM builds                          |

## Build presets

| Preset     | Binary dir       | Description                                 |
| ---------- | ---------------- | ------------------------------------------- |
| `debug`    | `build/debug`    | Debug build, developer mode on              |
| `release`  | `build/release`  | Optimised release build                     |
| `retro`    | `build/retro`    | 128×128 framebuffer, scale 4 (PICO-8 style) |
| `wasm`     | `build/wasm`     | Emscripten WASM release (Ninja)             |
| `wasm-dev` | `build/wasm-dev` | Emscripten WASM debug (Ninja)               |

## Native build

### Configure and build

```bash
cmake --preset debug
cmake --build build/debug --config Debug
```

For a release build:

```bash
cmake --preset release
cmake --build build/release --config Release
```

### Run

```bash
./build/debug/Debug/dithr path/to/cart.json
```

### Run tests

```bash
ctest --test-dir build/debug -C Debug --output-on-failure
```

## WASM build

### One-time setup

1. Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/).
2. Activate the SDK so the `EMSDK` environment variable is set:
    ```bash
    source /path/to/emsdk/emsdk_env.sh    # Linux / macOS
    emsdk_env.bat                           # Windows
    ```
3. Install Ninja (the WASM presets require it).

### Configure and build

```bash
cmake --preset wasm
cmake --build build/wasm --config Release
```

By default the WASM build bundles the `examples/hello_world` cart. To change
this, set `DTR_WASM_CART_DIR` to the path of your cart:

```bash
cmake --preset wasm -DDTR_WASM_CART_DIR=/path/to/my-game
```

### Run locally

A zero-dependency Node.js dev server is included:

```bash
node tools/serve-wasm.js
```

This serves `build/wasm/` on `http://localhost:8080` with the required
COOP/COEP headers and gzip/Brotli content negotiation.

## CMake cache variables

All console defaults can be overridden at configure time:

| Variable              | Default                | Description                   |
| --------------------- | ---------------------- | ----------------------------- |
| `DTR_FB_WIDTH`        | 320                    | Framebuffer width             |
| `DTR_FB_HEIGHT`       | 180                    | Framebuffer height            |
| `DTR_PALETTE_SIZE`    | 256                    | Palette size                  |
| `DTR_WINDOW_SCALE`    | 3                      | Default window scale          |
| `DTR_MAX_SPRITES`     | 1024                   | Max sprite tiles              |
| `DTR_SPRITE_FLAGS`    | 8                      | Flag bits per sprite          |
| `DTR_MAX_MAPS`        | 32                     | Max map slots                 |
| `DTR_MAX_MAP_LAYERS`  | 8                      | Max layers per map            |
| `DTR_MAX_MAP_OBJECTS` | 512                    | Max objects per map           |
| `DTR_MAX_CHANNELS`    | 16                     | Audio mixer channels          |
| `DTR_AUDIO_FREQ`      | 44100                  | Audio sample rate (Hz)        |
| `DTR_AUDIO_BUFFER`    | 2048                   | Audio buffer size             |
| `DTR_TARGET_FPS`      | 60                     | Target framerate              |
| `DTR_JS_HEAP_MB`      | 64                     | QuickJS heap limit (MB)       |
| `DTR_JS_STACK_KB`     | 512                    | QuickJS stack limit (KB)      |
| `DTR_VERSION_STR`     | `"0.1.0"`              | Engine version string         |
| `DTR_DEV_BUILD`       | OFF                    | Enable developer features     |
| `DTR_BUILD_EXAMPLES`  | ON                     | Build example projects        |
| `DTR_BUILD_TESTS`     | ON                     | Build test suite              |
| `DTR_WASM_CART_DIR`   | `examples/hello_world` | Cart to bundle in WASM builds |

Example — a PICO-8 style build with a tiny framebuffer:

```bash
cmake --preset debug -DDTR_FB_WIDTH=128 -DDTR_FB_HEIGHT=128 -DDTR_WINDOW_SCALE=4
```

## CMake targets

| Target               | Type           | Description                             |
| -------------------- | -------------- | --------------------------------------- |
| `dithr_core`         | Static library | All engine source except `main.c`       |
| `dithr`              | Executable     | Links `dithr_core` + SDL main callbacks |
| `test_input`         | Test exe       | Keyboard / virtual input tests          |
| `test_cart`          | Test exe       | Cart loading tests                      |
| `test_graphics`      | Test exe       | Graphics primitives tests               |
| `test_event`         | Test exe       | Event bus tests                         |
| `test_mouse_gamepad` | Test exe       | Mouse and gamepad state tests           |
| `test_runtime`       | Test exe       | QuickJS runtime wrapper tests           |

## Tooling

| Script                 | Usage                                                     | Description                                                                  |
| ---------------------- | --------------------------------------------------------- | ---------------------------------------------------------------------------- |
| `tools/create-cart.js` | `node tools/create-cart.js <name> [--template <t>]`       | Scaffold a new cart project (templates: `blank`, `platformer`, `pico8-port`) |
| `tools/cart-export.js` | `node tools/cart-export.js <cart.json> [--out file.html]` | Export a cart to standalone HTML                                             |
| `tools/serve-wasm.js`  | `node tools/serve-wasm.js [port]`                         | WASM dev server (default port 8080)                                          |
