# Prevent SDL from redefining main() in test binaries.
# SDL3 renames main → SDL_main and injects its own entry point, which can
# hang in headless CI environments (macOS Cocoa/NSApplication, Linux/Wayland).
add_compile_definitions(SDL_MAIN_HANDLED)

# When using MSVC ASan, locate the runtime DLL so it can be copied beside
# test executables.  The DLL lives next to cl.exe.
if (MSVC AND DTR_ENABLE_ASAN)
    get_filename_component(_msvc_compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|aarch64")
        set(_asan_rt_name "clang_rt.asan_dynamic-aarch64.dll")
    else()
        set(_asan_rt_name "clang_rt.asan_dynamic-x86_64.dll")
    endif()
    set(_asan_dll "${_msvc_compiler_dir}/${_asan_rt_name}")
    if (NOT EXISTS "${_asan_dll}")
        message(WARNING "ASan DLL not found at ${_asan_dll}")
        set(_asan_dll "")
    endif()
endif()

# Helper: copy runtime DLLs (and ASan DLL when applicable) beside a test target.
# This is a no-op for static builds (no shared DLLs to copy).
function(dtr_copy_test_dlls target)
    if (WIN32 AND TARGET SDL3-shared)
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
    if (EMSCRIPTEN)
        target_link_options(${name} PRIVATE
            -sENVIRONMENT=node
            -sEXIT_RUNTIME=1
            -sNODERAWFS=1
            -sALLOW_MEMORY_GROWTH=1
        )
    endif()
    dtr_copy_test_dlls(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# --- Input tests ---
dtr_add_test(test_input test_input.c)

# --- Cart tests (require QuickJS init not available under Emscripten) ---
if (NOT EMSCRIPTEN)
    dtr_add_test(test_cart test_cart.c)
endif()

# --- Graphics tests ---
dtr_add_test(test_graphics test_graphics.c LINK_LIB dithr_core_test)
dtr_add_test(test_graphics_ext test_graphics_ext.c LINK_LIB dithr_core_test)

# --- Event tests (require QuickJS init not available under Emscripten) ---
if (NOT EMSCRIPTEN)
    dtr_add_test(test_event test_event.c)
endif()

# --- Mouse / Gamepad tests ---
dtr_add_test(test_mouse_gamepad test_mouse_gamepad.c)

# --- Touch tests ---
dtr_add_test(test_touch test_touch.c)

# --- Runtime tests (require QuickJS + host file I/O) ---
if (NOT EMSCRIPTEN)
    dtr_add_test(test_runtime test_runtime.c)
endif()

# --- Module normalize tests ---
dtr_add_test(test_module_normalize test_module_normalize.c)

# --- Audio tests ---
dtr_add_test(test_audio test_audio.c)

# --- PostFX tests ---
dtr_add_test(test_postfx test_postfx.c)

# --- Cart import tests (require host file I/O) ---
if (NOT EMSCRIPTEN)
    dtr_add_test(test_cart_import test_cart_import.c)
endif()

# --- Collision tests ---
dtr_add_test(test_col test_col.c)

# --- Camera / text metric tests ---
dtr_add_test(test_cam test_cam.c LINK_LIB dithr_core_test)

# --- UI layout tests ---
dtr_add_test(test_ui test_ui.c LINK_LIB dithr_core_test)

# --- Map tests ---
dtr_add_test(test_map test_map.c)

# --- Synth tests ---
dtr_add_test(test_synth test_synth.c)

# --- Tween tests ---
dtr_add_test(test_tween test_tween.c LINK_LIB dithr_core_test)

# --- API bridge tests (require QuickJS, no graphics/audio) ---
if (NOT EMSCRIPTEN)
    dtr_add_test(test_api_bridge test_api_bridge.c)
endif()

# --- Console lifecycle tests (require SDL display subsystem) ---
if (NOT EMSCRIPTEN)
    dtr_add_test(test_console test_console.c)
endif()

# --- Editor JS tests (require QuickJS + host file paths) ---
if (NOT EMSCRIPTEN)
    dtr_add_test(test_editor_js test_editor_js.c)
    target_compile_definitions(test_editor_js PRIVATE
        JS_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/js"
    )
endif()

# --- CTest properties ---
set(DTR_TEST_TARGETS
    test_input test_graphics test_graphics_ext
    test_mouse_gamepad test_module_normalize test_audio test_postfx
    test_col test_cam test_ui test_map test_synth test_tween
)
if (NOT EMSCRIPTEN)
    list(APPEND DTR_TEST_TARGETS
        test_cart test_event test_runtime test_cart_import test_console
        test_api_bridge
    )
endif()
set_tests_properties(${DTR_TEST_TARGETS}
    PROPERTIES
        TIMEOUT 120
        ENVIRONMENT "SDL_AUDIO_DRIVER=dummy;SDL_VIDEO_DRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_VIDEODRIVER=dummy"
)
