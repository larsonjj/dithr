# Performance Guide

This guide covers frame budgets, rendering costs, and optimization
strategies for dithr carts.

## Frame Budget

At the default 60 FPS target the engine has **16.67 ms** per frame split
across four phases:

| Phase        | What runs                                         |
| ------------ | ------------------------------------------------- |
| Fixed Update | `_fixedUpdate(dt)` — physics, deterministic logic |
| Update       | `_update(dt)` — game logic, AI                    |
| Draw         | `_draw()` — all rendering calls                   |
| Flip         | Palette lookup + texture upload                   |

Use `sys.perf()` to inspect per-frame timings:

```js
const p = sys.perf();
// p.fixed_update_ms — time in _fixedUpdate (sum of all ticks)
// p.update_ms       — time in _update
// p.draw_ms         — time in _draw
// p.cpu             — 0.0–1.0 total budget usage
// p.markers         — custom perfBegin/perfEnd sections
```

## Fixed vs Variable Update

The engine provides two update callbacks with different timing
characteristics:

| Callback           | Delta                   | When to use                           |
| ------------------ | ----------------------- | ------------------------------------- |
| `_fixedUpdate(dt)` | Constant (1 / `ups`)    | Physics, collision, deterministic sim |
| `_update(dt)`      | Variable (real elapsed) | Input handling, camera, animations    |

`_fixedUpdate` fires 0–N times per frame at a fixed rate set by
`timing.ups` in `cart.json` (default 60). The accumulator is capped at
8 ticks to prevent a spiral of death after long pauses.

```js
// cart.json: { "timing": { "fps": 60, "ups": 50 } }

function _fixedUpdate(dt) {
    // dt is always 1/50 = 0.02 seconds
    vy += GRAVITY * dt;
    py += vy * dt;
    resolveCollisions();
}

function _update(dt) {
    // dt varies with actual frame time
    if (key.btnp("jump")) vy = JUMP_FORCE;
    cam.pos(px, py);
}
```

## Custom Profiling

Mark hot sections with `sys.perfBegin` / `sys.perfEnd` (up to 16
markers per frame):

```js
sys.perfBegin("physics");
stepPhysics();
sys.perfEnd("physics");

sys.perfBegin("ai");
tickEnemies();
sys.perfEnd("ai");
```

Results appear in `sys.perf().markers` as `{ physics: 0.42, ai: 0.15 }`.

## Rendering Costs

### Sprite drawing — `gfx.spr`

| Scenario                      | Approximate cost              |
| ----------------------------- | ----------------------------- |
| 8×8 non-flipped, opaque       | ~fastest (SIMD)               |
| 8×8 non-flipped, transparency | fast (SIMD + branch per 4 px) |
| 8×8 flipped or rotated        | scalar per-pixel              |
| Large `sspr` region           | proportional to pixel count   |

**Tips:**

- Prefer non-flipped sprites when possible — the engine uses a 4-wide
  SIMD fast path for them.
- Batch sprites that share the same spritesheet tile dimensions.
- Off-screen sprites are clipped per-row, not culled entirely.
  Pre-cull large sprite counts yourself.

### Tilemap rendering — `map.draw`

The map renderer blits visible tiles row-by-row. Non-patterned tiles
(no `gfx.fillp`) use the same SIMD fast path as sprites.

| Factor              | Impact                         |
| ------------------- | ------------------------------ |
| Visible tile count  | Linear cost                    |
| Layer count         | Each layer is a full pass      |
| Fill pattern active | Falls back to scalar per-pixel |

**Tips:**

- Limit the number of visible map layers. Combine decorative layers
  where possible.
- Avoid `gfx.fillp` unless needed — it disables the SIMD tile path.

### Primitives

| Primitive           | Notes                        |
| ------------------- | ---------------------------- |
| `rectfill` / `cls`  | `memset`-based, very fast    |
| `line`              | Bresenham, scalar            |
| `circ` / `circfill` | Midpoint algorithm, scalar   |
| `print`             | 1 px per glyph pixel, scalar |

### Post-processing — `postfx`

Effects run on the full framebuffer after `_draw()`, touching every
pixel.

| Effect     | Cost                                  |
| ---------- | ------------------------------------- |
| CRT        | 2 passes (vignette + shift per pixel) |
| SCANLINES  | 1 pass (skip every other row)         |
| BLOOM      | 1 pass (threshold + brighten)         |
| ABERRATION | 1 pass (offset R and B channels)      |

Each stacked effect adds a full-screen pass. Limit the stack to 2–3
effects in practice.

## SIMD

The engine auto-selects the best SIMD backend at compile time:

| Platform     | Backend      |
| ------------ | ------------ |
| x86/x64      | SSE2         |
| ARM/Apple Si | NEON         |
| WASM         | wasm-simd128 |
| Fallback     | Scalar (C)   |

SIMD accelerates:

- `gfx.spr` (non-flipped) — 4-wide palette index processing
- `map.draw` (non-patterned tiles) — same 4-wide path
- Palette flip (`dtr_gfx_flip_to`) — gather-style LUT lookup

No cart-side code changes are needed to benefit from SIMD.

## Build Optimizations

### LTO (Link-Time Optimization)

Enabled by default in the `release` preset. Allows the compiler to
inline across translation units.

```bash
cmake --preset release
cmake --build build/release
```

### Native architecture tuning

For local benchmarking on a known target machine, opt in to
`-march=native` so the compiler can use the full host instruction set
(AVX2, AVX-512, etc.). Off by default — release artifacts ship with a
portable baseline.

```bash
cmake --preset release -DDTR_NATIVE_ARCH=ON
cmake --build build/release
```

This option is a no-op on MSVC and Emscripten.

### PGO (Profile-Guided Optimization)

Two-step build that profiles real workloads:

```bash
# Step 1: generate profiling data
cmake --preset pgo-generate
cmake --build --preset pgo-generate
./build/pgo-generate/dithr path/to/cart   # run representative workload

# Step 2: rebuild using the profile
cmake --preset pgo-use
cmake --build --preset pgo-use
```

## WASM-Specific Notes

- The WASM build uses `-Oz -flto` for minimal binary size.
- Initial memory is 64 MB with `ALLOW_MEMORY_GROWTH`, capped at **512 MB**
  via `-sMAXIMUM_MEMORY` to prevent runaway allocations from crashing the
  browser tab.
- SIMD is available via `-msimd128` when `DTR_ENABLE_SIMD=ON` (default).
- Minimize the number of JS→C API calls per frame — each crossing has
  overhead in Emscripten.

## Engine Hotspots (for contributors)

If you are profiling the engine itself rather than a cart, the
dominant per-frame costs are usually:

| Hotspot                          | Source                             |
| -------------------------------- | ---------------------------------- |
| Sprite blit (`prv_blit_span`)    | `src/graphics.c` — 8-wide unrolled |
| Palette flip (`dtr_gfx_flip_to`) | `src/graphics.c` — SIMD LUT gather |
| Tilemap iteration                | `src/graphics.c` `dtr_gfx_tilemap` |
| Active screen transitions        | `src/graphics.c` transition update |
| Stacked postfx effects           | `src/postfx.c` per-pixel passes    |

`dtr_gfx_transition_update_buf` and `dtr_postfx_apply` early-out when no
transition is active and the postfx stack is empty respectively, so
adding either feature only costs cycles when actually used.

For per-function CPU time, the simplest cross-platform option is the
`spritemark` example, which prints a steady-state FPS readout in the
top-left corner. Use it to compare engine changes:

```bash
cmake --preset release -DDTR_NATIVE_ARCH=ON
cmake --build build/release
build/release/Release/dithr examples/spritemark/cart.json
```

Press SPACE (or click) to spawn 200 more sprites per press; the FPS
number in the top-left is the benchmark.

### Micro-benchmarks (`tests/perf/`)

For tracking individual primitives across builds, an opt-in benchmark
suite lives in `tests/perf/`. It is excluded from the default build
and CTest run; enable it explicitly:

```bash
cmake --preset release -DDTR_BUILD_PERF=ON
cmake --build build/release --config Release
ctest --test-dir build/release -L perf -C Release --output-on-failure
```

Each benchmark prints `name iters=N ns/iter=X.X` lines so results can
be diffed across commits. Add new files to `tests/perf/CMakeLists.txt`
via `dtr_add_perf_test(name source.c)` — they are auto-tagged with the
`perf` CTest label and stay out of the coverage gate.

## General Tips

1. **Measure first.** Use `sys.perf()` and `sys.perfBegin/End` before
   optimising blindly.
2. **Reduce draw calls.** Fewer `gfx.spr` / `gfx.print` calls matter
   more than pixel count.
3. **Limit active map layers.** Each visible layer is a full tile pass.
4. **Avoid per-pixel JS loops.** Operations like flood fill or
   per-pixel colour manipulation should use `gfx.poke` / `gfx.peek`
   sparingly — the C-side rendering is orders of magnitude faster.
5. **Pool objects.** Reuse JS objects and arrays to reduce GC pressure
   in QuickJS.
6. **Cap target FPS.** If your game looks fine at 30 FPS, call
   `sys.setTargetFps(30)` to halve CPU usage.
