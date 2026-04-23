# tests/perf/CMakeLists.txt
#
# Optional micro-benchmark suite.  Disabled by default to keep the
# normal build/CTest cycle quick; opt in with:
#
#   cmake --preset release -DDTR_BUILD_PERF=ON
#   cmake --build build/release --config Release
#   ctest --test-dir build/release -L perf -C Release
#
# Benchmarks reuse the SDL-free static lib (dithr_core_test) so they
# do not pull in a graphics or audio device.

function(dtr_add_perf_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(${name} PRIVATE dithr_core_test)
    set_target_properties(${name} PROPERTIES FOLDER "tests/perf")
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        LABELS "perf"
        TIMEOUT 300
    )
    if (TARGET ${name})
        dtr_copy_test_dlls(${name})
    endif()
endfunction()

dtr_add_perf_test(perf_blit       perf_blit.c)
dtr_add_perf_test(perf_primitives perf_primitives.c)
