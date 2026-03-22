# Prevent SDL from redefining main() in test binaries.
# On macOS, SDL3 renames main → SDL_main and injects its own entry point
# (Cocoa/NSApplication setup) which can hang in headless CI environments.
if (APPLE)
    add_compile_definitions(SDL_MAIN_HANDLED)
endif()

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

# --- Input tests ---
add_executable(test_input test_input.c)
target_link_libraries(test_input PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_input PRIVATE /wd4244)
endif()
dtr_copy_test_dlls(test_input)
add_test(NAME test_input COMMAND test_input)

# --- Cart tests ---
add_executable(test_cart test_cart.c)
target_link_libraries(test_cart PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_cart PRIVATE /wd4244)
endif()
dtr_copy_test_dlls(test_cart)
add_test(NAME test_cart COMMAND test_cart)

# --- Graphics tests ---
add_executable(test_graphics test_graphics.c)
target_link_libraries(test_graphics PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_graphics PRIVATE /wd4244)
endif()
dtr_copy_test_dlls(test_graphics)
add_test(NAME test_graphics COMMAND test_graphics)

# --- Event tests ---
add_executable(test_event test_event.c)
target_link_libraries(test_event PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_event PRIVATE /wd4244)
endif()
dtr_copy_test_dlls(test_event)
add_test(NAME test_event COMMAND test_event)

# --- Mouse / Gamepad tests ---
add_executable(test_mouse_gamepad test_mouse_gamepad.c)
target_link_libraries(test_mouse_gamepad PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_mouse_gamepad PRIVATE /wd4244)
endif()
dtr_copy_test_dlls(test_mouse_gamepad)
add_test(NAME test_mouse_gamepad COMMAND test_mouse_gamepad)

# --- Runtime tests ---
add_executable(test_runtime test_runtime.c)
target_link_libraries(test_runtime PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_runtime PRIVATE /wd4244)
endif()
dtr_copy_test_dlls(test_runtime)
add_test(NAME test_runtime COMMAND test_runtime)

# --- Audio tests ---
add_executable(test_audio test_audio.c)
target_link_libraries(test_audio PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_audio PRIVATE /wd4244)
endif()
dtr_copy_test_dlls(test_audio)
add_test(NAME test_audio COMMAND test_audio)

# --- PostFX tests ---
add_executable(test_postfx test_postfx.c)
target_link_libraries(test_postfx PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_postfx PRIVATE /wd4244)
endif()
dtr_copy_test_dlls(test_postfx)
add_test(NAME test_postfx COMMAND test_postfx)
