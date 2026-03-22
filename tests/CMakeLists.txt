# Prevent SDL from redefining main() in test binaries.
# SDL3 renames main → SDL_main and injects its own entry point, which can
# hang in headless CI environments (macOS Cocoa/NSApplication, Linux/Wayland).
add_compile_definitions(SDL_MAIN_HANDLED)

# When using MSVC ASan, locate the runtime DLL so it can be copied beside
# test executables.  The DLL lives next to cl.exe.
if (MSVC AND DTR_ENABLE_ASAN)
    get_filename_component(_msvc_compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    set(_asan_dll "${_msvc_compiler_dir}/clang_rt.asan_dynamic-x86_64.dll")
    if (NOT EXISTS "${_asan_dll}")
        message(WARNING "ASan DLL not found at ${_asan_dll}")
        set(_asan_dll "")
    endif()
endif()

# Helper: copy runtime DLLs (and ASan DLL when applicable) beside a test target.
function(dtr_copy_test_dlls target)
    if (WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
        )
        if (MSVC AND DTR_ENABLE_ASAN AND _asan_dll)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_asan_dll}" $<TARGET_FILE_DIR:${target}>
            )
        endif()
    endif()
endfunction()

# Helper: create a test executable, apply common options, and register with CTest.
# Usage: dtr_add_test(<name> <source> [LINK_LIB <library>])
# Defaults to linking against dithr_core if LINK_LIB is not specified.
function(dtr_add_test name source)
    cmake_parse_arguments(ARG "" "LINK_LIB" "" ${ARGN})
    if (NOT ARG_LINK_LIB)
        set(ARG_LINK_LIB dithr_core)
    endif()

    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ${ARG_LINK_LIB})
    if (MSVC)
        target_compile_options(${name} PRIVATE /wd4244)
    endif()
    dtr_copy_test_dlls(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# --- Input tests ---
dtr_add_test(test_input test_input.c)

# --- Cart tests ---
dtr_add_test(test_cart test_cart.c)

# --- Graphics tests ---
dtr_add_test(test_graphics test_graphics.c LINK_LIB dithr_core_test)

# --- Event tests ---
dtr_add_test(test_event test_event.c)

# --- Mouse / Gamepad tests ---
dtr_add_test(test_mouse_gamepad test_mouse_gamepad.c)

# --- Runtime tests ---
dtr_add_test(test_runtime test_runtime.c)

# --- Audio tests ---
dtr_add_test(test_audio test_audio.c)

# --- PostFX tests ---
dtr_add_test(test_postfx test_postfx.c)

# --- CTest properties ---
set_tests_properties(
    test_input test_cart test_graphics test_event
    test_mouse_gamepad test_runtime test_audio test_postfx
    PROPERTIES
        TIMEOUT 30
        ENVIRONMENT "SDL_AUDIO_DRIVER=dummy;SDL_VIDEO_DRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_VIDEODRIVER=dummy"
)
