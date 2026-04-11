# Testing

This guide covers how to run tests, write new tests, and work with code
coverage in dithr.

## Running Tests Locally

### Quick Start

```bash
cmake --preset debug
cmake --build build/debug --config Debug
cd build/debug
ctest --output-on-failure -C Debug
```

### Preset Summary

| Preset     | Purpose                        | Command                   |
| ---------- | ------------------------------ | ------------------------- |
| `debug`    | Standard debug build           | `cmake --preset debug`    |
| `asan`     | Address + undefined sanitizers | `cmake --preset asan`     |
| `coverage` | Code coverage instrumentation  | See [Coverage](#coverage) |

For sanitizer builds, set environment variables so SDL does not try to open
real audio/video devices:

```bash
export SDL_AUDIO_DRIVER=dummy SDL_VIDEO_DRIVER=dummy
```

On Linux with ASan, you may need the LSAN suppressions file to silence
known third-party leaks:

```bash
LSAN_OPTIONS=suppressions=tests/lsan_suppressions.txt ctest --output-on-failure
```

## Writing a New Test

1. Create `tests/test_<module>.c`.
2. Include the test harness and the header you are testing:

    ```c
    #include "test_harness.h"
    #include "<module>.h"
    ```

3. Write test functions — each one should call `DTR_PASS()` at the end:

    ```c
    static void test_something(void)
    {
        DTR_ASSERT_EQ_INT(my_func(1, 2), 3);
        DTR_PASS();
    }
    ```

4. Use the runner macros in `main()`:

    ```c
    int main(void)
    {
        DTR_TEST_BEGIN("my_module");
        DTR_RUN_TEST(test_something);
        DTR_TEST_END();
    }
    ```

5. Register the test in `tests/CMakeLists.txt`:

    ```cmake
    dtr_add_test(test_mymodule test_mymodule.c)
    ```

    If your test only needs SDL-free code paths, link against the test
    library instead:

    ```cmake
    dtr_add_test(test_mymodule test_mymodule.c LINK_LIB dithr_core_test)
    ```

### Available Assertion Macros

| Macro                        | Description                       |
| ---------------------------- | --------------------------------- |
| `DTR_ASSERT(expr)`           | Abort if `expr` is false          |
| `DTR_ASSERT_NOT_NULL(ptr)`   | Abort if `ptr` is `NULL`          |
| `DTR_ASSERT_EQ_INT(a, b)`    | Abort if two integers differ      |
| `DTR_ASSERT_EQ_STR(a, b)`    | Abort if two strings differ       |
| `DTR_ASSERT_NEAR(a, b, eps)` | Abort if floats differ by > `eps` |

Tests abort on the first failure. If every assertion passes, `DTR_TEST_END()`
prints a summary and returns 0.

## Coverage

CI enforces a minimum line-coverage threshold (currently 55%). To collect
coverage locally:

```bash
cmake -B build/coverage -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDTR_ENABLE_COVERAGE=ON \
  -DBUILD_SHARED_LIBS=OFF

cmake --build build/coverage

cd build/coverage
ctest --output-on-failure

# Collect and view
lcov --capture --directory . --include '*/src/*' \
     --output-file coverage.info --ignore-errors mismatch
genhtml coverage.info --output-directory coverage-report
open coverage-report/index.html
```

## Linting Example Carts

CI also lints the JavaScript examples. After editing any JS files in
`examples/`, verify locally:

```bash
cd examples/<cart_name>
npm ci --ignore-scripts
npm run lint
```
