# hello_world

Minimal example cart for [mvngin](../../README.md) — clears the screen and
draws centred text.

## Files

```
cart.json         Cart manifest
src/main.js       Game code — _init, _update, _draw
assets/sprites/   Sprite sheet (blank placeholder)
```

## Run (native)

```bash
# from the mvngin repo root
cmake --preset debug
cmake --build build/debug --config Debug
./build/debug/Debug/mvngin examples/hello_world/cart.json
```

## Run (WASM)

```bash
cmake --preset wasm -DMVN_WASM_CART_DIR=examples/hello_world
cmake --build build/wasm --config Release
node tools/serve-wasm.js
```

## What it demonstrates

- The three lifecycle callbacks (`_init`, `_update`, `_draw`)
- `gfx.cls()` to clear each frame
- `gfx.print()` to draw text with a palette colour
- Default input mappings in `cart.json`
