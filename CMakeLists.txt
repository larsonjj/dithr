cmake_minimum_required(VERSION 3.22.1)

project(mvngin
    VERSION 0.1.0
    DESCRIPTION "Fantasy console engine for building retro games"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings (applied only to our own targets, not dependencies)
if (MSVC)
    set(MVN_WARN_FLAGS /W4)
else()
    set(MVN_WARN_FLAGS -Wall -Wextra -Wpedantic)
endif()

# --- Console compile-time defaults (override via -DMVN_FB_WIDTH=128 etc.) ---
set(MVN_FB_WIDTH          320   CACHE STRING "Framebuffer width")
set(MVN_FB_HEIGHT         180   CACHE STRING "Framebuffer height")
set(MVN_PALETTE_SIZE      256   CACHE STRING "Palette size")
set(MVN_WINDOW_SCALE      3     CACHE STRING "Default window scale")
set(MVN_MAX_SPRITES       256   CACHE STRING "Max sprite tiles")
set(MVN_SPRITE_FLAGS      8     CACHE STRING "Flags per sprite")
set(MVN_MAX_MAPS          32    CACHE STRING "Max map slots")
set(MVN_MAX_MAP_LAYERS    8     CACHE STRING "Max layers per map")
set(MVN_MAX_MAP_OBJECTS   512   CACHE STRING "Max objects per map")
set(MVN_MAX_CHANNELS      16    CACHE STRING "Audio mixer channels")
set(MVN_AUDIO_FREQ        44100 CACHE STRING "Audio sample rate")
set(MVN_AUDIO_BUFFER      2048  CACHE STRING "Audio buffer size")
set(MVN_TARGET_FPS        60    CACHE STRING "Target framerate")
set(MVN_JS_HEAP_MB        64    CACHE STRING "JS heap size in MB")
set(MVN_JS_STACK_KB       512   CACHE STRING "JS stack size in KB")
set(MVN_VERSION_STR       "0.1.0" CACHE STRING "Engine version string")

# --- Dependencies ---
include(FetchContent)

# SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL3)

# SDL3_mixer
set(SDLMIXER_FLAC OFF)
set(SDLMIXER_GME  OFF)
set(SDLMIXER_MOD  OFF)
set(SDLMIXER_MIDI OFF)
set(SDLMIXER_OPUS OFF)
set(MPG123_NO_ASM  ON CACHE BOOL "Disable mpg123 asm to suppress yasm warning")
FetchContent_Declare(
    SDL3_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG release-3.2.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL YES
)
FetchContent_MakeAvailable(SDL3_mixer)

# QuickJS-NG (download source only, we build it ourselves)
FetchContent_Declare(
    quickjs
    GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
    GIT_TAG v0.12.1
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(quickjs)
if (NOT quickjs_POPULATED)
    FetchContent_MakeAvailable(quickjs)
endif()

# Build QuickJS as a static library from source
add_library(quickjs STATIC
    ${quickjs_SOURCE_DIR}/quickjs.c
    ${quickjs_SOURCE_DIR}/libunicode.c
    ${quickjs_SOURCE_DIR}/libregexp.c
    ${quickjs_SOURCE_DIR}/dtoa.c
)
target_include_directories(quickjs PUBLIC ${quickjs_SOURCE_DIR})
set_target_properties(quickjs PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
target_compile_definitions(quickjs PRIVATE
    QUICKJS_NG_BUILD
    _GNU_SOURCE
)
if (WIN32)
    target_compile_definitions(quickjs PRIVATE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0601
    )
endif()
# Suppress warnings in third-party code
if (MSVC)
    target_compile_options(quickjs PRIVATE /W0)
else()
    target_compile_options(quickjs PRIVATE -w)
endif()

# --- Engine static library (everything except main.c) ---
add_library(mvngin_core STATIC
    src/console.c
    src/runtime.c
    src/graphics.c
    src/audio.c
    src/input.c
    src/mouse.c
    src/gamepad.c
    src/event.c
    src/cart.c
    src/cart_import.c
    src/postfx.c
    src/api/api_register.c
    src/api/gfx_api.c
    src/api/map_api.c
    src/api/sfx_api.c
    src/api/mus_api.c
    src/api/key_api.c
    src/api/mouse_api.c
    src/api/pad_api.c
    src/api/input_api.c
    src/api/evt_api.c
    src/api/postfx_api.c
    src/api/math_api.c
    src/api/cart_api.c
    src/api/sys_api.c
)

target_include_directories(mvngin_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(mvngin_core PRIVATE ${MVN_WARN_FLAGS})

# Suppress C4244 from QuickJS inline functions in quickjs.h
if (MSVC)
    target_compile_options(mvngin_core PRIVATE /wd4244)
endif()

target_link_libraries(mvngin_core PUBLIC
    SDL3::SDL3
    SDL3_mixer::SDL3_mixer
    quickjs
)

if (NOT MSVC)
    target_link_libraries(mvngin_core PUBLIC m)
endif()

# Pass compile-time defaults as definitions
target_compile_definitions(mvngin_core PUBLIC
    CONSOLE_FB_WIDTH=${MVN_FB_WIDTH}
    CONSOLE_FB_HEIGHT=${MVN_FB_HEIGHT}
    CONSOLE_PALETTE_SIZE=${MVN_PALETTE_SIZE}
    CONSOLE_WINDOW_SCALE=${MVN_WINDOW_SCALE}
    CONSOLE_MAX_SPRITES=${MVN_MAX_SPRITES}
    CONSOLE_SPRITE_FLAGS=${MVN_SPRITE_FLAGS}
    CONSOLE_MAX_MAPS=${MVN_MAX_MAPS}
    CONSOLE_MAX_MAP_LAYERS=${MVN_MAX_MAP_LAYERS}
    CONSOLE_MAX_MAP_OBJECTS=${MVN_MAX_MAP_OBJECTS}
    CONSOLE_MAX_CHANNELS=${MVN_MAX_CHANNELS}
    CONSOLE_AUDIO_FREQ=${MVN_AUDIO_FREQ}
    CONSOLE_AUDIO_BUFFER=${MVN_AUDIO_BUFFER}
    CONSOLE_TARGET_FPS=${MVN_TARGET_FPS}
    CONSOLE_JS_HEAP_MB=${MVN_JS_HEAP_MB}
    CONSOLE_JS_STACK_KB=${MVN_JS_STACK_KB}
    CONSOLE_VERSION="${MVN_VERSION_STR}"
)

# Dev build flag
option(MVN_DEV_BUILD "Enable development features (hot reload, debug overlays)" OFF)
if (MVN_DEV_BUILD)
    target_compile_definitions(mvngin_core PUBLIC DEV_BUILD=1)
endif()

# --- Executable ---
add_executable(mvngin src/main.c)
target_compile_definitions(mvngin PRIVATE SDL_MAIN_USE_CALLBACKS=1)
target_link_libraries(mvngin PRIVATE mvngin_core)

# Copy shared library DLLs next to the executable
if (WIN32)
    add_custom_command(TARGET mvngin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:mvngin> $<TARGET_FILE_DIR:mvngin>
        COMMAND_EXPAND_LISTS
    )
endif()

# --- Examples ---
option(MVN_BUILD_EXAMPLES "Build example programs" ON)
if (MVN_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# --- Tests ---
option(MVN_BUILD_TESTS "Build tests" ON)
if (MVN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
