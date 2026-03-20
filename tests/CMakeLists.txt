# --- Input tests ---
add_executable(test_input test_input.c)
target_link_libraries(test_input PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_input PRIVATE /wd4244)
endif()
if (WIN32)
    add_custom_command(TARGET test_input POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:test_input> $<TARGET_FILE_DIR:test_input>
        COMMAND_EXPAND_LISTS
    )
endif()
add_test(NAME test_input COMMAND test_input)
set_tests_properties(test_input PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

# --- Cart tests ---
add_executable(test_cart test_cart.c)
target_link_libraries(test_cart PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_cart PRIVATE /wd4244)
endif()
if (WIN32)
    add_custom_command(TARGET test_cart POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:test_cart> $<TARGET_FILE_DIR:test_cart>
        COMMAND_EXPAND_LISTS
    )
endif()
add_test(NAME test_cart COMMAND test_cart)
set_tests_properties(test_cart PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

# --- Graphics tests ---
add_executable(test_graphics test_graphics.c)
target_link_libraries(test_graphics PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_graphics PRIVATE /wd4244)
endif()
if (WIN32)
    add_custom_command(TARGET test_graphics POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:test_graphics> $<TARGET_FILE_DIR:test_graphics>
        COMMAND_EXPAND_LISTS
    )
endif()
add_test(NAME test_graphics COMMAND test_graphics)
set_tests_properties(test_graphics PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

# --- Event tests ---
add_executable(test_event test_event.c)
target_link_libraries(test_event PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_event PRIVATE /wd4244)
endif()
if (WIN32)
    add_custom_command(TARGET test_event POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:test_event> $<TARGET_FILE_DIR:test_event>
        COMMAND_EXPAND_LISTS
    )
endif()
add_test(NAME test_event COMMAND test_event)
set_tests_properties(test_event PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

# --- Mouse / Gamepad tests ---
add_executable(test_mouse_gamepad test_mouse_gamepad.c)
target_link_libraries(test_mouse_gamepad PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_mouse_gamepad PRIVATE /wd4244)
endif()
if (WIN32)
    add_custom_command(TARGET test_mouse_gamepad POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:test_mouse_gamepad> $<TARGET_FILE_DIR:test_mouse_gamepad>
        COMMAND_EXPAND_LISTS
    )
endif()
add_test(NAME test_mouse_gamepad COMMAND test_mouse_gamepad)
set_tests_properties(test_mouse_gamepad PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

# --- Runtime tests ---
add_executable(test_runtime test_runtime.c)
target_link_libraries(test_runtime PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_runtime PRIVATE /wd4244)
endif()
if (WIN32)
    add_custom_command(TARGET test_runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:test_runtime> $<TARGET_FILE_DIR:test_runtime>
        COMMAND_EXPAND_LISTS
    )
endif()
add_test(NAME test_runtime COMMAND test_runtime)
set_tests_properties(test_runtime PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

# --- Audio tests ---
add_executable(test_audio test_audio.c)
target_link_libraries(test_audio PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_audio PRIVATE /wd4244)
endif()
if (WIN32)
    add_custom_command(TARGET test_audio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:test_audio> $<TARGET_FILE_DIR:test_audio>
        COMMAND_EXPAND_LISTS
    )
endif()
add_test(NAME test_audio COMMAND test_audio)
set_tests_properties(test_audio PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

# --- PostFX tests ---
add_executable(test_postfx test_postfx.c)
target_link_libraries(test_postfx PRIVATE dithr_core)
if (MSVC)
    target_compile_options(test_postfx PRIVATE /wd4244)
endif()
if (WIN32)
    add_custom_command(TARGET test_postfx POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:test_postfx> $<TARGET_FILE_DIR:test_postfx>
        COMMAND_EXPAND_LISTS
    )
endif()
add_test(NAME test_postfx COMMAND test_postfx)
set_tests_properties(test_postfx PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
