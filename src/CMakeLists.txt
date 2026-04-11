# --- Engine static library (everything except main.c) ---
# Shared source list and compile definitions for engine libraries.
set(DITHR_CORE_SOURCES
    console.c
    runtime.c
    graphics.c
    audio.c
    input.c
    mouse.c
    gamepad.c
    event.c
    cart.c
    cart_import.c
    postfx.c
    api/api_register.c
    api/gfx_api.c
    api/map_api.c
    api/sfx_api.c
    api/mus_api.c
    api/key_api.c
    api/mouse_api.c
    api/pad_api.c
    api/input_api.c
    api/evt_api.c
    api/postfx_api.c
    api/math_api.c
    api/cart_api.c
    api/sys_api.c
    api/col_api.c
    api/cam_api.c
    api/ui_api.c
    api/tween_api.c
    api/synth_api.c
    synth.c
    touch.c
    ui.c
    tween.c
    api/touch_api.c
)

# Prefix sources with current dir for out-of-tree references
list(TRANSFORM DITHR_CORE_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

# --- Public library (used by the executable and examples) ---
add_library(dithr_core STATIC ${DITHR_CORE_SOURCES})
target_include_directories(dithr_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(dithr_core PRIVATE ${DTR_WARN_FLAGS})
if (DTR_ENABLE_SIMD AND DTR_SIMD_FLAGS)
    target_compile_options(dithr_core PRIVATE ${DTR_SIMD_FLAGS})
endif()
if (MSVC)
    target_compile_options(dithr_core PRIVATE /wd4244)
endif()
target_link_libraries(dithr_core PUBLIC
    SDL3::SDL3
    SDL3_mixer::SDL3_mixer
    SDL3_image::SDL3_image
    quickjs
)
if (NOT MSVC)
    target_link_libraries(dithr_core PUBLIC m)
endif()
target_compile_definitions(dithr_core PUBLIC ${DITHR_COMPILE_DEFS})

# --- Test-only engine library (same sources, SDL not in link interface) ---
# SDL is linked PRIVATE so it is available during compilation but not
# propagated to consumers.  The linker only pulls object files whose symbols
# are actually referenced, so test binaries that only exercise SDL-free code
# paths (e.g. graphics) won't load SDL shared libraries at runtime.
add_library(dithr_core_test STATIC EXCLUDE_FROM_ALL ${DITHR_CORE_SOURCES})
target_include_directories(dithr_core_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(dithr_core_test PRIVATE ${DTR_WARN_FLAGS})
if (DTR_ENABLE_SIMD AND DTR_SIMD_FLAGS)
    target_compile_options(dithr_core_test PRIVATE ${DTR_SIMD_FLAGS})
endif()
if (MSVC)
    target_compile_options(dithr_core_test PRIVATE /wd4244)
endif()
target_link_libraries(dithr_core_test PRIVATE
    SDL3::SDL3
    SDL3_mixer::SDL3_mixer
    SDL3_image::SDL3_image
    quickjs
)
if (NOT MSVC)
    target_link_libraries(dithr_core_test PUBLIC m)
endif()
target_compile_definitions(dithr_core_test PUBLIC ${DITHR_COMPILE_DEFS})

# Dev build flag
option(DTR_DEV_BUILD "Enable development features (hot reload, debug overlays)" OFF)
if (DTR_DEV_BUILD)
    target_compile_definitions(dithr_core PUBLIC DEV_BUILD=1)
endif()
