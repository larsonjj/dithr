# Fuzz harnesses (libFuzzer, Clang only)
#
# Build:
#   cmake --preset fuzz
#   cmake --build build/fuzz
#
# Run:
#   ./build/fuzz/tests/fuzz/fuzz_cart_import corpus/ -max_len=65536
#   ./build/fuzz/tests/fuzz/fuzz_runtime    corpus/ -max_len=4096

# Require Clang's libFuzzer
if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(WARNING "Fuzz targets require Clang; skipping")
    return()
endif()

set(FUZZ_FLAGS -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)

function(dtr_add_fuzz_target name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE dithr_core)
    target_compile_options(${name} PRIVATE ${FUZZ_FLAGS})
    target_link_options(${name} PRIVATE ${FUZZ_FLAGS})
endfunction()

dtr_add_fuzz_target(fuzz_cart_import fuzz_cart_import.c)
dtr_add_fuzz_target(fuzz_runtime    fuzz_runtime.c)
