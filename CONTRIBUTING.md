# Contributing to dithr

Thank you for your interest in contributing! This guide covers the workflow and
conventions used in the project.

## Getting Started

1. **Fork and clone** the repository.
2. Install the [prerequisites](docs/building.md) — CMake >= 3.22.1, a C11
   compiler (MSVC, GCC, or Clang), and Git.
3. Configure and build:
    ```bash
    cmake --preset debug
    cmake --build build/debug --config Debug
    ```
4. Run the tests:
    ```bash
    cd build/debug
    ctest --output-on-failure -C Debug
    ```

## Code Style

Follow the rules in [docs/code-style.md](docs/code-style.md). The key points:

- C11 with `stdint.h` types (`int32_t`, `uint8_t`, …)
- 4-space indentation, 100-character line limit
- `dtr_` prefix for public functions, `prv_` prefix for file-static helpers
- `/* */` block comments only — no `//` comments
- Declare variables at the start of each block

## Adding a New API Namespace

The engine exposes JS namespaces (e.g. `gfx`, `sfx`, `col`). To add one:

1. Create `src/api/my_api.c` following the pattern in existing files:
    - Include `"api_common.h"`
    - Write `static JSValue` wrapper functions
    - Define a `JSCFunctionListEntry` array
    - Implement `dtr_my_api_register(JSContext *ctx, JSValue global)`
2. Add the declaration to `src/api/api_common.h`:
    ```c
    void dtr_my_api_register(JSContext *ctx, JSValue global);
    ```
3. Call it from `src/api/api_register.c`:
    ```c
    dtr_my_api_register(ctx, global);
    ```
4. Add the source file to `CMakeLists.txt` under the `dithr_core` sources.
5. Document the namespace in `docs/api-reference.md`.

## Testing

- Test files live in `tests/` and are named `test_<module>.c`.
- Each test uses the lightweight harness in `tests/test_harness.h`.
- Build and run all tests:
    ```bash
    cmake --build build/debug --config Debug
    cd build/debug
    ctest --output-on-failure -C Debug
    ```
- Run a single suite: `ctest -R test_graphics`
- On Linux/macOS you can use the `asan` preset for AddressSanitizer + UBSan.

## Pull Request Guidelines

- Keep changes focused — one feature or fix per PR.
- Make sure all tests pass before submitting.
- Follow the existing code style (see above).
- Update documentation if you add or change API surface.
