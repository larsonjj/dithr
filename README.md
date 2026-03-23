# dithr (Game Toolkit)

[![CI](https://github.com/larsonjj/dithr/actions/workflows/ci.yml/badge.svg)](https://github.com/larsonjj/dithr/actions/workflows/ci.yml)

A fantasy console engine for building retro games. Write game logic in
JavaScript, and the engine handles rendering, audio, input, and tilemaps
through a concise built-in API. Runs natively on Windows, macOS and Linux, or
in the browser via WebAssembly.

## Features

- **320 x 180** default framebuffer (configurable down to 128 x 128 for
  PICO-8 style games)
- **256-colour palette** with draw palette, screen palette, and transparency
- **Sprite sheet** rendering with flip, stretch-blit, and per-sprite flags
- **Tilemap** support with multiple levels, layers, and object queries
- **Post-processing** pipeline: CRT, scanlines, bloom, chromatic aberration
- **Sound effects & music** via SDL3_mixer (MP3, OGG, WAV)
- **Keyboard, mouse, and gamepad** input with a virtual action-mapping layer
- **Event bus** for custom game events (on / once / off / emit)
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

See [docs/building.md](docs/building.md) for all presets (release, retro,
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

## Documentation

| Document                               | Description                                            |
| -------------------------------------- | ------------------------------------------------------ |
| [API Reference](docs/api-reference.md) | All 15 JS namespaces with every function and constant  |
| [Cart Format](docs/cart-format.md)     | `cart.json` schema and asset conventions               |
| [Architecture](docs/architecture.md)   | Engine lifecycle, subsystem overview, design decisions |
| [Building](docs/building.md)           | Build presets, CMake variables, WASM workflow          |
| [Code Style](docs/code-style.md)       | C coding conventions for contributors                  |

## Project Structure

```
src/           C engine source and headers
src/api/       JS ↔ C API bindings (one file per namespace)
tests/         Unit tests (CTest)
examples/      Example carts
tools/         Node.js tooling (cart scaffold, export, WASM dev server)
docs/          Documentation
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
