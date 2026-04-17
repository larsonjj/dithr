# dithr (Game Toolkit)

| Platform | Compiler / Toolchain    | Build                                                                                                                                                                                | Notes                               |
| -------- | ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------- |
| Windows  | MSVC (latest)           | [![windows](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=build)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)        | Native (multi-config)               |
| Linux    | GCC (Debug)             | [![linux-gcc-dbg](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=build)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)  | Ninja, shared libs off              |
| Linux    | GCC (Release)           | [![linux-gcc-rel](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=build)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)  | Ninja                               |
| Linux    | GCC (ASan/UBSan)        | [![linux-gcc-asan](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=build)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml) | Address + UB sanitizers             |
| Linux    | Clang (Debug)           | [![linux-clang](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=build)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)    | Ninja, shared libs off              |
| Linux    | GCC + lcov              | [![coverage](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=coverage)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)    | Line coverage ≥ 55 %                |
| macOS    | AppleClang (Debug)      | [![macos-dbg](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=build)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)      | Ninja, shared libs off              |
| macOS    | AppleClang (ASan/UBSan) | [![macos-asan](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=build)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)     | Address + UB sanitizers             |
| WASM     | Emscripten (latest)     | [![wasm](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=build)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)           | + Playwright browser smoke test     |
| —        | clang-format-20         | [![fmt](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=format)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)           | C/H formatting check                |
| —        | clang-tidy              | [![tidy](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=tidy)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)            | Static analysis, warnings as errors |
| —        | ESLint + Prettier       | [![jslint](https://img.shields.io/github/actions/workflow/status/larsonjj/dithr/ci.yml?branch=main&label=lint)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)          | Example cart JS linting             |

A fantasy console engine for building retro games. Write game logic in
JavaScript, and the engine handles rendering, audio, input, and tilemaps
through a concise built-in API. Runs natively on Windows, macOS and Linux, or
in the browser via WebAssembly.

## Features

- **320 x 180** default framebuffer (configurable at build time)
- **256-colour palette** with draw palette, screen palette, and transparency
- **Sprite sheet** rendering with flip, stretch-blit, and per-sprite flags
- **Tilemap** support with multiple levels, layers, and object queries
- **Post-processing** pipeline: CRT, scanlines, bloom, chromatic aberration
- **Sound effects & music** via SDL3_mixer (MP3, OGG, WAV)
- **Keyboard, mouse, and gamepad** input with a virtual action-mapping layer
- **Collision helpers** — rect, circle, point-in-shape, circle-vs-rect tests
- **Camera helpers** — set, get, reset, lerp follow
- **Event bus** for custom game events (on / once / off / emit)
- **Hot reload** — automatic JS reload on save (F5 manual trigger), with
  `_save()` / `_restore()` callbacks for state preservation
- **QuickJS-NG** JavaScript runtime with async support
- **Cart system** — package a game as a single `cart.json` plus assets
- **WASM build** for one-click browser deployment

## Quick Start

### Prerequisites

| Tool       | Version             | Notes                                       |
| ---------- | ------------------- | ------------------------------------------- |
| CMake      | >= 3.22.1           |                                             |
| C compiler | MSVC, GCC, or Clang | C11                                         |
| Git        | any                 | For FetchContent                            |
| Node.js    | >= 18               | Optional — cart tooling and WASM dev server |
| Emscripten | >= 3.x              | Optional — WASM builds only                 |

All library dependencies (SDL3, SDL3_mixer, SDL3_image, QuickJS-NG) are
fetched automatically by CMake.

### Build (native)

```bash
cmake --preset debug
cmake --build build/debug --config Debug
```

### Run

```bash
./build/debug/Debug/dithr examples/hello_world/cart.json
```

See [docs/building.md](docs/building.md) for all presets (release,
WASM) and available CMake cache variables.

## Creating a Cart

Use the scaffolding tool to create a new project:

```bash
node tools/create-cart.js my-game --template blank
cd my-game
```

Edit `src/main.js`:

```js
function _init() {
    gfx.cls(0);
}

function _update(dt) {
    // game logic
}

function _draw() {
    gfx.cls(0);
    gfx.print("hello!", 140, 87, 7);
}
```

Run it:

```bash
dithr cart.json
```

### Command-Line Flags

| Flag           | Short | Description                |
| -------------- | ----- | -------------------------- |
| `--fullscreen` | `-f`  | Start in fullscreen mode   |
| `--scale N`    | `-s`  | Window scale factor (1–10) |
| `--mute`       | `-m`  | Start with audio muted     |

## Documentation

| Document                               | Description                                              |
| -------------------------------------- | -------------------------------------------------------- |
| [API Reference](docs/api-reference.md) | All 19 JS namespaces with every function and constant    |
| [Cart Format](docs/cart-format.md)     | `cart.json` schema and asset conventions                 |
| [Architecture](docs/architecture.md)   | Engine lifecycle, subsystem overview, design decisions   |
| [Building](docs/building.md)           | Build presets, CMake variables, WASM workflow            |
| [Code Style](docs/code-style.md)       | C coding conventions for contributors                    |
| [JS Code Style](docs/js-code-style.md) | JavaScript style for cart source files (ESLint/Prettier) |

## Project Structure

```
src/           C engine source and headers
src/api/       JS ↔ C API bindings (one file per namespace)
tests/         Unit tests (CTest)
examples/      Example carts
tools/         Node.js tooling (cart scaffold, export, WASM dev server)
docs/          Documentation
```

## Examples

The `examples/` directory contains playable demo carts:

| Example            | Description                                              |
| ------------------ | -------------------------------------------------------- |
| `hello_world`      | Minimal starter — prints text on screen                  |
| `sprite_animation` | Animated sprite with frame cycling                       |
| `tilemap`          | Multi-layer tilemap with camera scrolling                |
| `audio_demo`       | Sound effects and music playback                         |
| `input_demo`       | Keyboard, mouse, gamepad, and virtual action mapping     |
| `platformer`       | Side-scrolling platformer with tile collision            |
| `raycaster`        | Wolfenstein-style first-person raycaster                 |
| `spritemark`       | Bunnymark-style sprite benchmark (SOA data layout)       |
| `postfx_demo`      | Post-processing effects showcase                         |
| `persistence`      | Save/load game state with `cart.save()` / `cart.load()`  |
| `ui_demo`          | Immediate-mode UI widgets (buttons, sliders, text input) |
| `gauntlet_clone`   | Gauntlet-inspired dungeon crawler with procedural levels |
| `map_demo`         | Tilemap API with camera, layers, levels & object queries |
| `event_demo`       | Event bus (on/once/off/emit) and system events           |
| `transition_demo`  | Screen transitions — fade, wipe, dissolve                |
| `drawlist_demo`    | Depth-sorted rendering with draw list commands           |
| `palette_fx`       | Palette remapping, fill patterns & custom fonts          |
| `collision_demo`   | Collision helpers — rect, circle, point & mixed tests    |
| `sys_explorer`     | System introspection, file I/O & clipboard               |
| `tween_showcase`   | Easing curves, tween pool, math utils & input remapping  |
| `editor`           | Built-in sprite, map, music, and SFX editors             |

Run any example after building:

```bash
./build/debug/dithr examples/platformer/cart.json
```

## Testing

Build and run the test suite with CTest:

```bash
cmake --preset debug
cmake --build build/debug --config Debug
cd build/debug
ctest --output-on-failure -C Debug
```

Run a single test by name:

```bash
ctest --output-on-failure -C Debug -R test_graphics
```

For a sanitized build (Linux/macOS — AddressSanitizer + UBSan):

```bash
cmake --preset asan
cmake --build build/asan
cd build/asan
ctest --output-on-failure
```

## License

This project is licensed under the terms of the [MIT License](LICENSE).
