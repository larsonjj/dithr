cmake_minimum_required(VERSION 3.22.1)

project(mvngin
    VERSION 0.1.0
    DESCRIPTION "Game creation framework for building retro games"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Dependencies ---
include(FetchContent)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.2
    GIT_SHALLOW TRUE
)
# Pull and build SDL3 completely first
FetchContent_MakeAvailable(SDL3)

set(SDLMIXER_FLAC OFF)
set(SDLMIXER_GME OFF)
set(SDLMIXER_MOD OFF)
set(SDLMIXER_MIDI OFF)
set(SDLMIXER_OPUS OFF)
FetchContent_Declare(
    SDL3_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG release-3.2.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL YES
)
FetchContent_MakeAvailable(SDL3_mixer)

# --- Library ---
add_library(mvngin
    source/mvngin.c
)

target_include_directories(mvngin
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(mvngin
    PUBLIC
        SDL3::SDL3
        SDL3_mixer::SDL3_mixer
)

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
