cmake_minimum_required(VERSION 3.22.1)

project(dithr
    VERSION 0.1.0
    DESCRIPTION "Fantasy console engine for building retro games"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if (EMSCRIPTEN)
    set(CMAKE_C_EXTENSIONS ON)   # EM_ASM requires GNU extensions (-std=gnu11)
else()
    set(CMAKE_C_EXTENSIONS OFF)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings (applied only to our own targets, not dependencies)
if (MSVC)
    set(DTR_WARN_FLAGS /W4 /sdl)
else()
    set(DTR_WARN_FLAGS -Wall -Wextra -Wpedantic -Wshadow -Wformat=2)
endif()

# Sanitizers & coverage (must precede all target definitions)
option(DTR_ENABLE_ASAN  "Enable AddressSanitizer"           OFF)
option(DTR_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(DTR_ENABLE_COVERAGE "Enable code coverage (gcov)"    OFF)
if (DTR_ENABLE_ASAN)
    if (MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()
if (DTR_ENABLE_UBSAN AND NOT MSVC)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()
if (DTR_ENABLE_COVERAGE AND NOT MSVC)
    add_compile_options(--coverage -fno-inline)
    add_link_options(--coverage)
endif()

# Link-time optimisation (LTO / IPO)
option(DTR_ENABLE_LTO "Enable link-time optimisation" OFF)
if (DTR_ENABLE_LTO)
    if (EMSCRIPTEN)
        add_compile_options(-flto)
        add_link_options(-flto)
    else()
        include(CheckIPOSupported)
        check_ipo_supported(RESULT DTR_IPO_OK OUTPUT DTR_IPO_ERR)
        if (DTR_IPO_OK)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        else()
            message(WARNING "LTO not supported: ${DTR_IPO_ERR}")
        endif()
    endif()
endif()

# Profile-guided optimisation (PGO) — GCC / Clang only
option(DTR_ENABLE_PGO_GENERATE "Instrument build for PGO profile collection" OFF)
option(DTR_ENABLE_PGO_USE      "Use collected PGO profile data"              OFF)
set(DTR_PGO_PROFILE_DIR "" CACHE PATH "Directory containing PGO profile data")

if (DTR_ENABLE_PGO_GENERATE AND NOT MSVC)
    add_compile_options(-fprofile-generate)
    add_link_options(-fprofile-generate)
endif()
if (DTR_ENABLE_PGO_USE AND NOT MSVC)
    if (DTR_PGO_PROFILE_DIR)
        add_compile_options(-fprofile-use=${DTR_PGO_PROFILE_DIR} -fprofile-correction)
        add_link_options(-fprofile-use=${DTR_PGO_PROFILE_DIR})
    else()
        add_compile_options(-fprofile-use -fprofile-correction)
        add_link_options(-fprofile-use)
    endif()
endif()

# Fuzzing — libFuzzer (Clang only)
option(DTR_BUILD_FUZZ "Build fuzz harnesses" OFF)

# Native architecture tuning — GCC / Clang only.  Off by default so release
# artifacts stay portable; enable for local benchmarking to let the
# compiler use the host CPU's full instruction set (AVX2/AVX-512, etc.).
option(DTR_NATIVE_ARCH "Enable -march=native (non-MSVC, non-Emscripten)" OFF)
if (DTR_NATIVE_ARCH AND NOT MSVC AND NOT EMSCRIPTEN)
    add_compile_options(-march=native)
endif()

# SIMD optimisation (SSE2 / NEON / wasm-simd128)
option(DTR_ENABLE_SIMD "Enable SIMD optimizations" ON)
if (DTR_ENABLE_SIMD)
    if (EMSCRIPTEN)
        set(DTR_SIMD_FLAGS -msimd128)
    elseif (MSVC)
        # MSVC: SSE2 is the default for x64; add /arch:SSE2 only for 32-bit
        if (CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(DTR_SIMD_FLAGS /arch:SSE2)
        else()
            set(DTR_SIMD_FLAGS "")
        endif()
    else()
        # GCC / Clang: x86 gets -msse2, AArch64 has NEON by default
        include(CheckCCompilerFlag)
        check_c_compiler_flag(-msse2 DTR_HAS_SSE2_FLAG)
        if (DTR_HAS_SSE2_FLAG)
            set(DTR_SIMD_FLAGS -msse2)
        else()
            set(DTR_SIMD_FLAGS "")
        endif()
    endif()
endif()

# --- Console compile-time defaults (override via -DDTR_FB_WIDTH=128 etc.) ---
set(DTR_FB_WIDTH          320   CACHE STRING "Framebuffer width")
set(DTR_FB_HEIGHT         180   CACHE STRING "Framebuffer height")
set(DTR_PALETTE_SIZE      256   CACHE STRING "Palette size")
set(DTR_WINDOW_SCALE      3     CACHE STRING "Default window scale")
set(DTR_MAX_SPRITES       1024  CACHE STRING "Max sprite tiles")
set(DTR_SPRITE_FLAGS      8     CACHE STRING "Flags per sprite")
set(DTR_MAX_MAPS          32    CACHE STRING "Max map slots")
set(DTR_MAX_MAP_LAYERS    8     CACHE STRING "Max layers per map")
set(DTR_MAX_MAP_OBJECTS   512   CACHE STRING "Max objects per map")
set(DTR_MAX_CHANNELS      16    CACHE STRING "Audio mixer channels")
set(DTR_AUDIO_FREQ        44100 CACHE STRING "Audio sample rate")
set(DTR_AUDIO_BUFFER      2048  CACHE STRING "Audio buffer size")
set(DTR_TARGET_FPS        60    CACHE STRING "Target framerate")
set(DTR_JS_HEAP_MB        64    CACHE STRING "JS heap size in MB")
set(DTR_JS_STACK_KB       512   CACHE STRING "JS stack size in KB")
set(DTR_VERSION_STR       "0.1.0" CACHE STRING "Engine version string")

# --- Dependencies ---
include(FetchContent)

# SDL3
set(SDL_HAPTIC  OFF)
set(SDL_POWER   OFF)
set(SDL_SENSOR  OFF)
set(SDL_CAMERA  OFF)
set(SDL_HIDAPI  OFF)
set(SDL_LOCALE  ON)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL3)

# SDL3_mixer
set(SDLMIXER_FLAC    OFF)
set(SDLMIXER_GME     OFF)
set(SDLMIXER_MOD     OFF)
set(SDLMIXER_MIDI    OFF)
set(SDLMIXER_OPUS    OFF)
set(SDLMIXER_WAVPACK OFF)
set(MPG123_NO_ASM  ON CACHE BOOL "Disable mpg123 asm to suppress yasm warning")
FetchContent_Declare(
    SDL3_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG release-3.2.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL YES
)
FetchContent_MakeAvailable(SDL3_mixer)

# SDL3_image
set(SDLIMAGE_AVIF OFF)
set(SDLIMAGE_BMP  ON)
set(SDLIMAGE_GIF  OFF)
set(SDLIMAGE_JPG  OFF)
set(SDLIMAGE_JXL  OFF)
set(SDLIMAGE_LBM  OFF)
set(SDLIMAGE_PCX  OFF)
set(SDLIMAGE_PNG  ON)
set(SDLIMAGE_PNG_LIBPNG OFF)
set(SDLIMAGE_PNM  OFF)
set(SDLIMAGE_QOI  OFF)
set(SDLIMAGE_SVG  OFF)
set(SDLIMAGE_TGA  OFF)
set(SDLIMAGE_TIF  OFF)
set(SDLIMAGE_WEBP OFF)
set(SDLIMAGE_XCF  OFF)
set(SDLIMAGE_XPM  OFF)
set(SDLIMAGE_XV   OFF)
set(SDLIMAGE_VENDORED OFF)
FetchContent_Declare(
    SDL3_image
    URL https://github.com/libsdl-org/SDL_image/releases/download/release-3.4.2/SDL3_image-3.4.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL3_image)

# QuickJS-NG (download source only, we build it ourselves)
FetchContent_Declare(
    quickjs
    GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
    GIT_TAG v0.14.0
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
target_compile_definitions(quickjs PRIVATE
    QUICKJS_NG_BUILD
    _GNU_SOURCE
)
if (WIN32 AND NOT EMSCRIPTEN)
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

# --- Compile definitions shared with engine libraries ---
set(DITHR_COMPILE_DEFS
    CONSOLE_FB_WIDTH=${DTR_FB_WIDTH}
    CONSOLE_FB_HEIGHT=${DTR_FB_HEIGHT}
    CONSOLE_PALETTE_SIZE=${DTR_PALETTE_SIZE}
    CONSOLE_WINDOW_SCALE=${DTR_WINDOW_SCALE}
    CONSOLE_MAX_SPRITES=${DTR_MAX_SPRITES}
    CONSOLE_SPRITE_FLAGS=${DTR_SPRITE_FLAGS}
    CONSOLE_MAX_MAPS=${DTR_MAX_MAPS}
    CONSOLE_MAX_MAP_LAYERS=${DTR_MAX_MAP_LAYERS}
    CONSOLE_MAX_MAP_OBJECTS=${DTR_MAX_MAP_OBJECTS}
    CONSOLE_MAX_CHANNELS=${DTR_MAX_CHANNELS}
    CONSOLE_AUDIO_FREQ=${DTR_AUDIO_FREQ}
    CONSOLE_AUDIO_BUFFER=${DTR_AUDIO_BUFFER}
    CONSOLE_TARGET_FPS=${DTR_TARGET_FPS}
    CONSOLE_JS_HEAP_MB=${DTR_JS_HEAP_MB}
    CONSOLE_JS_STACK_KB=${DTR_JS_STACK_KB}
    CONSOLE_VERSION="${DTR_VERSION_STR}"
)
if (NOT MSVC)
    list(APPEND DITHR_COMPILE_DEFS _DEFAULT_SOURCE)
endif()

# --- Engine static library (everything except main.c) ---
# Sources, libraries, and compile definitions are defined in src/CMakeLists.txt.
add_subdirectory(src)

# --- Executable ---
add_executable(dithr src/main.c)
target_compile_definitions(dithr PRIVATE SDL_MAIN_USE_CALLBACKS=1)
target_compile_options(dithr PRIVATE ${DTR_WARN_FLAGS})
target_link_libraries(dithr PRIVATE dithr_core)

# --- Emscripten / WASM ---
if (EMSCRIPTEN)
    # Cart directory to embed in the virtual filesystem
    set(DTR_WASM_CART_DIR "" CACHE PATH "Cart directory to preload for WASM builds")

    target_compile_options(dithr PRIVATE -Oz -flto)
    target_compile_options(dithr_core PRIVATE -Oz -flto)

    if (DTR_ENABLE_SIMD)
        target_link_options(dithr PRIVATE -msimd128)
    endif()

    # Cap WASM heap growth at 512 MiB so a runaway cart cannot exhaust the
    # browser tab's memory. Generous for fantasy-console workloads while
    # staying well below typical browser per-tab limits.
    target_link_options(dithr PRIVATE
        -sALLOW_MEMORY_GROWTH=1
        -sMAXIMUM_MEMORY=536870912
        -sSTACK_SIZE=1048576
        -sINITIAL_MEMORY=67108864
        -sENVIRONMENT=web
        "SHELL:-s EXPORTED_RUNTIME_METHODS=['FS']"
        -lidbfs.js
        -flto
        -Oz
    )

    # Preload cart assets into the Emscripten virtual FS
    if (DTR_WASM_CART_DIR)
        target_link_options(dithr PRIVATE
            "SHELL:--preload-file ${DTR_WASM_CART_DIR}@/cart"
        )
        target_compile_definitions(dithr PRIVATE DTR_WASM_CART_PATH="/cart/cart.json")

        # Track cart directory contents so Ninja relinks when assets change
        file(GLOB_RECURSE DTR_CART_FILES "${DTR_WASM_CART_DIR}/*")
        set_property(TARGET dithr PROPERTY LINK_DEPENDS ${DTR_CART_FILES})
    endif()

    # Produce .html output with custom shell page
    set_target_properties(dithr PROPERTIES SUFFIX ".html")
    target_link_options(dithr PRIVATE
        "--shell-file" "${CMAKE_SOURCE_DIR}/tools/templates/shell.html"
    )
endif()

# Copy shared library DLLs next to the executable (Windows + shared builds only)
if (WIN32 AND TARGET SDL3-shared)
    add_custom_command(TARGET dithr POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:dithr> $<TARGET_FILE_DIR:dithr>
        COMMAND_EXPAND_LISTS
    )
    if (MSVC AND DTR_ENABLE_ASAN)
        get_filename_component(_msvc_compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
        if (CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|aarch64")
            set(_asan_rt_name "clang_rt.asan_dynamic-aarch64.dll")
        else()
            set(_asan_rt_name "clang_rt.asan_dynamic-x86_64.dll")
        endif()
        set(_asan_dll "${_msvc_compiler_dir}/${_asan_rt_name}")
        if (EXISTS "${_asan_dll}")
            add_custom_command(TARGET dithr POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_asan_dll}" $<TARGET_FILE_DIR:dithr>
            )
        endif()
    endif()
endif()

# --- Install rules ---
include(GNUInstallDirs)

install(TARGETS dithr
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install runtime DLLs (SDL3, SDL3_mixer, SDL3_image, codec libs)
if (TARGET SDL3-shared)
    install(TARGETS SDL3-shared RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if (TARGET SDL3_mixer-shared)
    install(TARGETS SDL3_mixer-shared RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if (TARGET SDL3_image-shared)
    install(TARGETS SDL3_image-shared RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Install cart templates for new-project scaffolding
install(DIRECTORY tools/templates/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/dithr/templates
)

# --- Examples ---
option(DTR_BUILD_EXAMPLES "Build example programs" ON)
if (DTR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# --- Tests ---
option(DTR_BUILD_TESTS "Build tests" ON)
if (DTR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Fuzz harnesses ---
if (DTR_BUILD_FUZZ)
    add_subdirectory(tests/fuzz)
endif()
