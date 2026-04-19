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

| Preset         | Binary dir           | Description                                                 |
| -------------- | -------------------- | ----------------------------------------------------------- |
| `debug`        | `build/debug`        | Debug build, developer mode on                              |
| `release`      | `build/release`      | Optimised release build                                     |
| `asan`         | `build/asan`         | Debug + AddressSanitizer + UndefinedBehaviorSanitizer       |
| `coverage`     | `build/coverage`     | Debug + code coverage instrumentation (GCC/Clang only)      |
| `pgo-generate` | `build/pgo-generate` | Release build that generates profile data (GCC/Clang only)  |
| `pgo-use`      | `build/pgo-use`      | Release build using collected profile data (GCC/Clang only) |
| `fuzz`         | `build/fuzz`         | Fuzz targets with libFuzzer (Clang only, Linux/macOS)       |
| `wasm`         | `build/wasm`         | Emscripten WASM release (Ninja)                             |
| `wasm-dev`     | `build/wasm-dev`     | Emscripten WASM debug (Ninja)                               |

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

### Sanitizers (ASan + UBSan)

The `asan` preset enables AddressSanitizer and UndefinedBehaviorSanitizer for
catching memory errors and undefined behaviour at runtime:

```bash
cmake --preset asan
cmake --build build/asan
ctest --test-dir build/asan --output-on-failure
```

> **Note:** On Windows with MSVC, the ASan runtime DLL
> (`clang_rt.asan_dynamic-x86_64.dll`) must be on `PATH`.

### Fuzzing

The `fuzz` preset builds fuzz targets with libFuzzer (Clang only, Linux/macOS):

```bash
cmake --preset fuzz
cmake --build build/fuzz
./build/fuzz/tests/fuzz/fuzz_cart_parse corpus/
```

Fuzz targets live in `tests/fuzz/`. Each target is a standalone executable that
accepts a corpus directory.

### Profile-guided optimisation (PGO)

PGO is a two-step process (GCC/Clang only, Linux/macOS):

```bash
# Step 1 — build with instrumentation and run a representative workload
cmake --preset pgo-generate
cmake --build build/pgo-generate
./build/pgo-generate/dithr examples/spritemark/cart.json   # run your workload

# Step 2 — rebuild using the collected profile data
cmake --preset pgo-use
cmake --build build/pgo-use
```

The `pgo-use` preset reads profile data from `build/pgo-generate/` by default
(configurable via `DTR_PGO_PROFILE_DIR`).

### Code coverage

Coverage is collected automatically in CI on Linux using `gcov` + `lcov`. To
run coverage locally on **Windows**, install
[OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage):

```powershell
winget install OpenCppCoverage.OpenCppCoverage
```

After installing, restart your terminal (and VS Code if using the integrated
terminal) so the updated PATH is picked up. Then use the **Coverage (Windows)**
VS Code task, or run manually:

```powershell
# Build first
cmake --preset debug
cmake --build build/debug --config Debug

# Collect coverage and generate HTML report
New-Item -ItemType Directory build/coverage-parts -Force | Out-Null
foreach ($t in Get-ChildItem build/debug/tests/Debug/test_*.exe) {
    OpenCppCoverage --quiet --sources "$PWD\src" `
        --export_type "binary:build/coverage-parts/$($t.BaseName).cov" `
        -- $t.FullName
}
$inputs = Get-ChildItem build/coverage-parts/*.cov |
    ForEach-Object { '--input_coverage'; $_.FullName }
OpenCppCoverage @inputs --export_type html:build/coverage-report
```

The report is generated in `build/coverage-report/`. Open `index.html` to view
it.

On **Linux / macOS**, use the `DTR_ENABLE_COVERAGE` CMake option (requires
`lcov` and `genhtml`):

```bash
cmake -B build/coverage -G Ninja -DCMAKE_BUILD_TYPE=Debug -DDTR_ENABLE_COVERAGE=ON
cmake --build build/coverage
ctest --test-dir build/coverage --output-on-failure
lcov --capture --directory build/coverage --include '*/src/*' \
     --output-file build/coverage.info --ignore-errors mismatch
genhtml build/coverage.info --output-directory build/coverage-report
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

### GitHub Pages

Pushing to `main` triggers the `.github/workflows/pages.yml` workflow, which
builds every example as a standalone WASM binary and deploys them to GitHub
Pages. Each example is available at `/<example-name>/` with a generated index
page linking to all of them.

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

Example — a custom build with a tiny framebuffer:

```bash
cmake --preset debug -DDTR_FB_WIDTH=128 -DDTR_FB_HEIGHT=128 -DDTR_WINDOW_SCALE=4
```

## CMake targets

| Target                  | Type           | Description                                                    |
| ----------------------- | -------------- | -------------------------------------------------------------- |
| `dithr_core`            | Static library | All engine source except `main.c`; SDL linked PUBLIC           |
| `dithr_core_test`       | Static library | Same sources as `dithr_core`; SDL linked PRIVATE (test builds) |
| `dithr`                 | Executable     | Links `dithr_core` + SDL main callbacks                        |
| `test_input`            | Test exe       | Keyboard / virtual input tests                                 |
| `test_cart`             | Test exe       | Cart loading tests                                             |
| `test_cart_import`      | Test exe       | Cart import / file-based loading tests                         |
| `test_graphics`         | Test exe       | Graphics primitives tests (links `dithr_core_test`)            |
| `test_graphics_ext`     | Test exe       | Extended graphics tests                                        |
| `test_event`            | Test exe       | Event bus tests                                                |
| `test_mouse_gamepad`    | Test exe       | Mouse and gamepad state tests                                  |
| `test_touch`            | Test exe       | Touch input tests                                              |
| `test_runtime`          | Test exe       | QuickJS runtime wrapper tests                                  |
| `test_module_normalize` | Test exe       | JS module path normalization tests                             |
| `test_audio`            | Test exe       | Audio subsystem tests                                          |
| `test_postfx`           | Test exe       | Post-processing pipeline tests                                 |
| `test_col`              | Test exe       | Collision detection tests                                      |
| `test_cam`              | Test exe       | Camera and text metric tests (links `dithr_core_test`)         |
| `test_ui`               | Test exe       | UI layout tests (links `dithr_core_test`)                      |
| `test_map`              | Test exe       | Tilemap tests                                                  |
| `test_synth`            | Test exe       | Synthesizer tests                                              |
| `test_tween`            | Test exe       | Tween animation tests (links `dithr_core_test`)                |
| `test_api_bridge`       | Test exe       | JS API bridge tests (QuickJS, no graphics/audio)               |
| `test_api_bridge_gfx`   | Test exe       | JS API bridge tests requiring a graphics context               |
| `test_api_bridge_map`   | Test exe       | JS API bridge map tests                                        |
| `test_console`          | Test exe       | Console lifecycle tests (require SDL display)                  |
| `test_editor_js`        | Test exe       | Editor JS tests (QuickJS + host file paths)                    |

`dithr_core_test` is marked `EXCLUDE_FROM_ALL` — it is only built when a test
target depends on it. Because SDL is linked PRIVATE, test binaries that only
exercise SDL-free code (like `test_graphics`) won't load SDL shared libraries
at runtime.

## Tooling

### SDK CLI (`@dithrkit/sdk`)

| Command           | Usage                         | Description                                    |
| ----------------- | ----------------------------- | ---------------------------------------------- |
| `dithrkit run`    | `npx dithrkit run [cart-dir]` | Run natively with hot-reload                   |
| `dithrkit dev`    | `npx dithrkit dev [cart-dir]` | Build TS in watch mode + run (native dev loop) |
| `dithrkit build`  | `npx dithrkit build`          | Bundle JS/TS source via esbuild                |
| `dithrkit serve`  | `npx dithrkit serve [port]`   | Build and serve in the browser                 |
| `dithrkit watch`  | `npx dithrkit watch [port]`   | Build, serve, and live-reload on file changes  |
| `dithrkit export` | `npx dithrkit export --web`   | Export for web or desktop distribution         |

### Engine development scripts

| Script                          | Usage                                                     | Description                                                         |
| ------------------------------- | --------------------------------------------------------- | ------------------------------------------------------------------- |
| `npm create @dithrkit`          | `npm create @dithrkit <name>`                             | Scaffold a new cart project (JS or TS)                              |
| `tools/cart-export.js`          | `node tools/cart-export.js <cart.json> [--out file.html]` | Export a cart to standalone HTML                                    |
| `tools/serve-wasm.js`           | `node tools/serve-wasm.js [port]`                         | WASM dev server (default port 8080)                                 |
| `tools/watch-wasm.js`           | `node tools/watch-wasm.js <cart-dir>`                     | WASM dev server with file watching and live-reload                  |
| `tools/generate-pages-index.js` | `node tools/generate-pages-index.js <siteDir>`            | Generate index.html linking to all built WASM examples (used by CI) |
